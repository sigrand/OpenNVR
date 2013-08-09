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


#include <moment/rtmpt_service.h>


// TODO Current RTMPT implementation is not secure enough.


// Hint: Don't put commas after *HEADERS macros when using them.

#define RTMPT_SERVICE__HEADERS_DATE \
	Byte date_buf [unixtimeToString_BufSize]; \
	Size const date_len = unixtimeToString (Memory::forObject (date_buf), getUnixtime());

#define RTMPT_SERVICE__COMMON_HEADERS(keepalive) \
	"Server: Moment/1.0\r\n" \
	"Date: ", ConstMemory (date_buf, date_len), "\r\n" \
	"Connection: ", (keepalive) ? "Keep-Alive" : "Close", "\r\n" \
	"Cache-Control: no-cache\r\n"

#define RTMPT_SERVICE__OK_HEADERS(keepalive) \
	"HTTP/1.", ((keepalive) ? "1" : "1"), " 200 OK\r\n" \
	RTMPT_SERVICE__COMMON_HEADERS(keepalive)

#define RTMPT_SERVICE__FCS_OK_HEADERS(keepalive) \
	RTMPT_SERVICE__OK_HEADERS(keepalive) \
	"Content-Type: application/x-fcs\r\n" \

#define RTMPT_SERVICE__404_HEADERS(keepalive) \
	"HTTP/1.", (keepalive) ? "1" : "1", " 404 Not found\r\n" \
	RTMPT_SERVICE__COMMON_HEADERS(keepalive) \
	"Content-Type: text/plain\r\n" \
	"Content-Length: 0\r\n"

#define RTMPT_SERVICE__400_HEADERS(keepalive) \
	"HTTP/1.", (keepalive) ? "1" : "1", " 400 Bad Request\r\n" \
	RTMPT_SERVICE__COMMON_HEADERS(keepalive) \
	"Content-Type: text/plain\r\n" \
	"Content-Length: 0\r\n"


using namespace M;

namespace Moment {

static LogGroup libMary_logGroup_rtmpt ("rtmpt", LogLevel::I);

RtmpConnection::Backend const RtmptService::rtmp_conn_backend = {
    rtmpClosed
};

mt_async void
RtmptService::RtmptSender::sendMessage (Sender::MessageEntry * const mt_nonnull msg_entry,
                                        bool                   const do_flush)
{
    mutex.lock ();
    sendMessage_unlocked (msg_entry, do_flush);
    mutex.unlock ();
}

mt_mutex (mutex) void
RtmptService::RtmptSender::sendMessage_unlocked (Sender::MessageEntry * const mt_nonnull msg_entry,
                                                 bool                   const do_flush)
{
    nonflushed_msg_list.append (msg_entry);

    // Counting length of new data.
    switch (msg_entry->type) {
	case Sender::MessageEntry::Pages: {
	    Sender::MessageEntry_Pages * const msg_pages =
		    static_cast <Sender::MessageEntry_Pages*> (msg_entry);
	    nonflushed_data_len += msg_pages->getTotalMsgLen();
	} break;
	default:
	    unreachable ();
    }

    if (do_flush)
	doFlush ();
}

mt_mutex (mutex) void
RtmptService::RtmptSender::doFlush ()
{
    pending_msg_list.stealAppend (nonflushed_msg_list.getFirst(), nonflushed_msg_list.getLast());
    pending_data_len += nonflushed_data_len;

    nonflushed_msg_list.clear();
    nonflushed_data_len = 0;
}

mt_async void
RtmptService::RtmptSender::flush ()
{
    mutex.lock ();
    doFlush ();
    mutex.unlock ();
}

mt_mutex (mutex) void
RtmptService::RtmptSender::flush_unlocked ()
{
    doFlush ();
}

mt_async void
RtmptService::RtmptSender::closeAfterFlush ()
{
    mutex.lock ();
    close_after_flush = true;
    mutex.unlock ();
}

mt_async void
RtmptService::RtmptSender::close ()
{
    closeAfterFlush ();
}

mt_mutex (mutex) bool
RtmptService::RtmptSender::isClosed_unlocked ()
{
    return false;
}

mt_mutex (mutex) Sender::SendState
RtmptService::RtmptSender::getSendState_unlocked ()
{
    return SendState::ConnectionReady;
}

void
RtmptService::RtmptSender::lock ()
{
    mutex.lock ();
}

void
RtmptService::RtmptSender::unlock ()
{
    mutex.unlock ();
}

mt_mutex (mutex) void
RtmptService::RtmptSender::sendPendingData (Sender * const mt_nonnull sender)
{
    MessageList::iter iter (pending_msg_list);
    while (!pending_msg_list.iter_done (iter)) {
	MessageEntry * const msg_entry = pending_msg_list.iter_next (iter);
	sender->sendMessage (msg_entry, false /* do_flush */);
    }

    pending_msg_list.clear ();
    pending_data_len = 0;
}

RtmptService::RtmptSender::RtmptSender (Object * const coderef_container)
    : Sender (coderef_container),
      DependentCodeReferenced (coderef_container),
      nonflushed_data_len (0),
      pending_data_len (0),
      close_after_flush (false)
{
}

mt_async
RtmptService::RtmptSender::~RtmptSender ()
{
    mutex.lock ();

    {
	MessageList::iter iter (nonflushed_msg_list);
	while (!nonflushed_msg_list.iter_done (iter)) {
	    MessageEntry * const msg_entry = nonflushed_msg_list.iter_next (iter);
	    Sender::deleteMessageEntry (msg_entry);
	}
    }

    {
	MessageList::iter iter (pending_msg_list);
	while (!pending_msg_list.iter_done (iter)) {
	    MessageEntry * const msg_entry = pending_msg_list.iter_next (iter);
	    Sender::deleteMessageEntry (msg_entry);
	}
    }

    mutex.unlock ();
}

RtmptService::RtmptSession::~RtmptSession ()
{
    CodeDepRef<RtmptService> const rtmpt_service = weak_rtmpt_service;
    rtmpt_service->num_session_objects.dec ();
}

RtmptService::RtmptConnection::~RtmptConnection ()
{
    CodeDepRef<RtmptService> const rtmpt_service = weak_rtmpt_service;
    rtmpt_service->num_connection_objects.dec ();
}

void
RtmptService::sessionKeepaliveTimerTick (void * const _session)
{
    RtmptSession * const session = static_cast <RtmptSession*> (_session);

    CodeDepRef<RtmptService> const self = session->weak_rtmpt_service;
    if (!self)
        return;

    self->mutex.lock ();
    if (!session->valid) {
	self->mutex.unlock ();
	return;
    }

    Time const cur_time = getTime();
    if (cur_time >= session->last_msg_time &&
	cur_time - session->last_msg_time > self->session_keepalive_timeout)
    {
	logD (rtmpt, _func, "RTMPT session timeout");
	mt_unlocks (mutex) self->destroyRtmptSession (session, true /* close_rtmp_conn */);
    } else {
        self->mutex.unlock ();
    }
}

void
RtmptService::connKeepaliveTimerTick (void * const _rtmpt_conn)
{
    RtmptConnection * const rtmpt_conn = static_cast <RtmptConnection*> (_rtmpt_conn);

    CodeDepRef<RtmptService> const self = rtmpt_conn->weak_rtmpt_service;
    if (!self)
        return;

    self->mutex.lock ();
    if (!rtmpt_conn->valid) {
        self->mutex.unlock ();
        return;
    }

    Time const cur_time = getTime();
    if (cur_time >= rtmpt_conn->last_msg_time &&
        cur_time - rtmpt_conn->last_msg_time > self->conn_keepalive_timeout)
    {
        logD (rtmpt, _func, "RTMPT connection timeout");
        mt_unlocks (mutex) self->doConnectionClosed (rtmpt_conn);
    } else {
        self->mutex.unlock ();
    }
}

mt_unlocks (mutex) void
RtmptService::destroyRtmptSession (RtmptSession * const mt_nonnull session,
                                   bool           const close_rtmp_conn)
{
    if (!session->valid) {
        mutex.unlock ();
	return;
    }
    session->valid = false;

    if (session->session_keepalive_timer) {
	server_ctx->getMainThreadContext()->getTimers()->deleteTimer (session->session_keepalive_timer);
	session->session_keepalive_timer = NULL;
    }

    Ref<RtmptSession> tmp_session = session;
    if (!session->session_map_entry.isNull()) {
        session_map.remove (session->session_map_entry);
        --num_valid_sessions;
    }

    mutex.unlock ();

    if (close_rtmp_conn) {
        tmp_session->rtmp_conn.close_noBackendCb ();
        tmp_session = NULL; // last unref
    }
}

mt_mutex (mutex) void
RtmptService::destroyRtmptConnection (RtmptConnection * const mt_nonnull rtmpt_conn)
{
    if (!rtmpt_conn->valid) {
	return;
    }
    rtmpt_conn->valid = false;
    --num_valid_connections;

    logI (rtmpt, _func, "closed, rtmpt_conn 0x", fmt_hex, (UintPtr) rtmpt_conn);

    CodeDepRef<ServerThreadContext> const thread_ctx = rtmpt_conn->weak_thread_ctx;

    if (thread_ctx) {
        if (rtmpt_conn->conn_keepalive_timer) {
            thread_ctx->getTimers()->deleteTimer (rtmpt_conn->conn_keepalive_timer);
            rtmpt_conn->conn_keepalive_timer = NULL;
        }

        if (rtmpt_conn->pollable_key) {
            thread_ctx->getPollGroup()->removePollable (rtmpt_conn->pollable_key);
            rtmpt_conn->pollable_key = NULL;
        }
    }

    conn_list.remove (rtmpt_conn);
    rtmpt_conn->unref ();
}

mt_unlocks (mutex) void
RtmptService::doConnectionClosed (RtmptConnection * const mt_nonnull rtmpt_conn)
{
    destroyRtmptConnection (rtmpt_conn);
    mutex.unlock ();

#if 0
// Too early to close the connection. It is still in use.
// In particular, it is very likely that processInput() is yet to be called
// for this connection.

    rtmpt_conn->conn->close ();
#endif
}

mt_async void
RtmptService::rtmpClosed (void * const _session)
{
    RtmptSession * const session = static_cast <RtmptSession*> (_session);

    CodeDepRef<RtmptService> const self = session->weak_rtmpt_service;
    if (!self)
        return;

    self->mutex.lock ();
    mt_unlocks (mutex) self->destroyRtmptSession (session, false /* close_rtmp_conn */);
}

void
RtmptService::sendDataInReply (Sender       * const mt_nonnull conn_sender,
                               RtmptSession * const mt_nonnull session)
{
    session->rtmpt_sender.mutex.lock ();

    RTMPT_SERVICE__HEADERS_DATE
    conn_sender->send (
	    page_pool,
	    false /* do_flush */,
	    RTMPT_SERVICE__FCS_OK_HEADERS(!no_keepalive_conns)
	    "Content-Length: ", 1 /* idle interval */ + session->rtmpt_sender.pending_data_len, "\r\n"
	    "\r\n",
	    // TODO Variable idle intervals.
	    "\x09");

    session->rtmpt_sender.sendPendingData (conn_sender);
    conn_sender->flush ();

    if (session->rtmpt_sender.close_after_flush)
        session->closed = true;

    session->rtmpt_sender.mutex.unlock ();

#if 0
// We're not destroying the session immediately, since more requests may arrive
// from the client for this session. The session is expected to time out eventually.

    // If close after flush has been requested for session->rtmpt_sender, then
    // virtual RTMP connection should be closed, hence we're destroying the session.
    if (destroy_session) {
        mutex.lock ();
	mt_unlocks (mutex) destroyRtmptSession (session, true /* close_rtmp_conn */);
    }
#endif
}

void
RtmptService::doOpen (Sender * const mt_nonnull conn_sender,
		     IpAddress const client_addr)
{
    Ref<RtmptSession> const session = grab (new (std::nothrow) RtmptSession);
    num_session_objects.inc ();
    session->session_info.creation_unixtime = getUnixtime();

    session->valid = true;
    session->closed = false;
    session->weak_rtmpt_service = this;
    session->last_msg_time = getTime();
    // TODO Do not allow more than one external IP address for a single session.
    //      ^^^ Maybe configurable, disallowed by default.
    session->session_info.last_client_addr = client_addr;

    session->session_id = session_id_counter;
    ++session_id_counter;

    session->rtmp_conn.init (server_ctx->getMainThreadContext()->getTimers(),
                             page_pool,
                             0 /* send_delay_millisec */,
                             rtmp_ping_timeout_millisec,
                             prechunking_enabled,
                             false /* momentrtmp_proto */);
    session->rtmp_conn.setBackend (
            CbDesc<RtmpConnection::Backend> (&rtmp_conn_backend, session, session));
    session->rtmp_conn.setSender (&session->rtmpt_sender);

    {
	Result res = Result::Failure;
	bool const call_res =
                frontend.call_ret (&res, frontend->clientConnected,
                                   /*(*/ &session->rtmp_conn, client_addr /*)*/);
	if (!call_res || !res) {
            session->rtmp_conn.close_noBackendCb ();

            RTMPT_SERVICE__HEADERS_DATE
            conn_sender->send (
                    page_pool,
                    true /* do_flush */,
                    RTMPT_SERVICE__404_HEADERS(!no_keepalive_conns)
                    "\r\n");
            return;
	}
    }

    mutex.lock ();
    // The session might have been invalidated by rtmpClose().
    if (!session->valid) {
        mutex.unlock ();
        RTMPT_SERVICE__HEADERS_DATE
        conn_sender->send (
                page_pool,
                true /* do_flush */,
                RTMPT_SERVICE__404_HEADERS(!no_keepalive_conns)
                "\r\n");
        return;
    }

    session->session_map_entry = session_map.add (session);
    ++num_valid_sessions;

    {
	// Checking for session timeout at least each 10 seconds.
	Time const timeout = (session_keepalive_timeout >= 10 ? 10 : session_keepalive_timeout);
	session->session_keepalive_timer =
                server_ctx->getMainThreadContext()->getTimers()->addTimer (
                        CbDesc<Timers::TimerCallback> (sessionKeepaliveTimerTick,
                                                       session,
                                                       session /* coderef_container */),
                        timeout,
                        true  /* periodical */,
                        false /* auto_delete */);
    }
    mutex.unlock ();

    RTMPT_SERVICE__HEADERS_DATE
    conn_sender->send (
	    page_pool,
	    true /* do_flush */,
	    RTMPT_SERVICE__FCS_OK_HEADERS(!no_keepalive_conns)
	    "Content-Length: ", toString (Memory(), session->session_id) + 1 /* for \n */, "\r\n"
	    "\r\n",
	    session->session_id,
	    "\n");
}

Ref<RtmptService::RtmptSession>
RtmptService::doSend (Sender          * const mt_nonnull conn_sender,
                      Uint32            const session_id,
                      RtmptConnection * const rtmpt_conn)
{
    mutex.lock ();
    SessionMap::Entry const session_entry = session_map.lookup (session_id);
    Ref<RtmptSession> session;
    if (!session_entry.isNull()) {
        session = session_entry.getData();
        if (session->closed)
            logD_ (_func, "Session closed: ", session_id);
    } else {
        logD_ (_func, "Session not found: ", session_id);
    }

    if (session_entry.isNull()
        || session->closed)
    {
        mutex.unlock ();
        RTMPT_SERVICE__HEADERS_DATE
        conn_sender->send (
                page_pool,
                true /* do_flush */,
                RTMPT_SERVICE__404_HEADERS(!no_keepalive_conns)
                "\r\n");
        return NULL;
    }

    {
        Time const cur_time = getTime();
        session->last_msg_time = cur_time;
        if (rtmpt_conn)
            rtmpt_conn->last_msg_time = cur_time;
    }

    mutex.unlock ();

    sendDataInReply (conn_sender, session);
    return session;
}

void
RtmptService::doClose (Sender * const mt_nonnull conn_sender,
                       Uint32   const session_id)
{
    mutex.lock ();
    SessionMap::Entry const session_entry = session_map.lookup (session_id);
    if (session_entry.isNull()) {
        mutex.unlock ();

	logD_ (_func, "Session not found: ", session_id);

	RTMPT_SERVICE__HEADERS_DATE
	conn_sender->send (
		page_pool,
		true /* do_flush */,
		RTMPT_SERVICE__404_HEADERS(!no_keepalive_conns)
		"\r\n");
	return;
    }

    Ref<RtmptSession> const session = session_entry.getData();
    mt_unlocks (mutex) destroyRtmptSession (session, true /* close_rtmp_conn */);

    RTMPT_SERVICE__HEADERS_DATE
    conn_sender->send (
	    page_pool,
	    true /* do_flush */,
	    RTMPT_SERVICE__OK_HEADERS(!no_keepalive_conns)
	    "Content-Type: text/plain\r\n"
	    "Content-Length: 0\r\n"
	    "\r\n");
}

Ref<RtmptService::RtmptSession>
RtmptService::doHttpRequest (HttpRequest     * const mt_nonnull req,
                             Sender          * const mt_nonnull conn_sender,
                             RtmptConnection * const rtmpt_conn)
{
    logD (rtmpt, _func, req->getRequestLine());

    ConstMemory const command = req->getPath (0);

    if (equal (command, "send")) {
        Uint32 const session_id = strToUlong (req->getPath (1));
        return doSend (conn_sender, session_id, rtmpt_conn);
    } else
    if (equal (command, "idle")) {
        Uint32 const session_id = strToUlong (req->getPath (1));
        doSend (conn_sender, session_id, rtmpt_conn);
    } else
    if (equal (command, "open")) {
        doOpen (conn_sender, req->getClientAddress());
    } else
    if (equal (command, "close")) {
        Uint32 const session_id = strToUlong (req->getPath (1));
        doClose (conn_sender, session_id);
    } else {
        if (!equal (command, "fcs"))
            logW_ (_func, "uknown command: ", command);

        RTMPT_SERVICE__HEADERS_DATE
        conn_sender->send (
                page_pool,
                true /* do_flush */,
                RTMPT_SERVICE__400_HEADERS (!no_keepalive_conns)
                "\r\n");
    }

    return NULL;
}

HttpServer::Frontend const RtmptService::http_frontend = {
    httpRequest,
    httpMessageBody,
    httpClosed
};

mt_async void
RtmptService::httpRequest (HttpRequest * const req,
                           void        * const _rtmpt_conn)
{
    RtmptConnection * const rtmpt_conn = static_cast <RtmptConnection*> (_rtmpt_conn);
    CodeDepRef<RtmptService> const self = rtmpt_conn->weak_rtmpt_service;
    if (!self)
        return;

    rtmpt_conn->cur_req_session = NULL;
    Ref<RtmptSession> const session = self->doHttpRequest (req, &rtmpt_conn->conn_sender, rtmpt_conn);
    if (session && req->hasBody())
        rtmpt_conn->cur_req_session = session;

    if (!req->getKeepalive() || self->no_keepalive_conns)
	rtmpt_conn->conn_sender.closeAfterFlush ();
}

mt_async void
RtmptService::httpMessageBody (HttpRequest * const mt_nonnull /* req */,
                               Memory        const mem,
                               bool          const /* end_of_request */,
                               Size        * const mt_nonnull ret_accepted,
                               void        * const  _rtmpt_conn)
{
    RtmptConnection * const rtmpt_conn = static_cast <RtmptConnection*> (_rtmpt_conn);
    CodeDepRef<RtmptService> const self = rtmpt_conn->weak_rtmpt_service;
    if (!self)
        return;

    RtmptSession * const session = rtmpt_conn->cur_req_session;
    if (!session) {
	*ret_accepted = mem.len();
        return;
    }

  // 'cur_req_session' is not null, which means that we're processing
  // message body of a "/send" request.

  // Note that we don't check 'cur_req_session->valid' here because
  // that would require an extra mutex lock. Calling rtmp_conn.doProcessInput()
  // for an invalid session should be harmless.

    Size accepted;
    Receiver::ProcessInputResult res;
    {
        session->rtmp_input_mutex.lock ();
        res = session->rtmp_conn.doProcessInput (mem, &accepted);
        session->rtmp_input_mutex.unlock ();
    }
    if (res == Receiver::ProcessInputResult::Error) {
	logE_ (_func, "failed to parse RTMP data: ", toString (res));

        self->mutex.lock ();
	mt_unlocks (mutex) self->destroyRtmptSession (session, true /* close_rtmp_conn */);

	rtmpt_conn->cur_req_session = NULL;

	*ret_accepted = mem.len();
    } else {
        // RtmpConnection::doProcessInput() never returns InputBlocked.
        assert (res != Receiver::ProcessInputResult::InputBlocked);
        *ret_accepted = accepted;
    }
}

mt_async void
RtmptService::httpClosed (HttpRequest * const /* req */,
                          Exception   * const exc_,
                          void        * const _rtmpt_conn)
{
    if (exc_)
	logE_ (_func, "exception: ", exc_->toString());

    RtmptConnection * const rtmpt_conn = static_cast <RtmptConnection*> (_rtmpt_conn);

    CodeDepRef<RtmptService> const self = rtmpt_conn->weak_rtmpt_service;
    if (!self)
        return;

    self->mutex.lock ();
    mt_unlocks (mutex) self->doConnectionClosed (rtmpt_conn);
}

HttpService::HttpHandler const RtmptService::http_handler = {
    service_httpRequest,
    service_httpMessageBody
};

Result
RtmptService::service_httpRequest (HttpRequest   * const mt_nonnull req,
                                   Sender        * const mt_nonnull conn_sender,
                                   Memory const  & /* msg_body */,
                                   void         ** const mt_nonnull ret_msg_data,
                                   void          * const _self)
{
    RtmptService * const self = static_cast <RtmptService*> (_self);

    Ref<RtmptSession> session = self->doHttpRequest (req, conn_sender, NULL /* rtmpt_conn */);
    if (session && req->hasBody()) {
        *ret_msg_data = session;
        session.setNoUnref ((RtmptSession*) NULL);
    }

    if (!req->getKeepalive() || self->no_keepalive_conns)
	conn_sender->closeAfterFlush ();

    return Result::Success;
}

// Almost identical to httpMessaggeBody()
Result
RtmptService::service_httpMessageBody (HttpRequest  * const mt_nonnull /* req */,
                                       Sender       * const mt_nonnull /* conn_sender */,
                                       Memory const &mem,
                                       bool           const end_of_request,
                                       Size         * const mt_nonnull ret_accepted,
                                       void         * const _session,
                                       void         * const _self)
{
    RtmptService * const self = static_cast <RtmptService*> (_self);
    RtmptSession * const session = static_cast <RtmptSession*> (_session);

    if (!session) {
	*ret_accepted = mem.len();
	return Result::Success;
    }

  // 'cur_req_session' is not null, which means that we're processing
  // message body of a "/send" request.

  // Note that we don't check 'cur_req_session->valid' here because
  // that would require an extra mutex lock. Calling rtmp_conn.doProcessInput()
  // for an invalid session should be harmless.

    Size accepted;
    Receiver::ProcessInputResult res;
    {
        session->rtmp_input_mutex.lock ();
        res = session->rtmp_conn.doProcessInput (mem, &accepted);
        session->rtmp_input_mutex.unlock ();
    }
    if (res == Receiver::ProcessInputResult::Error) {
	logE_ (_func, "failed to parse RTMP data: ", toString (res));

	self->mutex.lock ();
	mt_unlocks (mutex) self->destroyRtmptSession (session, true /* close_rtmp_conn */);

	*ret_accepted = mem.len();
    } else {
        // RtmpConnection::doProcessInput() never returns InputBlocked.
        assert (res != Receiver::ProcessInputResult::InputBlocked);
        *ret_accepted = accepted;
    }

    if (end_of_request) {
        if (*ret_accepted != mem.len()) {
            logW_ (_func, "RTMPT request contains an incomplete RTMP message");
            self->mutex.lock ();
            mt_unlocks (mutex) self->destroyRtmptSession (session, true /* close_rtmp_conn */);
            session->unref ();
            return Result::Failure;
        }

	session->unref ();
    }

    return Result::Success;
}

bool
RtmptService::acceptOneConnection ()
{
    Ref<RtmptConnection> const rtmpt_conn = grab (new (std::nothrow) RtmptConnection);
    num_connection_objects.inc ();
    rtmpt_conn->connection_info.creation_unixtime = getUnixtime();

    rtmpt_conn->valid = true;
    rtmpt_conn->weak_rtmpt_service = this;

    rtmpt_conn->last_msg_time = getTime();
    rtmpt_conn->cur_req_session = NULL;

    CodeDepRef<ServerThreadContext> const thread_ctx = server_ctx->selectThreadContext ();
    rtmpt_conn->weak_thread_ctx = thread_ctx;

    IpAddress client_addr;
    {
	TcpServer::AcceptResult const res = tcp_server.accept (&rtmpt_conn->tcp_conn, &client_addr);
	if (res == TcpServer::AcceptResult::Error) {
	    logE_ (_func, exc->toString());
	    return false;
	}

	if (res == TcpServer::AcceptResult::NotAccepted)
	    return false;

	assert (res == TcpServer::AcceptResult::Accepted);
    }
    rtmpt_conn->connection_info.client_addr = client_addr;

    rtmpt_conn->conn_sender.init (thread_ctx->getDeferredProcessor());
    rtmpt_conn->conn_sender.setConnection (&rtmpt_conn->tcp_conn);
    rtmpt_conn->conn_receiver.init (&rtmpt_conn->tcp_conn, thread_ctx->getDeferredProcessor());

    rtmpt_conn->http_server.init (CbDesc<HttpServer::Frontend> (&http_frontend, rtmpt_conn, rtmpt_conn),
                                  &rtmpt_conn->conn_receiver,
                                  &rtmpt_conn->conn_sender,
                                  page_pool,
                                  client_addr);

    logI (rtmpt, _func, "accepted rtmpt_conn 0x", fmt_hex, (UintPtr) rtmpt_conn.ptr(), ", client_addr ", client_addr);

    mutex.lock ();
    rtmpt_conn->pollable_key =
            thread_ctx->getPollGroup()->addPollable (rtmpt_conn->tcp_conn.getPollable(),
                                                     true /* activate */);
    if (!rtmpt_conn->pollable_key) {
        mutex.unlock ();
	logE_ (_func, "PollGroup::addPollable() failed: ", exc->toString ());
        return true;
    }

    if (conn_keepalive_timeout > 0) {
        rtmpt_conn->conn_keepalive_timer =
                thread_ctx->getTimers()->addTimer_microseconds (
                        CbDesc<Timers::TimerCallback> (
                                connKeepaliveTimerTick, rtmpt_conn, rtmpt_conn),
                        conn_keepalive_timeout,
                        true  /* periodical */,
                        false /* auto_delete */);
    }

    conn_list.append (rtmpt_conn);
    rtmpt_conn->ref ();
    ++num_valid_connections;
    mutex.unlock ();

    rtmpt_conn->conn_receiver.start ();

    return true;
}

TcpServer::Frontend const RtmptService::tcp_server_frontend = {
    accepted
};

void
RtmptService::accepted (void * const _self)
{
    RtmptService * const self = static_cast <RtmptService*> (_self);

    for (;;) {
	if (!self->acceptOneConnection ())
	    break;
    }
}

void
RtmptService::attachToHttpService (HttpService * const http_service,
                                   // TODO Use path? Does it work with RTMPT?
                                   ConstMemory   const /* path */)
{
    ConstMemory const paths [] = { ConstMemory ("send"),
                                   ConstMemory ("idle"),
                                   ConstMemory ("open"),
                                   ConstMemory ("close"),
                                   ConstMemory ("fcs") };
    Size const num_paths = sizeof (paths) / sizeof (ConstMemory);

    for (unsigned i = 0; i < num_paths; ++i) {
	http_service->addHttpHandler (
		CbDesc<HttpService::HttpHandler> (&http_handler, this, getCoderefContainer()),
		paths [i],
		false /* preassembly */,
		0     /* preassembly_limit */,
		false /* parse_body_params */);
    }
}

mt_throws Result
RtmptService::bind (IpAddress const addr)
{
    if (!tcp_server.bind (addr))
	return Result::Failure;

    return Result::Success;
}

mt_throws Result
RtmptService::start ()
{
    if (!tcp_server.listen ())
	return Result::Failure;

    mutex.lock ();
    assert (!server_pollable_key);
    server_pollable_key =
            server_ctx->getMainThreadContext()->getPollGroup()->addPollable (
                    tcp_server.getPollable());
    if (!server_pollable_key) {
        mutex.unlock ();
	return Result::Failure;
    }
    mutex.unlock ();

    if (!tcp_server.start ()) {
        logF_ (_func, "tcp_server.start() failed: ", exc->toString());

        mutex.lock ();
        server_ctx->getMainThreadContext()->getPollGroup()->removePollable (server_pollable_key);
        server_pollable_key = NULL;
        mutex.unlock ();

        return Result::Failure;
    }

    return Result::Success;
}

mt_mutex (mutex) void
RtmptService::updateRtmptSessionsInfo ()
{
    Time const cur_unixtime = getUnixtime();
    Time const cur_time = getTime();

    SessionMap::data_iterator iter (session_map);
    while (!iter.done()) {
        RtmptSession * const session = iter.next ();
        session->session_info.last_req_unixtime = cur_unixtime - (cur_time - session->last_msg_time);
    }
}

mt_mutex (mutex) RtmptService::RtmptSessionInfoIterator
RtmptService::getRtmptSessionsInfo_unlocked (RtmptSessionsInfo * const ret_info)
{
    if (ret_info) {
        ret_info->num_session_objects = num_session_objects.get ();
        ret_info->num_valid_sessions = num_valid_sessions;
    }

    updateRtmptSessionsInfo ();

    return RtmptSessionInfoIterator (*this);
}

mt_mutex (mutex) void
RtmptService::updateRtmptConnectionsInfo ()
{
    Time const cur_unixtime = getUnixtime();
    Time const cur_time = getTime();

    ConnectionList::iterator iter (conn_list);
    while (!iter.done()) {
        RtmptConnection * const rtmpt_conn = iter.next ();
        rtmpt_conn->connection_info.last_req_unixtime = cur_unixtime - (cur_time - rtmpt_conn->last_msg_time);
    }
}

mt_mutex (mutex) RtmptService::RtmptConnectionInfoIterator
RtmptService::getRtmptConnectionsInfo_unlocked (RtmptConnectionsInfo * const ret_info)
{
    if (ret_info) {
        ret_info->num_connection_objects = num_connection_objects.get ();
        ret_info->num_valid_connections = num_valid_connections;
    }

    updateRtmptConnectionsInfo ();

    return RtmptConnectionInfoIterator (*this);
}

mt_const Result
RtmptService::init (ServerContext * const mt_nonnull server_ctx,
                    PagePool      * const mt_nonnull page_pool,
                    bool            const enable_standalone_tcp_server,
                    Time            const rtmp_ping_timeout_millisec,
                    Time            const session_keepalive_timeout,
                    Time            const conn_keepalive_timeout,
                    bool            const no_keepalive_conns,
                    bool            const prechunking_enabled)
{
    this->frontend                   = frontend;
    this->server_ctx                 = server_ctx;
    this->page_pool                  = page_pool;
    this->rtmp_ping_timeout_millisec = rtmp_ping_timeout_millisec;
    this->session_keepalive_timeout  = session_keepalive_timeout;
    this->conn_keepalive_timeout     = conn_keepalive_timeout;
    this->no_keepalive_conns         = no_keepalive_conns;
    this->prechunking_enabled        = prechunking_enabled;

    if (enable_standalone_tcp_server) {
        tcp_server.init (CbDesc<TcpServer::Frontend> (&tcp_server_frontend, this, getCoderefContainer()),
                         server_ctx->getMainThreadContext()->getDeferredProcessor(),
                         server_ctx->getMainThreadContext()->getTimers());

        if (!tcp_server.open ())
            return Result::Failure;
    }

    return Result::Success;
}

RtmptService::RtmptService (Object * const coderef_container)
    : DependentCodeReferenced    (coderef_container),
      prechunking_enabled        (true),
      rtmp_ping_timeout_millisec (5 * 60 * 1000),
      session_keepalive_timeout  (60),
      conn_keepalive_timeout     (60),
      no_keepalive_conns         (false),
      server_ctx                 (coderef_container),
      page_pool                  (coderef_container),
      tcp_server                 (coderef_container),
      session_id_counter         (1),
      num_valid_sessions         (0),
      num_valid_connections      (0)
{
}

RtmptService::~RtmptService ()
{
    mutex.lock ();

    if (server_pollable_key) {
        server_ctx->getMainThreadContext()->getPollGroup()->removePollable (server_pollable_key);
        server_pollable_key = NULL;
    }

    {
	SessionMap::Iterator iter (session_map);
	while (!iter.done ()) {
	    Ref<RtmptSession> const session = iter.next().getData();
	    mt_unlocks (mutex) destroyRtmptSession (session, true /* close_rtmp_conn */);
            mutex.lock ();
	}
    }

    {
	ConnectionList::iter iter (conn_list);
	while (!conn_list.iter_done (iter)) {
	    RtmptConnection * const rtmpt_conn = conn_list.iter_next (iter);
	    destroyRtmptConnection (rtmpt_conn);
	}
    }

    mutex.unlock ();
}

}

