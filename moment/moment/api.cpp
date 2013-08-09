/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov
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


#include <libmary/types.h>
#include <cstdio>

#include <libmary/libmary.h>
#include <moment/libmoment.h>
#include <mconfig/mconfig.h>

#include <moment/api.h>


// This file deals with data structures with overlapping meaning from three
// different domains. A naming convention should be applied to help identifying
// which symbol refers to which domain. The domains:
//
//   1. C API - "External" domain.
//   2. Moment's internal C++ interfaces - "Internal" domain.
//   3. Helper types and data of this API binding - "API" domain.
//
// Naming convention:
//
//   * All internal API types have "Api_" prefix in their names.
//   * Variables of API types have "api_" prefix.
//   * Variables of external types have "ext_" prefix.
//   * Variables of internal types have "int_" prefix.


using namespace M;
using namespace Moment;


extern "C" {


// _________________________________ Messages __________________________________

// Duplicated in api_amf.cpp
struct MomentMessage {
    PagePool::PageListHead *page_list;
    Size msg_offset;
    Size msg_len;
    Size prechunk_size;

    PagePool::PageListArray *pl_array;
};

#if 0
void moment_array_get (MomentArray   *array,
		       size_t         offset,
		       unsigned char *buf,
		       size_t         len)
{
}
#endif

#if 0
// Unnecessary
void moment_array_set (MomentArray         *array,
		       size_t               offset,
		       unsigned char const *buf,
		       size_t               len)
{
}
#endif

//MomentArray* moment_message_get_array (MomentMessage *message);

void moment_message_get_data (MomentMessage * const message,
			      size_t          const offset,
			      unsigned char * const buf,
			      size_t          const len)
{
    message->pl_array->get (offset, Memory (buf, len));
}


// _______________________________ Stream events _______________________________

struct MomentStreamHandler
{
    MomentAudioMessageCallback audio_cb;
    void *audio_cb_data;

    MomentVideoMessageCallback video_cb;
    void *video_cb_data;

    MomentRtmpDataMessageCallback command_cb;
    void *command_cb_data;

    MomentStreamClosedCallback closed_cb;
    void *closed_cb_data;

    MomentNumWatchersChangedCallback num_watchers_cb;
    void *num_watchers_cb_data;
};

MomentStreamHandler* moment_stream_handler_new (void)
{
    MomentStreamHandler * const stream_handler = new (std::nothrow) MomentStreamHandler;
    assert (stream_handler);

    stream_handler->audio_cb = NULL;
    stream_handler->audio_cb_data = NULL;

    stream_handler->video_cb = NULL;
    stream_handler->video_cb_data = NULL;

    stream_handler->command_cb = NULL;
    stream_handler->command_cb_data = NULL;

    stream_handler->closed_cb = NULL;
    stream_handler->closed_cb_data = NULL;

    stream_handler->num_watchers_cb = NULL;
    stream_handler->num_watchers_cb_data = NULL;

    return stream_handler;
}

void moment_stream_handler_delete (MomentStreamHandler * const stream_handler)
{
    delete stream_handler;
}

void moment_stream_handler_set_audio_message (MomentStreamHandler        * const stream_handler,
					      MomentAudioMessageCallback   const cb,
					      void                       * const user_data)
{
    stream_handler->audio_cb = cb;
    stream_handler->audio_cb_data = user_data;
}

void moment_stream_handler_set_video_message (MomentStreamHandler        * const stream_handler,
					      MomentVideoMessageCallback   const cb,
					      void                       * const user_data)
{
    stream_handler->video_cb = cb;
    stream_handler->video_cb_data = user_data;
}

void moment_stream_handler_set_rtmp_command_message (MomentStreamHandler           * const stream_handler,
						     MomentRtmpDataMessageCallback   const cb,
						     void                          * const user_data)
{
    stream_handler->command_cb = cb;
    stream_handler->command_cb_data = user_data;
}

void moment_stream_handler_set_closed (MomentStreamHandler        * const stream_handler,
				       MomentStreamClosedCallback   const cb,
				       void                       * const user_data)
{
    stream_handler->closed_cb = cb;
    stream_handler->closed_cb_data = user_data;
}

void moment_stream_handler_set_num_watchers_changed (MomentStreamHandler              * const stream_handler,
                                                     MomentNumWatchersChangedCallback   const cb,
                                                     void                             * const user_data)
{
    stream_handler->num_watchers_cb = cb;
    stream_handler->num_watchers_cb_data = user_data;
}


// ___________________________ Video stream control ____________________________

MomentStream* moment_create_stream (void)
{
    Ref<VideoStream> video_stream = grab (new (std::nothrow) VideoStream);
    MomentStream * const stream = static_cast <MomentStream*> (video_stream);
    video_stream.setNoUnref ((VideoStream*) NULL);
    return stream;
}

MomentStream* moment_get_stream (char const      * const name_buf,
				 size_t            const name_len,
				 MomentStreamKey * const ret_stream_key,
				 unsigned          const create)
{
    MomentServer * const moment = MomentServer::getInstance ();

    MomentServer::VideoStreamKey video_stream_key;
    // TODO ret_video_stream_key in getVideoStream()?
    // ...not very nice.
    Ref<VideoStream> video_stream = moment->getVideoStream (ConstMemory (name_buf, name_len));
    if (!video_stream) {
	// FIXME There's a logical race condition here. It'd be better to get away without one.
	if (!create)
	    return NULL;

	video_stream = grab (new (std::nothrow) VideoStream);
	video_stream_key = moment->addVideoStream (video_stream, ConstMemory (name_buf, name_len));
    }

    if (ret_stream_key)
	*ret_stream_key = video_stream_key.getAsVoidPtr();

    MomentStream * const stream = static_cast <MomentStream*> (video_stream);
    video_stream.setNoUnref ((VideoStream*) NULL);
    return stream;
}

void moment_stream_ref (MomentStream * const stream)
{
    static_cast <VideoStream*> (stream)->ref ();
}

void moment_stream_unref (MomentStream * const stream)
{
    static_cast <VideoStream*> (stream)->unref ();
}

void moment_remove_stream (MomentStreamKey const stream_key)
{
    if (!stream_key)
	return;

    MomentServer * const moment = MomentServer::getInstance ();
    moment->removeVideoStream (MomentServer::VideoStreamKey::fromVoidPtr (stream_key));
}

namespace {
class Api_StreamHandler_Wrapper : public Referenced
{
public:
    // TODO Make use of cb_mutex
    Mutex cb_mutex;

    MomentStreamHandler stream_handler;
    GenericInformer::SubscriptionKey subscription_key;
};
}

static void stream_audioMessage (VideoStream::AudioMessage * const audio_msg,
				 void                      * const _stream_handler)
{
    MomentStreamHandler * const stream_handler = static_cast <MomentStreamHandler*> (_stream_handler);

    if (stream_handler->audio_cb)
	stream_handler->audio_cb (audio_msg, stream_handler->audio_cb_data);
}

static void stream_videoMessage (VideoStream::VideoMessage * const video_msg,
				 void                      * const _stream_handler)
{
    MomentStreamHandler * const stream_handler = static_cast <MomentStreamHandler*> (_stream_handler);

    if (stream_handler->video_cb)
	stream_handler->video_cb (video_msg, stream_handler->video_cb_data);
}

static void stream_rtmpCommandMessage (RtmpConnection       * const mt_nonnull conn,
				       VideoStream::Message * const mt_nonnull msg,
				       ConstMemory const    &method_name,
				       AmfDecoder           * const mt_nonnull amf_decoder,
				       void                 * const _stream_handler)
{
    MomentStreamHandler * const stream_handler = static_cast <MomentStreamHandler*> (_stream_handler);

// TODO
#if 0
    MomentMessage msg;

    msg.prechunk_size = msg_info->prechunk_size;

//    PagePool::PageListArray pl_array (/* TODO */);

    if (stream_handler->command_cb)
	stream_handler->command_cb (&msg, stream_handler->command_cb_data);
#endif
}

static void stream_closed (void * const _stream_handler)
{
    MomentStreamHandler * const stream_handler = static_cast <MomentStreamHandler*> (_stream_handler);

    if (stream_handler->closed_cb)
	stream_handler->closed_cb (stream_handler->closed_cb_data);
}

static void stream_num_watchers_changed (Count   const num_watchers,
                                         void  * const _stream_handler)
{
    MomentStreamHandler * const stream_handler = static_cast <MomentStreamHandler*> (_stream_handler);

    if (stream_handler->num_watchers_cb)
        stream_handler->num_watchers_cb (num_watchers, stream_handler->num_watchers_cb_data);
}

static VideoStream::EventHandler const stream_event_handler = {
    stream_audioMessage,
    stream_videoMessage,
    stream_rtmpCommandMessage,
    stream_closed,
    stream_num_watchers_changed
};

MomentStreamHandlerKey moment_stream_add_handler (MomentStream        * const stream,
						  MomentStreamHandler * const stream_handler)
{
    Ref<Api_StreamHandler_Wrapper> wrapper = grab (new (std::nothrow) Api_StreamHandler_Wrapper);
    wrapper->stream_handler = *stream_handler;
    // TODO Fix race condition with stream_closed() (What if the stream has just been closed?)
    wrapper->subscription_key =
	    static_cast <VideoStream*> (stream)->getEventInformer()->subscribe (
		    &stream_event_handler, &wrapper->stream_handler, wrapper /* ref_data */, NULL);
    return static_cast <void*> (wrapper);
}

void moment_stream_remove_handler (MomentStream           * const stream,
				   MomentClientHandlerKey   const stream_handler_key)
{
    Api_StreamHandler_Wrapper * const wrapper =
	    static_cast <Api_StreamHandler_Wrapper*> (stream_handler_key);
    static_cast <VideoStream*> (stream)->getEventInformer()->unsubscribe (wrapper->subscription_key);
    // TODO FIXME wrapper->unref() would be more logical, since wrapper is used as 'ref_data' for subscribe().
    // TODO FIXME What if a call is already in progress? Is it a race condition?
    delete wrapper;
}

void moment_stream_plus_one_watcher (MomentStream * const stream)
{
    static_cast <VideoStream*> (stream)->plusOneWatcher ();
}

void moment_stream_minus_one_watcher (MomentStream * const stream)
{
    static_cast <VideoStream*> (stream)->minusOneWatcher ();
}

void moment_stream_bind_to_stream (MomentStream * const stream,
                                   MomentStream * const bind_audio_stream,
                                   MomentStream * const bind_video_stream,
                                   int            const bind_audio,
                                   int            const bind_video)
{
    static_cast <VideoStream*> (stream)->bindToStream (static_cast <VideoStream*> (bind_audio_stream),
                                                       static_cast <VideoStream*> (bind_video_stream),
                                                       (bool) bind_audio /* bind_audio */,
                                                       (bool) bind_video /* bind_video */);
}


// _______________________________ Client events _______________________________

struct MomentClientHandler {
    MomentClientConnectedCallback connected_cb;
    void *connected_cb_data;

    MomentClientDisconnectedCallback disconnected_cb;
    void *disconnected_cb_data;

    MomentStartWatchingCallback start_watching_cb;
    void *start_watching_cb_data;

    MomentStartStreamingCallback start_streaming_cb;
    void *start_streaming_cb_data;

    MomentRtmpCommandMessageCallback rtmp_command_cb;
    void *rtmp_command_cb_data;
};

MomentClientHandler* moment_client_handler_new (void)
{
    MomentClientHandler * const client_handler = new (std::nothrow) MomentClientHandler;
    assert (client_handler);

    client_handler->connected_cb = NULL;
    client_handler->connected_cb_data = NULL;

    client_handler->disconnected_cb = NULL;
    client_handler->disconnected_cb_data = NULL;

    client_handler->start_watching_cb = NULL;
    client_handler->start_watching_cb_data = NULL;

    client_handler->start_streaming_cb = NULL;
    client_handler->start_streaming_cb_data = NULL;

    client_handler->rtmp_command_cb = NULL;
    client_handler->rtmp_command_cb_data = NULL;

    return client_handler;
}

void moment_client_handler_delete (MomentClientHandler * const client_handler)
{
    delete client_handler;
}

void moment_client_handler_set_connected (MomentClientHandler           * const client_handler,
					  MomentClientConnectedCallback   const cb,
					  void                          * const user_data)
{
    client_handler->connected_cb = cb;
    client_handler->connected_cb_data = user_data;
}

void moment_client_handler_set_disconnected (MomentClientHandler              * const client_handler,
					     MomentClientDisconnectedCallback   const cb,
					     void                             * const user_data)
{
    client_handler->disconnected_cb = cb;
    client_handler->disconnected_cb_data = user_data;
}

void moment_client_handler_set_start_watching (MomentClientHandler         * const client_handler,
					       MomentStartWatchingCallback   const cb,
					       void                        * const user_data)
{
    client_handler->start_watching_cb = cb;
    client_handler->start_watching_cb_data = user_data;
}

void moment_client_handler_set_start_streaming (MomentClientHandler          * const client_handler,
						MomentStartStreamingCallback   const cb,
						void                         * const user_data)
{
    client_handler->start_streaming_cb = cb;
    client_handler->start_streaming_cb_data = user_data;
}

void moment_client_handler_set_rtmp_command_message (MomentClientHandler              * const client_handler,
						     MomentRtmpCommandMessageCallback   const cb,
						     void                             * const user_data)
{
    client_handler->rtmp_command_cb = cb;
    client_handler->rtmp_command_cb_data = user_data;
}

namespace {
class Api_ClientHandler_Wrapper : public Referenced
{
public:
    MomentClientHandler ext_client_handler;
    MomentServer::ClientHandlerKey int_client_handler_key;
};
} // namespace {}

class MomentClientSession : public Object
{
public:
    Ref<MomentServer::ClientSession> int_client_session;
    Ref<Api_ClientHandler_Wrapper> api_client_handler_wrapper;
    void *client_cb_data;

    MomentClientSession ()
    {
//	logD_ (_func, "0x", fmt_hex, (UintPtr) this);
    }

    ~MomentClientSession ()
    {
//	logD_ (_func, "0x", fmt_hex, (UintPtr) this);
    }
};

void moment_client_session_ref (MomentClientSession * const api_client_session)
{
    api_client_session->ref ();
}

void moment_client_session_unref (MomentClientSession * const api_client_session)
{
    api_client_session->unref ();
}

void moment_client_session_disconnect (MomentClientSession * const api_client_session)
{
    MomentServer * const moment = MomentServer::getInstance ();
    moment->disconnect (api_client_session->int_client_session);
}

static void client_rtmpCommandMessage (RtmpConnection       * const mt_nonnull conn,
				       VideoStream::Message * const mt_nonnull int_msg,
				       ConstMemory            const method_name,
				       AmfDecoder           * const mt_nonnull amf_decoder,
				       void                 * const _api_client_session)
{
    MomentClientSession * const api_client_session = static_cast <MomentClientSession*> (_api_client_session);

    if (!api_client_session->api_client_handler_wrapper->ext_client_handler.rtmp_command_cb)
	return;

    MomentMessage ext_msg;

    ext_msg.page_list     = &int_msg->page_list;
    ext_msg.msg_len       = int_msg->msg_len;
    ext_msg.msg_offset    = int_msg->msg_offset;
    ext_msg.prechunk_size = int_msg->prechunk_size;

    PagePool *normalized_page_pool;
    PagePool::PageListHead normalized_pages;
    Size normalized_offset = 0;
    bool unref_normalized_pages;
    if (int_msg->prechunk_size == 0) {
	normalized_pages = int_msg->page_list;
        normalized_offset = int_msg->msg_offset;
	unref_normalized_pages = false;
    } else {
	unref_normalized_pages = true;

	RtmpConnection::normalizePrechunkedData (int_msg,
						 int_msg->page_pool,
						 &normalized_page_pool,
						 &normalized_pages,
						 &normalized_offset);
	// TODO This assertion should become unnecessary (support msg_offs
	//      in PagePool::PageListArray).
	assert (normalized_offset == 0);
    }

    PagePool::PageListArray pl_array (normalized_pages.first, int_msg->msg_len);
    ext_msg.pl_array = &pl_array;

    api_client_session->api_client_handler_wrapper->ext_client_handler.rtmp_command_cb (
	    &ext_msg,
	    api_client_session->client_cb_data,
	    api_client_session->api_client_handler_wrapper->ext_client_handler.rtmp_command_cb_data);

    if (unref_normalized_pages)
	normalized_page_pool->msgUnref (normalized_pages.first);
}

static void client_clientDisconnected (void * const _api_client_session)
{
    MomentClientSession * const api_client_session = static_cast <MomentClientSession*> (_api_client_session);

//    logD_ (_func, "api_client_session: 0x", fmt_hex, (UintPtr) api_client_session);
//    logD_ (_func, "api_client_session refcount before: ", api_client_session->getRefCount ());

    if (api_client_session->api_client_handler_wrapper->ext_client_handler.disconnected_cb) {
	api_client_session->api_client_handler_wrapper->ext_client_handler.disconnected_cb (
		api_client_session->client_cb_data,
		api_client_session->api_client_handler_wrapper->ext_client_handler.disconnected_cb_data);
    }

//    logD_ (_func, "api_client_session refcount after: ", api_client_session->getRefCount ());
    api_client_session->unref ();
}

static MomentServer::ClientSession::Events client_session_events = {
    client_rtmpCommandMessage,
    client_clientDisconnected
};

namespace {
class Client_StartWatchingCallback_Data
{
public:
    Cb<MomentServer::StartWatchingCallback> cb;
};
}

static void client_startWatchingCallback (MomentStream * const ext_stream,
                                          void         * const _data)
{
    Client_StartWatchingCallback_Data * const data = static_cast <Client_StartWatchingCallback_Data*> (_data);
    data->cb.call_ (static_cast <VideoStream*> (ext_stream));
    delete data;
}

static bool client_startWatching (ConstMemory        const stream_name,
                                  ConstMemory        const /* stream_name_with_params */,
                                  IpAddress          const client_addr,
                                  CbDesc<MomentServer::StartWatchingCallback> const &cb,
                                  Ref<VideoStream> * const mt_nonnull ret_video_stream,
                                  void             * const _api_client_session)
{
    *ret_video_stream = NULL;

    MomentClientSession * const api_client_session = static_cast <MomentClientSession*> (_api_client_session);

    if (api_client_session->api_client_handler_wrapper->ext_client_handler.start_watching_cb) {
        Client_StartWatchingCallback_Data * const data = new (std::nothrow) Client_StartWatchingCallback_Data;
        assert (data);
        data->cb = cb;

	MomentStream *ext_stream = NULL;
        bool const complete =
		(bool) api_client_session->api_client_handler_wrapper->ext_client_handler.start_watching_cb (
                               (char const *) stream_name.mem(),
                               stream_name.len(),
                               api_client_session->client_cb_data,
                               api_client_session->api_client_handler_wrapper->ext_client_handler.start_watching_cb_data,
                               client_startWatchingCallback,
                               data,
                               &ext_stream);
        if (!complete)
            return false;
        else
            delete data;

	*ret_video_stream = static_cast <VideoStream*> (ext_stream);
        return true;
    }

    *ret_video_stream = NULL;
    return true;
}

namespace {
class Client_StartStreamingCallback_Data
{
public:
    Cb<MomentServer::StartStreamingCallback> cb;
};
}

static void client_startStreamingCallback (MomentResult   const res,
                                           void         * const _data)
{
    Client_StartStreamingCallback_Data * const data = static_cast <Client_StartStreamingCallback_Data*> (_data);
    data->cb.call_ (res == MomentResult_Success ? Result::Success : Result::Failure);
    delete data;
}

static bool client_startStreaming (ConstMemory     const stream_name,
                                   IpAddress       const client_addr,
                                   VideoStream   * const mt_nonnull video_stream,
                                   RecordingMode   const rec_mode,
                                   CbDesc<MomentServer::StartStreamingCallback> const &cb,
                                   Result        * const mt_nonnull ret_res,
                                   void          * const _api_client_session)
{
    *ret_res = Result::Failure;

    MomentClientSession * const api_client_session = static_cast <MomentClientSession*> (_api_client_session);

    if (api_client_session->api_client_handler_wrapper->ext_client_handler.start_streaming_cb) {
        Client_StartStreamingCallback_Data * const data = new (std::nothrow) Client_StartStreamingCallback_Data;
        assert (data);
        data->cb = cb;

        MomentResult res = MomentResult_Failure;
        bool const complete =
                (bool) api_client_session->api_client_handler_wrapper->ext_client_handler.start_streaming_cb (
                               (char const *) stream_name.mem(),
                               stream_name.len(),
                               static_cast <MomentStream*> (video_stream),
                               (MomentRecordingMode) (Uint32) rec_mode,
                               api_client_session->client_cb_data,
                               api_client_session->api_client_handler_wrapper->ext_client_handler.start_streaming_cb_data,
                               client_startStreamingCallback,
                               data,
                               &res);
        if (!complete)
            return false;
        else
            delete data;

	*ret_res = (res == MomentResult_Success ? Result::Success : Result::Failure);
        return true;
    }

    *ret_res = Result::Failure;
    return true;
}

static MomentServer::ClientSession::Backend const client_session_backend = {
    client_startWatching,
    client_startStreaming
};

static void client_clientConnected (MomentServer::ClientSession * const int_client_session,
				    ConstMemory  app_name,
				    ConstMemory  full_app_name,
				    void        * const _api_client_handler_wrapper)
{
    Api_ClientHandler_Wrapper * const api_client_handler_wrapper =
	    static_cast <Api_ClientHandler_Wrapper*> (_api_client_handler_wrapper);
    MomentClientHandler * const ext_client_handler = &api_client_handler_wrapper->ext_client_handler;

    Ref<MomentClientSession> api_client_session = grab (new (std::nothrow) MomentClientSession);
    api_client_session->int_client_session = int_client_session;
    api_client_session->client_cb_data = NULL;
    api_client_session->api_client_handler_wrapper = api_client_handler_wrapper;

    if (ext_client_handler->connected_cb) {
	ext_client_handler->connected_cb (api_client_session,
					  (char const *) app_name.mem(),
					  app_name.len(),
					  (char const *) full_app_name.mem(),
					  full_app_name.len(),
					  &api_client_session->client_cb_data,
					  ext_client_handler->connected_cb_data);
    }

    if (!int_client_session->isConnected_subscribe (
		CbDesc<MomentServer::ClientSession::Events> (
			&client_session_events, api_client_session, api_client_session)))
    {
	if (ext_client_handler->disconnected_cb) {
	    ext_client_handler->disconnected_cb (api_client_session->client_cb_data,
						 ext_client_handler->disconnected_cb_data);
	}
    }

    int_client_session->setBackend (
	    CbDesc<MomentServer::ClientSession::Backend> (
		    &client_session_backend, api_client_session, api_client_session));

    api_client_session.setNoUnref ((MomentClientSession*) NULL);
}

static MomentServer::ClientHandler api_client_handler = {
    client_clientConnected
};

MomentClientHandlerKey moment_add_client_handler (MomentClientHandler * const ext_client_handler,
						  char const          * const prefix_buf,
						  size_t                const prefix_len)
{
    MomentServer * const moment = MomentServer::getInstance ();

    Ref<Api_ClientHandler_Wrapper> api_client_handler_wrapper = grab (new (std::nothrow) Api_ClientHandler_Wrapper);
    api_client_handler_wrapper->ext_client_handler = *ext_client_handler;
    api_client_handler_wrapper->int_client_handler_key =
	    moment->addClientHandler (
		    CbDesc<MomentServer::ClientHandler> (&api_client_handler,
							 api_client_handler_wrapper,
							 NULL /* coderef_container */,
							 api_client_handler_wrapper),
		    ConstMemory (prefix_buf, prefix_len));

    return static_cast <MomentClientHandlerKey> (api_client_handler_wrapper);
}

void moment_remove_client_handler (MomentClientHandlerKey const ext_client_handler_key)
{
    Api_ClientHandler_Wrapper * const api_client_handler_wrapper =
	    static_cast <Api_ClientHandler_Wrapper*> (ext_client_handler_key);

    MomentServer * const moment = MomentServer::getInstance ();

    moment->removeClientHandler (api_client_handler_wrapper->int_client_handler_key);
    // TODO FIXME wrapper->unref() would be more logical, since wrapper is used as 'ref_data' for subscribe().
    // TODO FIXME What if a call is already in progress? Is it a race condition?
    delete api_client_handler_wrapper;
}

void moment_client_send_rtmp_command_message (MomentClientSession * const api_client_session,
					      unsigned char const * const msg_buf,
					      size_t                const msg_len)
{
    CodeDepRef<RtmpConnection> const conn = api_client_session->int_client_session->getRtmpConnection ();
    if (conn)
	conn->sendCommandMessage_AMF0 (RtmpConnection::CommandMessageStreamId, ConstMemory (msg_buf, msg_len));
}

void moment_client_send_rtmp_command_message_passthrough (MomentClientSession * const api_client_session,
							  MomentMessage       * const api_msg)
{
    CodeDepRef<RtmpConnection> const conn = api_client_session->int_client_session->getRtmpConnection ();
    if (conn) {
	conn->sendCommandMessage_AMF0_Pages (RtmpConnection::CommandMessageStreamId,
					     api_msg->page_list,
					     api_msg->msg_offset,
					     api_msg->msg_len,
					     api_msg->prechunk_size);
    }
}


// _____________________________ Config file access ____________________________

MomentConfigIterator moment_config_section_iter_begin (MomentConfigSection * const ext_section)
{
    MConfig::Section *int_section = static_cast <MConfig::Section*> (ext_section);
    if (!int_section) {
	MomentServer * const moment = MomentServer::getInstance ();
	int_section = moment->getConfig()->getRootSection();
    }

    return MConfig::Section::iter (*int_section).getAsVoidPtr();
}

MomentConfigSectionEntry* moment_confg_section_iter_next (MomentConfigSection  * const ext_section,
							  MomentConfigIterator   const ext_iter)
{
    MConfig::Section * const int_section = static_cast <MConfig::Section*> (ext_section);
    MConfig::Section::iter int_iter = MConfig::Section::iter::fromVoidPtr (ext_iter);
    return static_cast <MomentConfigSectionEntry*> (int_section->iter_next (int_iter));
}

MomentConfigSection* moment_config_section_entry_is_section (MomentConfigSectionEntry * const ext_section_entry)
{
    MConfig::SectionEntry * const int_section_entry = static_cast <MConfig::SectionEntry*> (ext_section_entry);
    if (int_section_entry->getType() == MConfig::SectionEntry::Type_Section)
	return static_cast <MomentConfigSection*> (static_cast <MConfig::Section*> (int_section_entry));

    return NULL;
}

MomentConfigOption* moment_config_section_entry_is_option (MomentConfigSectionEntry * const ext_section_entry)
{
    MConfig::SectionEntry * const int_section_entry = static_cast <MConfig::SectionEntry*> (ext_section_entry);
    if (int_section_entry->getType() == MConfig::SectionEntry::Type_Option)
	return static_cast <MomentConfigOption*> (static_cast <MConfig::Option*> (int_section_entry));

    return NULL;
}

size_t moment_config_option_get_value (MomentConfigOption * const ext_option,
				       char               * const buf,
				       size_t               const len)
{
    MConfig::Option * const int_option = static_cast <MConfig::Option*> (ext_option);
    MConfig::Value * const val = int_option->getValue ();
    if (!val)
	return 0;

    Ref<String> const str = val->getAsString ();
    Size tocopy = str->mem().len();
    if (tocopy > len)
	tocopy = len;

    memcpy (buf, str->mem().mem(), tocopy);
    return str->mem().len();
}

size_t moment_config_get_option (char   * const opt_path,
				 size_t   const opt_path_len,
				 char   * const buf,
				 size_t   const len,
				 int    * const ret_is_set)
{
    MomentServer * const moment = MomentServer::getInstance ();
    MConfig::Option * const int_option =
	    moment->getConfig()->getOption (ConstMemory (opt_path, opt_path_len), false /* create */);
    if (!int_option) {
	if (ret_is_set)
	    *ret_is_set = 0;

	return 0;
    }

    if (ret_is_set)
	*ret_is_set = 1;

    return moment_config_option_get_value (static_cast <MomentConfigOption*> (int_option),
					   buf, len);
}


// __________________________________ Logging __________________________________

void moment_log (MomentLogLevel   const log_level,
		 char const     * const fmt,
		 ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (log_level, fmt, ap);
    va_end (ap);
}

void moment_vlog (MomentLogLevel   const log_level,
		  char const     * const fmt,
		  va_list                orig_ap)
{
    int n;
    int size = 256;

    char *p;
    va_list ap;

    while (1) {
	p = new (std::nothrow) char [size];
        assert (p);

	va_copy (ap, orig_ap);
	n = vsnprintf (p, size, fmt, ap);
	va_end (ap);

	if (n > -1 && n < size)
	    break;

	if (n > -1)
	    size = n + 1;
	else
	    size *= 2;

	delete p;
    }

    log_ (LogLevel ((LogLevel::Value) log_level), ConstMemory (p, n));
}

void moment_log_d (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (MomentLogLevel_D, fmt, ap);
    va_end (ap);
}

void moment_log_i (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (MomentLogLevel_I, fmt, ap);
    va_end (ap);
}

void moment_log_w (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (MomentLogLevel_W, fmt, ap);
    va_end (ap);
}

void moment_log_e (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (MomentLogLevel_E, fmt, ap);
    va_end (ap);
}

void moment_log_h (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (MomentLogLevel_H, fmt, ap);
    va_end (ap);
}

void moment_log_f (char const *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    moment_vlog (MomentLogLevel_F, fmt, ap);
    va_end (ap);
}

void moment_log_dump_stream_list ()
{
    MomentServer * const moment = MomentServer::getInstance ();
    moment->dumpStreamList ();
}


} // extern "C"

