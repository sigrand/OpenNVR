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


#ifndef LIBMOMENT__RTMP_SERVICE__H__
#define LIBMOMENT__RTMP_SERVICE__H__


#include <libmary/libmary.h>
#include <moment/rtmp_video_service.h>


//#define MOMENT__RTMP_SERVICE__USE_IMMEDIATE_SENDER


namespace Moment {

using namespace M;

class RtmpService : public RtmpVideoService,
		    public DependentCodeReferenced
{
private:
    StateMutex mutex;

public:
    class ClientSessionInfo
    {
    public:
        IpAddress client_addr;
        Time creation_unixtime;
        Time last_send_unixtime;
        Time last_recv_unixtime;
        StRef<String> last_play_stream;
        StRef<String> last_publish_stream;

        ClientSessionInfo ()
            : creation_unixtime  (0),
              last_send_unixtime (0),
              last_recv_unixtime (0)
        {}
    };

private:
    class SessionList_name;

    class ClientSession : public Object,
			  public IntrusiveListElement<SessionList_name>
    {
    public:
	bool valid;

	mt_const ServerThreadContext *thread_ctx;

	mt_const WeakCodeRef weak_rtmp_service;
	mt_const RtmpService *unsafe_rtmp_service;

	TcpConnection tcp_conn;
#ifndef MOMENT__RTMP_SERVICE__USE_IMMEDIATE_SENDER
	DeferredConnectionSender conn_sender;
#else
	ImmediateConnectionSender conn_sender;
#endif
	ConnectionReceiver conn_receiver;
	RtmpConnection rtmp_conn;

	mt_mutex (RtmpService::mutex) PollGroup::PollableKey pollable_key;

        mt_mutex (mutex) ClientSessionInfo session_info;

	ClientSession ()
	    : thread_ctx    (NULL),
	      tcp_conn      (this /* coderef_container */),
	      conn_sender   (this /* coderef_container */),
	      conn_receiver (this /* coderef_container */),
	      rtmp_conn     (this /* coderef_container */)
	{}

	~ClientSession ();
    };

    typedef IntrusiveList<ClientSession, SessionList_name> SessionList;

    mt_const bool prechunking_enabled;

    mt_const ServerContext *server_ctx;
    mt_const PagePool *page_pool;
    mt_const Time send_delay_millisec;
    mt_const Time rtmp_ping_timeout_millisec;

    TcpServer tcp_server;

    mt_mutex (mutex) SessionList session_list;

    AtomicInt num_session_objects;
    Count num_valid_sessions;

    mt_mutex (mutex) void destroySession (ClientSession *session);

  // ____ Accept watchdog ____

    mt_const Time accept_watchdog_timeout_sec;

    mt_mutex (mutex) Time last_accept_time;

    static void acceptWatchdogTick (void *_self);

  // _________________________

    bool acceptOneConnection ();

  mt_iface (RtmpConnection::Backend)
    static RtmpConnection::Backend const rtmp_conn_backend;

    static void closeRtmpConn (void *_session);
  mt_iface_end

  mt_iface (TcpServer::Frontend)
    static TcpServer::Frontend const tcp_server_frontend;

    static void accepted (void *_self);
  mt_iface_end

public:
    mt_throws Result bind (IpAddress addr);

    mt_throws Result start ();

    void rtmpServiceLock   () { mutex.lock (); }
    void rtmpServiceUnlock () { mutex.unlock (); }


  // _________________________ Current client sessions _________________________

private:
    mt_mutex (mutex) void updateClientSessionsInfo ();

public:
    class SessionInfoIterator
    {
        friend class RtmpService;

    private:
        SessionList::iterator iter;

        SessionInfoIterator (RtmpService const &rtmp_service) : iter (rtmp_service.session_list) {}

    public:
        SessionInfoIterator () {}

        bool operator == (SessionInfoIterator const &iter) const { return this->iter == iter.iter; }
        bool operator != (SessionInfoIterator const &iter) const { return this->iter != iter.iter; }

        bool done () const { return iter.done(); }

        ClientSessionInfo* next ()
        {
            ClientSession * const session = iter.next ();
            return &session->session_info;
        }
    };

    struct SessionsInfo
    {
        Count num_session_objects;
        Count num_valid_sessions;
    };

    mt_mutex (mutex) SessionInfoIterator getClientSessionsInfo_unlocked (SessionsInfo *ret_info);

  // ___________________________________________________________________________


    mt_const mt_throws Result init (ServerContext * mt_nonnull server_ctx,
                                    PagePool      * mt_nonnull page_pool,
                                    Time           send_delay_millisec,
                                    Time           rtmp_ping_timeout_millisec,
                                    bool           prechunking_enabled,
                                    Time           accept_watchdog_timeout_sec);

     RtmpService (Object *coderef_container);
    ~RtmpService ();
};

}


#endif /* LIBMOMENT__RTMP_SERVICE__H__ */

