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


#include <moment/rtmp_service.h>


using namespace M;

namespace Moment {

static LogGroup libMary_logGroup_rtmp_service ("rtmp_service", LogLevel::E);

RtmpConnection::Backend const RtmpService::rtmp_conn_backend = {
    closeRtmpConn
};

TcpServer::Frontend const RtmpService::tcp_server_frontend = {
    accepted
};

RtmpService::ClientSession::~ClientSession ()
{
//    logD_ (_func_);

    CodeRef const rtmp_service_ref = weak_rtmp_service;
    if (rtmp_service_ref) {
        RtmpService * const rtmp_service = unsafe_rtmp_service;
        rtmp_service->num_session_objects.dec ();
    }
}

mt_mutex (mutex) void
RtmpService::destroySession (ClientSession * const session)
{
    logD (rtmp_service, _func, "session: 0x", fmt_hex, (UintPtr) session);

    if (!session->valid) {
	logD (rtmp_service, _func, "invalid session");
	return;
    }
    session->valid = false;
    --num_valid_sessions;

    assert (session->pollable_key);
    session->thread_ctx->getPollGroup()->removePollable (session->pollable_key);
    session->pollable_key = NULL;

    // TODO close TCP connection explicitly.

    session_list.remove (session);
    logD (rtmp_service, _func, "session refcount: ", session->getRefCount());
    session->unref ();
}

void
RtmpService::acceptWatchdogTick (void * const _self)
{
    RtmpService * const self = static_cast <RtmpService*> (_self);

    Time const time = getTime ();

    self->mutex.lock ();

    logD_ (_func, "time: ", time, ", last_accept_time: ", self->last_accept_time);

    if (self->last_accept_time == 0) {
        self->mutex.unlock ();
        return;
    }

    if (self->last_accept_time < time
        && time - self->last_accept_time >= self->accept_watchdog_timeout_sec)
    {
        logF_ (_func, "accept watchdog hit, aborting");
        abort ();
    }

    self->mutex.unlock ();
}

bool
RtmpService::acceptOneConnection ()
{
    logD (rtmp_service, _func_);

    ServerThreadContext * const thread_ctx = server_ctx->selectThreadContext();

    Ref<ClientSession> const session = grab (new (std::nothrow) ClientSession);
    num_session_objects.inc ();
    session->session_info.creation_unixtime = getUnixtime();

    session->thread_ctx = thread_ctx;
    session->rtmp_conn.init (thread_ctx->getTimers(),
                             page_pool,
                             send_delay_millisec,
                             rtmp_ping_timeout_millisec,
                             prechunking_enabled,
                             false /* momentrtmp_proto */);

// TEST
//    session->traceReferences ();

    session->valid = true;
    session->weak_rtmp_service = this;
    session->unsafe_rtmp_service = this;

    IpAddress client_addr;
    {
	TcpServer::AcceptResult const res = tcp_server.accept (&session->tcp_conn, &client_addr);
	if (res == TcpServer::AcceptResult::Error) {
	    logE (rtmp_service, _func, "accept() failed: ", exc->toString());
	    return false;
	}

	if (res == TcpServer::AcceptResult::NotAccepted)
	    return false;

	assert (res == TcpServer::AcceptResult::Accepted);
    }
    session->session_info.client_addr = client_addr;

    // It is important to initialize conn_receiver with input blocked and
    // to unblock input only after clientConnected() call is complete.
    // Otherwise the client might loose early rtmp_conn input/error events.
    session->conn_receiver.init (&session->tcp_conn,
                                 thread_ctx->getDeferredProcessor(),
                                 true /* block_input */);

    session->conn_sender.setConnection (&session->tcp_conn);
#ifndef MOMENT__RTMP_SERVICE__USE_IMMEDIATE_SENDER
    session->conn_sender.setQueue (thread_ctx->getDeferredConnectionSenderQueue());
#endif

    session->rtmp_conn.setBackend (CbDesc<RtmpConnection::Backend> (&rtmp_conn_backend, session, session));
    session->rtmp_conn.setSender (&session->conn_sender);

    session->conn_receiver.setFrontend (session->rtmp_conn.getReceiverFrontend());

    {
        mutex.lock ();

        session->pollable_key = thread_ctx->getPollGroup()->addPollable (session->tcp_conn.getPollable(),
                                                                         true /* activate */);
        if (!session->pollable_key) {
            mutex.unlock ();
            logE (rtmp_service, _func, "PollGroup::addPollable() failed: ", exc->toString());
            return true;
        }

        // Completing session creation before calling any client callbacks.
        session_list.append (session);
        session->ref ();
        // We may call destroySession(session) from now on.

        ++num_valid_sessions;
        last_accept_time = getTime();

        mutex.unlock ();
    }

    {
	Result res = Result::Failure;
	bool const call_res =
                frontend.call_ret (&res, frontend->clientConnected,
                                   /*(*/ &session->rtmp_conn, client_addr /*)*/);
	if (!call_res || !res) {
            mutex.lock ();
            destroySession (session);
            mutex.unlock ();
	    return true;
	}
    }

    session->conn_receiver.start ();

    logD (rtmp_service, _func, "done");

    return true;
}

void
RtmpService::closeRtmpConn (void * const _session)
{
    logD (rtmp_service, _func, "session 0x", fmt_hex, (UintPtr) _session);

    ClientSession * const session = static_cast <ClientSession*> (_session);

    CodeRef const self_ref = session->weak_rtmp_service;
    if (!self_ref) {
        logD_ (_func, "self gone");
        return;
    }
    RtmpService * const self = session->unsafe_rtmp_service;

    self->mutex.lock ();
    self->destroySession (session);
    self->mutex.unlock ();
}

void
RtmpService::accepted (void * const _self)
{
    RtmpService * const self = static_cast <RtmpService*> (_self);

    for (;;) {
	if (!self->acceptOneConnection ())
	    break;
    }
}

mt_const mt_throws Result
RtmpService::init (ServerContext * const mt_nonnull server_ctx,
                   PagePool      * const mt_nonnull page_pool,
                   Time            const send_delay_millisec,
                   Time            const rtmp_ping_timeout_millisec,
                   bool            const prechunking_enabled,
                   Time            const accept_watchdog_timeout_sec)
{
    DeferredProcessor * const deferred_processor = server_ctx->getMainThreadContext()->getDeferredProcessor();
    Timers * const timers = server_ctx->getMainThreadContext()->getTimers();

    this->server_ctx = server_ctx;
    this->page_pool = page_pool;
    this->send_delay_millisec = send_delay_millisec;
    this->rtmp_ping_timeout_millisec = rtmp_ping_timeout_millisec;
    this->prechunking_enabled = prechunking_enabled;
    this->accept_watchdog_timeout_sec = accept_watchdog_timeout_sec;

    if (!tcp_server.open ())
	return Result::Failure;

    tcp_server.init (CbDesc<TcpServer::Frontend> (&tcp_server_frontend, this, getCoderefContainer()),
                     deferred_processor,
                     timers);

    if (accept_watchdog_timeout_sec != 0) {
        logD_ (_func, "starting accept watchdog timer, timeout: ",
               accept_watchdog_timeout_sec, " seconds");

        Time timeout = accept_watchdog_timeout_sec / 4;
        if (timeout == 0)
            timeout = 1;

        timers->addTimer (CbDesc<Timers::TimerCallback> (
                                  acceptWatchdogTick,
                                  this,
                                  getCoderefContainer()),
                          timeout,
                          true /* periodical */,
                          true /* auto_delete */);
    }

    return Result::Success;
}

mt_throws Result
RtmpService::bind (IpAddress addr)
{
    if (!tcp_server.bind (addr))
	return Result::Failure;

    return Result::Success;
}

mt_throws Result
RtmpService::start ()
{
    if (!tcp_server.listen ())
	return Result::Failure;

    // TODO Call removePollable() when done.
    if (!server_ctx->getMainThreadContext()->getPollGroup()->addPollable (
                tcp_server.getPollable()))
    {
	return Result::Failure;
    }

    if (!tcp_server.start ()) {
        logF_ (_func, "tcp_server.start() failed: ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

mt_mutex (mutex) void
RtmpService::updateClientSessionsInfo ()
{
    SessionList::iterator iter (session_list);
    while (!iter.done()) {
        ClientSession * const session = iter.next ();
#if 0
// TODO Update the following fields:
    Time last_send_time;
    Time last_recv_time;
    StRef<String> last_play_stream;
    StRef<String> last_publish_stream;
#endif
       session->session_info.last_send_unixtime = 0;
       session->session_info.last_recv_unixtime = 0;
    }
}

mt_mutex (mutex) RtmpService::SessionInfoIterator
RtmpService::getClientSessionsInfo_unlocked (SessionsInfo * const ret_info)
{
    if (ret_info) {
        ret_info->num_session_objects = num_session_objects.get ();
        ret_info->num_valid_sessions = num_valid_sessions;
    }

    updateClientSessionsInfo ();

    return SessionInfoIterator (*this);
}

RtmpService::RtmpService (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      prechunking_enabled (true),
      server_ctx (NULL),
      page_pool (NULL),
      send_delay_millisec (0),
      rtmp_ping_timeout_millisec (5 * 60 * 1000),
      tcp_server (coderef_container),
      num_valid_sessions (0),
      accept_watchdog_timeout_sec (0),
      last_accept_time (0)
{
}

RtmpService::~RtmpService ()
{
    mutex.lock ();

    SessionList::iter iter (session_list);
    while (!session_list.iter_done (iter)) {
	ClientSession * const session = session_list.iter_next (iter);
	destroySession (session);
    }

    mutex.unlock ();
}

}

