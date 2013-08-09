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


#ifndef LIBMOMENT__RTMPT_SERVICE__H__
#define LIBMOMENT__RTMPT_SERVICE__H__


#include <libmary/libmary.h>

#include <moment/rtmp_connection.h>
#include <moment/rtmp_video_service.h>


namespace Moment {

using namespace M;

class RtmptService : public RtmpVideoService,
                     public DependentCodeReferenced
{
private:
    StateMutex mutex;

public:
    class RtmptSessionInfo
    {
    public:
        IpAddress last_client_addr;
        Time creation_unixtime;
        Time last_req_unixtime;
        StRef<String> last_play_stream;
        StRef<String> last_publish_stream;

        RtmptSessionInfo ()
            : creation_unixtime (0),
              last_req_unixtime (0)
        {
        }
    };

    class RtmptConnectionInfo
    {
    public:
        IpAddress client_addr;
        Time creation_unixtime;
        Time last_req_unixtime;

        RtmptConnectionInfo ()
            : creation_unixtime (0),
              last_req_unixtime (0)
        {
        }
    };

private:
    class RtmptSender : public Sender,
                        public DependentCodeReferenced
    {
	friend class RtmptService;

    private:
      mt_mutex (mutex)
      mt_begin
        // Note: The only way to do proper framedropping for RTMPT clients is
        // to monitor active senders' states. That looks too complex for now.

	MessageList nonflushed_msg_list;
	Size nonflushed_data_len;

	MessageList pending_msg_list;
	Size pending_data_len;

	bool close_after_flush;
      mt_end

	mt_mutex (mutex) void doFlush ();

    public:
      mt_iface (Sender)
	mt_async void sendMessage (Sender::MessageEntry * mt_nonnull msg_entry,
				   bool do_flush);
        mt_mutex (mutex) void sendMessage_unlocked (Sender::MessageEntry * mt_nonnull msg_entry,
                                                    bool do_flush);
	mt_async void flush ();
        mt_mutex (mutex) void flush_unlocked ();
	mt_async void closeAfterFlush ();
        mt_async void close ();
        mt_mutex (mutex) bool isClosed_unlocked ();
        mt_mutex (mutex) SendState getSendState_unlocked ();
        void lock ();
        void unlock ();
      mt_iface_end

	mt_mutex (mutex) void sendPendingData (Sender * mt_nonnull sender);

	RtmptSender (Object *coderef_container);
	mt_async ~RtmptSender ();
    };

    class RtmptSession : public Object
    {
    public:
        mt_mutex (RtmptService::mutex) RtmptSessionInfo session_info;

	mt_mutex (RtmptService::mutex) bool valid;
        mt_mutex (RtmptService::mutex) bool closed;

        mt_const WeakDepRef<RtmptService> weak_rtmpt_service;

	mt_const Uint32 session_id;

	typedef Map< Ref<RtmptSession>,
		     MemberExtractor< RtmptSession,
				      Uint32,
				      &RtmptSession::session_id >,
		     DirectComparator<Uint32> >
		SessionMap_;

	mt_const SessionMap_::Entry session_map_entry;

	RtmptSender rtmpt_sender;

        // Synchronizes calls to rtmp_conn.doProcessInput()
        FastMutex rtmp_input_mutex;
	RtmpConnection rtmp_conn;

	mt_mutex (RtmptService::mutex) Time last_msg_time;
	mt_mutex (RtmptService::mutex) Timers::TimerKey session_keepalive_timer;

	RtmptSession  ()
            : rtmpt_sender (this /* coderef_container */),
              rtmp_conn    (this /* coderef_container */)
        {}

        ~RtmptSession ();
    };

    typedef RtmptSession::SessionMap_ SessionMap;

    class ConnectionList_name;

public:
    class RtmptConnection : public Object,
                            public IntrusiveListElement<ConnectionList_name>
    {
        friend class RtmptService;

    private:
        mt_mutex (RtmptService::mutex) RtmptConnectionInfo connection_info;

	mt_mutex (RtmptService::mutex) bool valid;

        mt_const WeakDepRef<RtmptService> weak_rtmpt_service;
        mt_const WeakDepRef<ServerThreadContext> weak_thread_ctx;

        TcpConnection tcp_conn;
	mt_mutex (RtmptService::mutex) PollGroup::PollableKey pollable_key;

	ImmediateConnectionSender conn_sender;
	ConnectionReceiver conn_receiver;
	HttpServer http_server;

	mt_sync_domain (RtmptService::http_frontend) Ref<RtmptSession> cur_req_session;

        mt_mutex (RtmptService::mutex) Time last_msg_time;
        mt_mutex (RtmptService::mutex) Timers::TimerKey conn_keepalive_timer;

	RtmptConnection ()
            : tcp_conn      (this /* coderef_container */),
              conn_sender   (this /* coderef_container */),
              conn_receiver (this /* coderef_container */),
              http_server   (this /* coderef_container */)
        {}

        ~RtmptConnection ();
    };

    typedef IntrusiveList<RtmptConnection, ConnectionList_name> ConnectionList;

private:
    mt_const bool prechunking_enabled;
    mt_const Time rtmp_ping_timeout_millisec;
    mt_const Time session_keepalive_timeout;
    mt_const Time conn_keepalive_timeout;
    mt_const bool no_keepalive_conns;

    mt_const DataDepRef<ServerContext> server_ctx;
    mt_const DataDepRef<PagePool> page_pool;

    TcpServer tcp_server;
    mt_mutex (mutex) PollGroup::PollableKey server_pollable_key;

    mt_mutex (mutex) ConnectionList conn_list;

    mt_mutex (mutex) SessionMap session_map;
    // TODO IdMapper
    mt_mutex (mutex) Uint32 session_id_counter;

    AtomicInt num_session_objects;
    Count num_valid_sessions;

    AtomicInt num_connection_objects;
    Count num_valid_connections;

    static void sessionKeepaliveTimerTick (void *_session);

    static void connKeepaliveTimerTick (void *_rtmpt_conn);

    mt_unlocks (mutex) void destroyRtmptSession (RtmptSession * mt_nonnull session,
                                                 bool          close_rtmp_conn);

    mt_mutex (mutex) void destroyRtmptConnection (RtmptConnection * mt_nonnull rtmpt_conn);

    mt_unlocks (mutex) void doConnectionClosed (RtmptConnection * mt_nonnull rtmpt_conn);

  mt_iface (RtmpConnection::Backend)
    static RtmpConnection::Backend const rtmp_conn_backend;

    static mt_async void rtmpClosed (void *_session);
  mt_iface_end

    void sendDataInReply (Sender       * mt_nonnull conn_sender,
                          RtmptSession * mt_nonnull session);

    void doOpen (Sender    * mt_nonnull conn_sender,
                 IpAddress  client_addr);

    Ref<RtmptSession> doSend (Sender          * mt_nonnull conn_sender,
                              Uint32           session_id,
                              RtmptConnection *rtmpt_conn);

    void doClose (Sender * mt_nonnull conn_sender,
                  Uint32  session_id);

    Ref<RtmptSession> doHttpRequest (HttpRequest     * mt_nonnull req,
                                     Sender          * mt_nonnull conn_sender,
                                     RtmptConnection *rtmpt_conn);

  mt_iface (HttpServer::Frontend)
    static HttpServer::Frontend const http_frontend;

    static mt_async void httpRequest (HttpRequest * mt_nonnull req,
				      void        *_rtmpt_conn);

    static mt_async void httpMessageBody (HttpRequest  * mt_nonnull req,
					  Memory        mem,
					  bool          end_of_request,
					  Size         * mt_nonnull ret_accepted,
					  void         *_rtmpt_conn);

    static mt_async void httpClosed (HttpRequest *req,
                                     Exception   *exc,
				     void        *_rtmpt_conn);
  mt_iface_end

  mt_iface (HttpService::HttpHandler)
    static HttpService::HttpHandler const http_handler;

    static Result service_httpRequest (HttpRequest   * mt_nonnull req,
				       Sender        * mt_nonnull conn_sender,
				       Memory const  &msg_body,
				       void         ** mt_nonnull ret_msg_data,
				       void          *_self);

    static Result service_httpMessageBody (HttpRequest  * mt_nonnull req,
					   Sender       * mt_nonnull conn_sender,
					   Memory const &mem,
					   bool          end_of_request,
					   Size         * mt_nonnull ret_accepted,
					   void         *msg_data,
					   void         *_self);
  mt_iface_end

    bool acceptOneConnection ();

  mt_iface (TcpServer::Frontend)
    static TcpServer::Frontend const tcp_server_frontend;

    static void accepted (void *_self);
  mt_iface_end

public:
    // mostly mt_const
    void attachToHttpService (HttpService *http_service,
			      ConstMemory  path = ConstMemory());

    mt_throws Result bind (IpAddress addr);

    mt_throws Result start ();

    void rtmptServiceLock   () { mutex.lock (); }
    void rtmptServiceUnlock () { mutex.unlock (); }


  // __________________________ Current RTMPT sessions _________________________

private:
    mt_mutex (mutex) void updateRtmptSessionsInfo ();

public:
    class RtmptSessionInfoIterator
    {
        friend class RtmptService;

    private:
        SessionMap::data_iterator iter;

        RtmptSessionInfoIterator (RtmptService &rtmpt_service) : iter (rtmpt_service.session_map) {}

    public:
        bool operator == (RtmptSessionInfoIterator const &iter) const { return this->iter == iter.iter; }
        bool operator != (RtmptSessionInfoIterator const &iter) const { return this->iter != iter.iter; }

        bool done () /* const */ { return iter.done(); }

        RtmptSessionInfo* next ()
        {
            RtmptSession * const session = iter.next ();
            return &session->session_info;
        }
    };

    struct RtmptSessionsInfo
    {
        Count num_session_objects;
        Count num_valid_sessions;
    };

    mt_mutex (mutex) RtmptSessionInfoIterator getRtmptSessionsInfo_unlocked (RtmptSessionsInfo *ret_info);


  // ________________________ Current RTMPT connections ________________________

private:
    mt_mutex (mutex) void updateRtmptConnectionsInfo ();

public:
    class RtmptConnectionInfoIterator
    {
        friend class RtmptService;

    private:
        ConnectionList::iterator iter;

        RtmptConnectionInfoIterator (RtmptService &rtmpt_service) : iter (rtmpt_service.conn_list) {}

    public:
        bool operator == (RtmptConnectionInfoIterator const &iter) const { return this->iter == iter.iter; }
        bool operator != (RtmptConnectionInfoIterator const &iter) const { return this->iter != iter.iter; }

        bool done () const { return iter.done(); }

        RtmptConnectionInfo* next ()
        {
            RtmptConnection * const rtmpt_conn = iter.next ();
            return &rtmpt_conn->connection_info;
        }
    };

    struct RtmptConnectionsInfo
    {
        Count num_connection_objects;
        Count num_valid_connections;
    };

    mt_mutex (mutex) RtmptConnectionInfoIterator getRtmptConnectionsInfo_unlocked (RtmptConnectionsInfo *ret_info);

  // ___________________________________________________________________________


    mt_const Result init (ServerContext * mt_nonnull server_ctx,
                          PagePool      * mt_nonnull page_pool,
                          bool           enable_standalone_tcp_server,
                          Time           rtmp_ping_timeout_millisec,
                          Time           session_keepalive_timeout,
                          Time           conn_keepalive_timeout,
                          bool           no_keepalive_conns,
                          bool           prechunking_enabled);

    RtmptService (Object *coderef_container);

    ~RtmptService ();
};

}


#endif /* LIBMOMENT__RTMPT_SERVICE__H__ */

