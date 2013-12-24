/*  Moment Video Server - High performance media server
    Copyright (C) 2011-2013 Dmitry Shatrov
    e-mail: shatrov@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef MOMENT__SERVER__H__
#define MOMENT__SERVER__H__


#include <libmary/libmary.h>
#include <mconfig/mconfig.h>


#include <moment/moment_types.h>
#include <moment/rtmp_connection.h>
#include <moment/video_stream.h>
#include <moment/storage.h>
#include <moment/push_protocol.h>
#include <moment/fetch_protocol.h>
#include <moment/transcoder.h>
#include <moment/media_source_provider.h>

#include <moment/moment_request_handler.h>


#define MOMENT_SERVER__HEADERS_DATE \
	Byte date_buf [unixtimeToString_BufSize]; \
	Size const date_len = unixtimeToString (Memory::forObject (date_buf), getUnixtime());

#define MOMENT_SERVER__COMMON_HEADERS \
	"Server: Moment/1.0\r\n" \
	"Date: ", ConstMemory (date_buf, date_len), "\r\n" \
    "Connection: Keep-Alive\r\n" \
    "Access-Control-Allow-Origin: *\r\n" \
    "Access-Control-Allow-Methods: POST, GET\r\n" \
    "Access-Control-Allow-Headers: Origin, Content-Type, Accept\r\n"

#define MOMENT_SERVER__OK_HEADERS(mime_type, content_length) \
	"HTTP/1.1 200 OK\r\n" \
	MOMENT_SERVER__COMMON_HEADERS \
	"Content-Type: ", (mime_type), "\r\n" \
	"Content-Length: ", (content_length), "\r\n" \
	"Cache-Control: no-cache\r\n"

#define MOMENT_SERVER__400_HEADERS(content_length) \
	"HTTP/1.1 400 Bad Request\r\n" \
	MOMENT_SERVER__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_SERVER__404_HEADERS(content_length) \
	"HTTP/1.1 404 Not Found\r\n" \
	MOMENT_SERVER__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_SERVER__500_HEADERS(content_length) \
	"HTTP/1.1 500 Internal Server Error\r\n" \
	MOMENT_SERVER__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"


namespace Moment {

using namespace M;

class ChannelManager;

// Only one MomentServer object may be initialized during program's lifetime.
// This limitation comes form loadable modules support.
//
class MomentServer : public Object
{
public: /* private: */
    StateMutex mutex;

private:
    class VideoStreamEntry;
    class StreamList_name;
    typedef IntrusiveList< VideoStreamEntry, StreamList_name, DeleteAction<VideoStreamEntry> > StreamList;

    class StreamHashEntry : public StReferenced
    {
    public:
        StreamList stream_list;
    };

    typedef StringHash< StRef<StreamHashEntry> > VideoStreamHash;

    class VideoStreamEntry : public IntrusiveListElement< StreamList_name >
    {
    public:
	Ref<VideoStream> video_stream;
        VideoStreamHash::EntryKey entry_key;
        bool displaced;

	VideoStreamEntry (VideoStream * const video_stream)
	    : video_stream (video_stream),
              displaced (false)
	{}
    };

    class RestreamInfo;

    class StreamInfo : public Referenced
    {
    public:
        mt_mutex (mutex) Ref<RestreamInfo> restream_info;
    };

public:
    class VideoStreamKey
    {
        friend class MomentServer;
    private:
        VideoStreamEntry *stream_entry;
        VideoStreamKey (VideoStreamEntry * const stream_entry) : stream_entry (stream_entry) {}
    public:
        operator bool () const { return stream_entry; }
        VideoStreamKey () : stream_entry (NULL) {}

        // Methods for C API binding.
        void *getAsVoidPtr () const { return stream_entry; }
        static VideoStreamKey fromVoidPtr (void *ptr)
            { return VideoStreamKey (static_cast <VideoStreamEntry*> (ptr)); }
    };


  // _________________________________ Events __________________________________

public:
    struct Events
    {
        void (*configReload) (MConfig::Config *new_config,
                              void            *cb_data);

        void (*destroy) (void *cb_data);
    };

private:
    Informer_<Events> event_informer;

    static void informConfigReload (Events * const events,
                                    void   * const cb_data,
                                    void   * const _new_config)
    {
        if (events->configReload) {
            MConfig::Config * const new_config = static_cast <MConfig::Config*> (_new_config);
            events->configReload (new_config, cb_data);
        }
    }

    void fireConfigReload (MConfig::Config * const new_config)
    {
        event_informer.informAll (informConfigReload, new_config);
    }

    static void informDestroy (Events * const events,
                               void   * const cb_data,
                               void   * const /* inform_data */)
    {
        if (events->destroy)
            events->destroy (cb_data);
    }

    void fireDestroy ()
    {
        event_informer.informAll (informDestroy, NULL /* inform_data */);
    }

public:
    Informer_<Events>* getEventInformer () { return &event_informer; }


  // ___________________________ Transcoder backend ____________________________

public:
    struct TranscoderBackend
    {
        Ref<Transcoder> (*newTranscoder) (void *cb_data);
    };

private:
    mt_mutex (mutex) Cb<TranscoderBackend> transcoder_backend;

public:
    void setTranscoderBackend (CbDesc<TranscoderBackend> const &cb)
    {
        mutex.lock ();
        transcoder_backend = cb;
        mutex.unlock ();
    }

    Ref<Transcoder> newTranscoder ()
    {
        mutex.lock ();
        Cb<TranscoderBackend> const cb = transcoder_backend;
        mutex.unlock ();

        Ref<Transcoder> transcoder;
        if (cb)
            cb.call_ret (&transcoder, cb->newTranscoder);

        return transcoder;
    }


  // __________________________ HTTP request handlers __________________________

private:
    static HttpService::HttpHandler const admin_http_handler;

    static Result adminHttpRequest (HttpRequest   * mt_nonnull req,
                                    Sender        * mt_nonnull conn_sender,
                                    Memory const  &msg_body,
                                    void         ** mt_nonnull ret_msg_data,
                                    void          *_self);

    static HttpService::HttpHandler const server_http_handler;

    static Result serverHttpRequest (HttpRequest   * mt_nonnull req,
                                     Sender        * mt_nonnull conn_sender,
                                     Memory const  &msg_body,
                                     void         ** mt_nonnull ret_msg_data,
                                     void          *_self);

    static bool _adminHttpRequest (HTTPServerRequest &req, HTTPServerResponse &resp, void * _self);

    static bool _serverHttpRequest (HTTPServerRequest &req, HTTPServerResponse &resp, void * _self);

public:
    struct HttpRequestResult {
	enum Value {
	    Success,
	    NotFound
	};
	operator Value () const { return value; }
	HttpRequestResult (Value const value) : value (value) {}
	HttpRequestResult () {}
    private:
	Value value;
    };

    struct HttpRequestHandler {
        HttpRequestResult (*httpRequest) (HttpRequest  * mt_nonnull req,
                                          Sender       * mt_nonnull conn_sender,
                                          Memory        msg_body,
                                          void         *cb_data);
    };

private:
    class HttpHandlerEntryList_name;

    struct HttpHandlerEntry : public IntrusiveListElement< HttpHandlerEntryList_name >
    {
        Cb<HttpRequestHandler> cb;
    };

    typedef IntrusiveList< HttpHandlerEntry,
                           HttpHandlerEntryList_name,
                           DeleteAction<HttpHandlerEntry> >
            HttpHandlerEntryList;

    HttpHandlerEntryList  admin_http_handlers;
    HttpHandlerEntryList server_http_handlers;

public:
    void  addAdminRequestHandler (CbDesc<HttpRequestHandler> const &cb);
    void addServerRequestHandler (CbDesc<HttpRequestHandler> const &cb);


  // __________________________ Video stream handlers __________________________

public:
    struct VideoStreamHandler
    {
        void (*videoStreamAdded) (VideoStream * mt_nonnull video_stream,
                                  ConstMemory  stream_name,
                                  void        *cb_data);
    };

    class VideoStreamHandlerKey
    {
        friend class MomentServer;

    private:
	GenericInformer::SubscriptionKey sbn_key;
    };

private:
    struct VideoStreamAddedNotification
    {
        Ref<VideoStream> video_stream;
        Ref<String> stream_name;
    };

    struct StreamClosedNotification
    {
        Ref<VideoStream> stream;
    };

    Informer_<VideoStreamHandler> video_stream_informer;

    DeferredProcessor::Task vs_added_inform_task;
    DeferredProcessor::Task vs_closed_inform_task;
    DeferredProcessor::Registration vs_inform_reg;

    mt_mutex (mutex) List<VideoStreamAddedNotification> vs_added_notifications;
    mt_mutex (mutex) List<StreamClosedNotification> vs_closed_notifications;

    static void informVideoStreamAdded (VideoStreamHandler *vs_handler,
                                        void               *cb_data,
                                        void               *inform_data);

#if 0
    void fireVideoStreamAdded (VideoStream * mt_nonnull video_stream,
                               ConstMemory  stream_name);
#endif

    mt_mutex (mutex) void notifyDeferred_VideoStreamAdded (VideoStream * mt_nonnull video_stream,
                                                           ConstMemory  stream_name);

    mt_mutex (mutex) void notifyDeferred_StreamClosed (VideoStream * mt_nonnull video_stream);

    static bool videoStreamAddedInformTask (void *_self);
    static bool streamClosedInformTask (void *_self);

public:
    VideoStreamHandlerKey addVideoStreamHandler (CbDesc<VideoStreamHandler> const &vs_handler);

    mt_mutex (mutex) VideoStreamHandlerKey addVideoStreamHandler_unlocked (CbDesc<VideoStreamHandler> const &vs_handler);

    void removeVideoStreamHandler (VideoStreamHandlerKey vs_handler_key);

    mt_mutex (mutex) void removeVideoStreamHandler_unlocked (VideoStreamHandlerKey vs_handler_key);

  // ___________________________________________________________________________


public:
    typedef void StartWatchingCallback (VideoStream *video_stream,
                                        void        *cb_data);

    typedef void StartStreamingCallback (Result  result,
                                         void   *cb_data);

    class AuthSession;

    class ClientSessionList_name;

    class ClientSession : public Object,
			  public IntrusiveListElement<ClientSessionList_name>
    {
	friend class MomentServer;

    private:
        StateMutex mutex;

    public:
	struct Events
	{
	    void (*rtmpCommandMessage) (RtmpConnection       * mt_nonnull rtmp_conn,
					VideoStream::Message * mt_nonnull msg,
					ConstMemory           method_name,
					AmfDecoder           * mt_nonnull amf_decoder,
					void                 *cb_data);

	    void (*clientDisconnected) (void *cb_data);
	};

	struct Backend
	{
	    bool (*startWatching) (ConstMemory       stream_name,
                                   ConstMemory       stream_name_with_params,
                                   IpAddress         client_addr,
                                   CbDesc<StartWatchingCallback> const &cb,
                                   Ref<VideoStream> * mt_nonnull ret_video_stream,
                                   void             *cb_data);

	    bool (*startStreaming) (ConstMemory    stream_name,
                                    IpAddress      client_addr,
                                    VideoStream   * mt_nonnull video_stream,
                                    RecordingMode  rec_mode,
                                    CbDesc<StartStreamingCallback> const &cb,
                                    Result        * mt_nonnull ret_res,
                                    void          *cb_data);
	};

    private:
	mt_mutex (mutex) bool processing_connected_event;
	mt_mutex (mutex) bool disconnected;

        mt_mutex (mutex) Ref<String> auth_key;
        mt_mutex (mutex) Ref<String> stream_name;

	mt_const WeakDepRef<RtmpConnection> weak_rtmp_conn;
	mt_const RtmpConnection *unsafe_rtmp_conn;

	mt_const IpAddress client_addr;

        mt_const Ref<AuthSession> auth_session;

	Informer_<Events> event_informer;
	Cb<Backend> backend;

	// TODO synchronization - ?
	VideoStreamKey video_stream_key;

	static void informRtmpCommandMessage (Events *events,
					      void   *cb_data,
					      void   *inform_data);

	static void informClientDisconnected (Events *events,
					      void   *cb_data,
					      void   *inform_data);

    public:
	Informer_<Events>* getEventInformer ()
	{
	    return &event_informer;
	}

	bool isConnected_subscribe (CbDesc<Events> const &cb);

	// Should be called by clientConnected handlers only.
	void setBackend (CbDesc<Backend> const &cb);

    private:
	void fireRtmpCommandMessage (RtmpConnection       * mt_nonnull conn,
				     VideoStream::Message * mt_nonnull msg,
				     ConstMemory           method_name,
				     AmfDecoder           * mt_nonnull amf_decoder);

	ClientSession ();

    public:
	CodeDepRef<RtmpConnection> getRtmpConnection () { return weak_rtmp_conn; }

	~ClientSession ();
    };

    typedef IntrusiveList<ClientSession, ClientSessionList_name> ClientSessionList;

    struct ClientHandler
    {
	void (*clientConnected) (ClientSession *client_session,
				 ConstMemory    app_name,
				 ConstMemory    full_app_name,
				 void          *cb_data);
    };

private:
    class Namespace;

//public:
    class ClientEntry : public Object
    {
	friend class MomentServer;

    private:
        StateMutex mutex;

	Namespace *parent_nsp;
	StringHash< Ref<ClientEntry> >::EntryKey client_entry_key;

	Informer_<ClientHandler> event_informer;

	static void informClientConnected (ClientHandler *client_handler,
					   void          *cb_data,
					   void          *inform_data);

	void fireClientConnected (ClientSession *client_session,
				  ConstMemory    app_name,
				  ConstMemory    full_app_name);

    public:
	Informer_<ClientHandler>* getEventInformer ()
	{
	    return &event_informer;
	}

	ClientEntry ()
	    : event_informer (this, &mutex)
	{
	}
    };


  // _____________________ Serving templated static pages ______________________

public:
    struct PageRequestResult {
	enum Value {
	    Success,
	    NotFound,
	    AccessDenied,
	    InternalError
	};
	operator Value () const { return value; }
	PageRequestResult (Value const value) : value (value) {}
	PageRequestResult () {}
    private:
	Value value;
    };

    struct PageRequest {
	// If ret.mem() == NULL, then the parameter is not set.
	// If ret.len() == 0, then the parameter has empty value.
	virtual ConstMemory getParameter     (ConstMemory name) = 0;
	virtual IpAddress   getClientAddress () = 0;
	virtual void        addHashVar       (ConstMemory name, ConstMemory value) = 0;
	virtual void        showSection      (ConstMemory name) = 0;
        virtual ~PageRequest () {}
    };

    struct PageRequestHandler {
	PageRequestResult (*pageRequest) (PageRequest  *req,
					  ConstMemory   path,
					  ConstMemory   full_path,
					  void         *cb_data);
    };

private:
    class PageRequestHandlerEntry : public Object
    {
	friend class MomentServer;

    private:
        StateMutex mutex;

	Informer_<PageRequestHandler> event_informer;

	typedef StringHash< Ref<PageRequestHandlerEntry> > PageRequestHandlerHash;
	mt_const PageRequestHandlerHash::EntryKey hash_key;

	mt_mutex (MomentServer::mutex) Count num_handlers;

	static void informPageRequest (PageRequestHandler *handler,
				       void               *cb_data,
				       void               *inform_data);

	PageRequestResult firePageRequest (PageRequest *page_req,
					   ConstMemory  path,
					   ConstMemory  full_path);

    public:
	Informer_<PageRequestHandler>* getEventInformer ()
            { return &event_informer; }

	PageRequestHandlerEntry ()
	    : event_informer (this, &mutex),
	      num_handlers (0)
	{}
    };

    typedef PageRequestHandlerEntry::PageRequestHandlerHash PageRequestHandlerHash;

    mt_mutex (mutex) PageRequestHandlerHash page_handler_hash;

public:
    struct PageRequestHandlerKey {
	PageRequestHandlerEntry *handler_entry;
	GenericInformer::SubscriptionKey sbn_key;
    };

    PageRequestHandlerKey addPageRequestHandler (CbDesc<PageRequestHandler> const &cb,
						 ConstMemory path);

    void removePageRequestHandler (PageRequestHandlerKey handler_key);

    PageRequestResult processPageRequest (PageRequest *page_req,
					  ConstMemory  path);

  // ___________________________________________________________________________


private:
    mt_const ServerApp        *server_app;
    mt_const PagePool         *page_pool;
    mt_const HttpService      *http_service;
    mt_const HttpService      *admin_http_service;
    mt_const ServerThreadPool *recorder_thread_pool;
    mt_const ServerThreadPool *reader_thread_pool;
    mt_const Storage          *storage;

    mt_const WeakRef<ChannelManager> weak_channel_manager;

    mt_const bool publish_all_streams;
    mt_const bool enable_restreaming;

    mt_mutex (mutex) ClientSessionList client_session_list;

    static MomentServer *instance;

    mt_const bool new_streams_on_top;
    mt_mutex (mutex) VideoStreamHash video_stream_hash;

    mt_const Ref<VideoStream> mix_video_stream;

    class Namespace : public BasicReferenced
    {
    public:
	// We use Ref<Namespace> because 'Namespace' is an incomplete type here
	// (language limitation).
	typedef StringHash< Ref<Namespace> > NamespaceHash;
	// We use Ref<ClientEntry> because of ClientEntry::event_informer.
	typedef StringHash< Ref<ClientEntry> > ClientEntryHash;

	Namespace *parent_nsp;
	NamespaceHash::EntryKey namespace_hash_key;

	NamespaceHash namespace_hash;
	ClientEntryHash client_entry_hash;

	Namespace ()
	    : parent_nsp (NULL)
	{
	}
    };

    mt_mutex (mutex) Namespace root_namespace;

    mt_mutex (mutex) ClientEntry* getClientEntry_rec (ConstMemory  path,
						      ConstMemory * mt_nonnull ret_path_tail,
						      Namespace   * mt_nonnull nsp);

    mt_mutex (mutex) ClientEntry* getClientEntry (ConstMemory  path,
						  ConstMemory * mt_nonnull ret_path_tail,
						  Namespace   * mt_nonnull nsp);

    mt_throws Result loadModules ();

public:
  // Getting pointers to common objects

    ServerApp*        getServerApp ();
    PagePool*         getPagePool ();
    HttpService*      getHttpService ();
    HttpService*      getAdminHttpService ();
    ServerThreadPool* getRecorderThreadPool ();
    ServerThreadPool* getReaderThreadPool ();
    Storage*          getStorage ();

    Ref<ChannelManager> getChannelManager () { return weak_channel_manager.getRef(); }

    static MomentServer* getInstance ();


  // _________________________________ Config __________________________________

public:
    class VarHashEntry : public HashEntry<>
    {
    public:
        MConfig::Varlist::Var *var;
    };

    typedef Hash< VarHashEntry,
                  ConstMemory,
                  MemberExtractor< VarHashEntry,
                                   MConfig::Varlist::Var*,
                                   &VarHashEntry::var,
                                   ConstMemory,
                                   AccessorExtractor< MConfig::Varlist::Var,
                                                      ConstMemory,
                                                      &MConfig::Varlist::Var::getName > >,
                  MemoryComparator<>,
                  DefaultStringHasher >
            VarHash;

private:
    Mutex config_mutex;

    mt_mutex (config_mutex) Ref<MConfig::Config>  config;
    mt_mutex (config_mutex) Ref<MConfig::Varlist> default_varlist;
    mt_mutex (config_mutex) VarHash               default_var_hash;

    mt_one_of(( mt_const, mt_mutex (config_mutex) )) void parseDefaultVarlist (MConfig::Config * mt_nonnull new_config);

    mt_mutex (config_mutex) void releaseDefaultVarHash ();

public:
    Ref<MConfig::Config>  getConfig ();
    Ref<MConfig::Varlist> getDefaultVarlist ();

    mt_mutex (config_mutex) MConfig::Config* getConfig_unlocked ();
    mt_mutex (config_mutex) VarHash* getDefaultVarHash_unlocked () { return &default_var_hash; }

    void setConfig (MConfig::Config * mt_nonnull new_config);

    void configLock ();

    void configUnlock ();

  // ___________________________________________________________________________


  // Client events

    Ref<ClientSession> rtmpClientConnected (ConstMemory const &path,
					    RtmpConnection    * mt_nonnull conn,
					    IpAddress   const &client_addr);

    void clientDisconnected (ClientSession * mt_nonnull client_session);

    void rtmpCommandMessage (ClientSession        * const mt_nonnull client_session,
			     RtmpConnection       * const mt_nonnull conn,
			     VideoStream::Message * const mt_nonnull msg,
			     ConstMemory const    &method_name,
			     AmfDecoder           * const mt_nonnull amf_decoder)
    {
	client_session->fireRtmpCommandMessage (conn, msg, method_name, amf_decoder);
    }

    void disconnect (ClientSession * mt_nonnull client_session);

    bool startWatching (ClientSession    * mt_nonnull client_session,
                        ConstMemory       stream_name,
                        ConstMemory       stream_name_with_params,
                        ConstMemory       auth_key,
                        CbDesc<StartWatchingCallback> const &cb,
                        Ref<VideoStream> * mt_nonnull ret_video_stream);

    bool startStreaming (ClientSession * mt_nonnull client_session,
                         ConstMemory    stream_name,
                         ConstMemory    auth_key,
                         VideoStream   * mt_nonnull video_stream,
                         RecordingMode  rec_mode,
                         CbDesc<StartStreamingCallback> const &cb,
                         Result        * mt_nonnull ret_res);

    void decStreamUseCount (VideoStream *stream);

    struct ClientHandlerKey
    {
	ClientEntry *client_entry;
	GenericInformer::SubscriptionKey sbn_key;
    };

private:
    mt_mutex (mutex) ClientHandlerKey addClientHandler_rec (CbDesc<ClientHandler> const &cb,
							    ConstMemory const           &path,
							    Namespace                   *nsp);

public:
    ClientHandlerKey addClientHandler (CbDesc<ClientHandler> const &cb,
				       ConstMemory           const &path);

    void removeClientHandler (ClientHandlerKey client_handler_key);

  // Get/add/remove video streams

    // TODO There's a logical problem here. A stream can only be deleted
    // by the one who created it. This limitation makes little sense.
    // But overcoming it requires more complex synchronization.
    Ref<VideoStream> getVideoStream (ConstMemory path);

    mt_mutex (mutex) Ref<VideoStream> getVideoStream_unlocked (ConstMemory path);

    VideoStreamKey addVideoStream (VideoStream *stream,
				   ConstMemory  path);

    mt_mutex (mutex) VideoStreamKey addVideoStream_unlocked (VideoStream *stream,
                                                             ConstMemory  path);

private:
    mt_mutex (mutex) void removeVideoStream_unlocked (VideoStreamKey video_stream_key);

public:
    void removeVideoStream (VideoStreamKey video_stream_key);

    Ref<VideoStream> getMixVideoStream ();


  // _______________________________ Restreaming _______________________________

private:
    class RestreamInfo : public Object
    {
    public:
        mt_const WeakRef<MomentServer> weak_moment;

        mt_mutex (mutex) VideoStreamKey stream_key;
        // Valid when 'stream_key' is non-null, which means that
        // video_stream_hash holds a reference to the stream.
        mt_mutex (mutex) VideoStream *unsafe_stream;

        mt_mutex (mutex) Ref<FetchConnection> fetch_conn;
    };

    static FetchConnection::Frontend const restream__fetch_conn_frontend;

    static void restreamFetchConnDisconnected (bool             * mt_nonnull ret_reconnect,
                                               Time             * mt_nonnull ret_reconnect_after_millisec,
                                               Ref<VideoStream> * mt_nonnull ret_new_stream,
                                               void             *_restream_info);

public:
    Ref<VideoStream> startRestreaming (ConstMemory stream_name,
                                       ConstMemory uri);

private:
    mt_unlocks (mutex) void stopRestreaming (RestreamInfo * mt_nonnull restream_info);


  // __________________________ Push/fetch protocols ___________________________

private:
    typedef StringHash< Ref<PushProtocol> >  PushProtocolHash;
    typedef StringHash< Ref<FetchProtocol> > FetchProtocolHash;

    mt_mutex (mutex) PushProtocolHash  push_protocol_hash;
    mt_mutex (mutex) FetchProtocolHash fetch_protocol_hash;

public:
    void addPushProtocol (ConstMemory   protocol_name,
                          PushProtocol * mt_nonnull push_protocol);

    void addFetchProtocol (ConstMemory    protocol_name,
                           FetchProtocol * mt_nonnull fetch_protocol);

    Ref<PushProtocol>  getPushProtocolForUri  (ConstMemory uri);
    Ref<FetchProtocol> getFetchProtocolForUri (ConstMemory uri);


  // _________________________ media source providers __________________________

private:
    mt_const DataDepRef<MediaSourceProvider> media_source_provider;

public:
    Ref<MediaSource> createMediaSource (CbDesc<MediaSource::Frontend> const &frontend,
                                        Timers            *timers,
                                        DeferredProcessor *deferred_processor,
                                        PagePool          *page_pool,
                                        VideoStream       *video_stream,
                                        VideoStream       *mix_video_stream,
                                        Time               initial_seek,
                                        ChannelOptions    *channel_opts,
                                        PlaybackItem      *playback_item);

    mt_const void setMediaSourceProvider (MediaSourceProvider * const media_source_provider)
        { this->media_source_provider = media_source_provider; }


  // ______________________________ Authorization ______________________________

public:
    class AuthSession : public virtual Referenced
    {
    };

    enum AuthAction
    {
        AuthAction_Watch,
        AuthAction_WatchRestream,
        AuthAction_Stream
    };

    typedef void CheckAuthorizationCallback (bool         authorized,
                                             ConstMemory  reply_str,
                                             void        *cb_data);

    struct AuthBackend
    {
        Ref<AuthSession> (*newAuthSession) (void *cb_data);

        bool (*checkAuthorization) (AuthSession   *auth_session,
                                    AuthAction     auth_action,
                                    ConstMemory    stream_name,
                                    ConstMemory    auth_key,
                                    IpAddress      client_addr,
                                    CbDesc<CheckAuthorizationCallback> const &cb,
                                    bool          * mt_nonnull ret_authorized,
                                    StRef<String> * mt_nonnull ret_reply_str,
                                    void          *cb_data);

        void (*authSessionDisconnected) (AuthSession *auth_session,
                                         ConstMemory  auth_key,
                                         IpAddress    client_addr,
                                         ConstMemory  stream_name,
                                         void        *cb_data);
    };

private:
    mt_mutex (mutex) Cb<AuthBackend> auth_backend;

public:
    void setAuthBackend (CbDesc<AuthBackend> const &auth_backend)
    {
        this->auth_backend = auth_backend;
    }

    bool checkAuthorization (AuthSession   *auth_session,
                             AuthAction     auth_action,
                             ConstMemory    stream_name,
                             ConstMemory    auth_key,
                             IpAddress      client_addr,
                             CbDesc<CheckAuthorizationCallback> const &cb,
                             bool          * mt_nonnull ret_authorized,
                             StRef<String> * mt_nonnull ret_reply_str);

  // ___________________________________________________________________________


    void dumpStreamList ();

    mt_locks (mutex) void lock ();

    mt_unlocks (mutex) void unlock ();

    Result init (ServerApp        * mt_nonnull server_app,
		 PagePool         * mt_nonnull page_pool,
		 HttpService      * mt_nonnull http_service,
		 HttpService      * mt_nonnull admin_http_service,
		 MConfig::Config  * mt_nonnull config,
		 ServerThreadPool * mt_nonnull recorder_thread_pool,
                 ServerThreadPool * mt_nonnull reader_thread_pool,
		 Storage          * mt_nonnull storage,
                 ChannelManager   *channel_manager);

    MomentServer ();

    ~MomentServer ();    


  // _________________________ Internal public methods _________________________

    Result setClientSessionVideoStream (ClientSession *client_session,
                                        VideoStream   *video_stream,
                                        ConstMemory    stream_name);

  // ___________________________________________________________________________

};

}


#include <moment/channel_manager.h>


#endif /* MOMENT__SERVER__H__ */

