/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011 Dmitry Shatrov

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include <libmary/log.h>
#include <libmary/util_dev.h>

#include <libmary/http_service.h>


// TODO Use selectThreadContext() for multithreading


namespace M {

static LogGroup libMary_logGroup_http_service ("http_service", LogLevel::N);

HttpService::HttpConnection::HttpConnection ()
    : valid (true),
      tcp_conn      (this),
      conn_sender   (this),
      conn_receiver (this),
      http_server   (this),
      pollable_key  (NULL),
      receiving_body  (false),
      preassembly_buf (NULL)
{
    logD (http_service, _func, "0x", fmt_hex, (UintPtr) this);
}

HttpService::HttpConnection::~HttpConnection ()
{
    logD (http_service, _func, "0x", fmt_hex, (UintPtr) this);

    if (preassembly_buf)
	delete[] preassembly_buf;
}

mt_mutex (mutex) void
HttpService::releaseHttpConnection (HttpConnection * const mt_nonnull http_conn)
{
    if (http_conn->conn_keepalive_timer) {
	timers->deleteTimer (http_conn->conn_keepalive_timer);
	http_conn->conn_keepalive_timer = NULL;
    }

    poll_group->removePollable (http_conn->pollable_key);
}

void
HttpService::destroyHttpConnection (HttpConnection * const mt_nonnull http_conn)
{
    mutex.lock ();
    if (!http_conn->valid) {
	mutex.unlock ();
	return;
    }
    http_conn->valid = false;

    releaseHttpConnection (http_conn);
    conn_list.remove (http_conn);
    mutex.unlock ();

    http_conn->unref ();
}

void
HttpService::connKeepaliveTimerExpired (void * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    logI (http_service, _func, "0x", fmt_hex, (UintPtr) (http_conn));

    CodeDepRef<HttpService> const self = http_conn->weak_http_service;
    if (!self)
        return;

    // Timers belong to the same thread as PollGroup, hence this call is safe.
    self->doCloseHttpConnection (http_conn, NULL /* req */);
}

HttpServer::Frontend const HttpService::http_frontend = {
    httpRequest,
    httpMessageBody,
    httpClosed
};

void
HttpService::httpRequest (HttpRequest * const mt_nonnull req,
			  void        * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    logD_ (_func, "http_conn 0x", fmt_hex, (UintPtr) http_conn, ": ", req->getRequestLine());

    CodeDepRef<HttpService> const self = http_conn->weak_http_service;
    if (!self)
        return;

    http_conn->cur_handler = NULL;
    http_conn->cur_msg_data = NULL;

    self->mutex.lock ();

    if (self->no_keepalive_conns)
	req->setKeepalive (false);

    if (http_conn->conn_keepalive_timer) {
#warning fix race
        // FIXME Race condition: the timer might have just expired
        //       and an assertion in Timers::restartTimer() will be hit.
        self->timers->restartTimer (http_conn->conn_keepalive_timer);
    }

  // Searching for a handler with the longest matching path.
  //
  //     /a/b/c/  - last path element should be empty;
  //     /a/b/c   - last path element is "c".

    Namespace *cur_namespace = &self->root_namespace;
    Count const num_path_els = req->getNumPathElems();
    ConstMemory handler_path_el;
    for (Count i = 0; i < num_path_els; ++i) {
	ConstMemory const path_el = req->getPath (i);
	Namespace::NamespaceHash::EntryKey const namespace_key = cur_namespace->namespace_hash.lookup (path_el);
	if (!namespace_key) {
	    handler_path_el = path_el;
	    break;
	}

	cur_namespace = namespace_key.getData();
	assert (cur_namespace);
    }

    Namespace::HandlerHash::EntryKey handler_key = cur_namespace->handler_hash.lookup (handler_path_el);
    if (!handler_key) {
	handler_key = cur_namespace->handler_hash.lookup (ConstMemory());
	// We could add a trailing slash to the path and redirect the client.
	// This would make both "http://a.b/c" and "http://a.b/c/" work.
    }
    if (!handler_key) {
	self->mutex.unlock ();
	logD (http_service, _func, "No suitable handler found");

	ConstMemory const reply_body = "404 Not Found";

	Byte date_buf [unixtimeToString_BufSize];
	Size const date_len = unixtimeToString (Memory::forObject (date_buf), getUnixtime());
	logD (http_service, _func, "page_pool: 0x", fmt_hex, (UintPtr) self->page_pool.ptr());
	http_conn->conn_sender.send (
		self->page_pool,
		true /* do_flush */,
		"HTTP/1.1 404 Not found\r\n"
		"Server: Moment/1.0\r\n"
		"Date: ", ConstMemory (date_buf, date_len), "\r\n"
		"Connection: Keep-Alive\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: ", reply_body.len(), "\r\n"
		"\r\n",
		reply_body);

	if (!req->getKeepalive())
            http_conn->conn_sender.closeAfterFlush ();

	return;
    }
    HandlerEntry * const handler = handler_key.getDataPtr();

    // Note: We count on the fact that handler entries are never removed during
    // lifetime of HttpService. This may change in the future, in which case
    // we'll have to add an extra reference to handler entry here.
    self->mutex.unlock ();

    http_conn->cur_handler = handler;
    logD (http_service, _func, "http_conn->cur_handler: 0x", fmt_hex, (UintPtr) http_conn->cur_handler);

    http_conn->receiving_body = false;
    http_conn->preassembled_len = 0;

    if (!handler->preassembly || !req->hasBody()) {
        if (handler->cb && handler->cb->httpRequest) {
            Result res = Result::Failure;
            if (!handler->cb.call_ret<Result> (
                        &res,
                        handler->cb->httpRequest,
                        /*(*/
                            req,
                            &http_conn->conn_sender,
                            Memory(),
                             &http_conn->cur_msg_data
                        /*)*/)
                || !res)
            {
                http_conn->cur_handler = NULL;
            }

            if (http_conn->cur_msg_data && !req->hasBody())
                logW_ (_func, "msg_data is likely lost");

            http_conn->receiving_body = true;
        }
    }
}

void
HttpService::httpMessageBody (HttpRequest  * const mt_nonnull req,
			      Memory         const mem,
			      bool           const end_of_request,
			      Size         * const mt_nonnull ret_accepted,
			      void         * const  _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    if (!http_conn->cur_handler) {
	*ret_accepted = mem.len();
	return;
    }

    if (http_conn->cur_handler->preassembly
	&& http_conn->preassembled_len < http_conn->cur_handler->preassembly_limit)
    {
        // 'size' is how much we'are going to preassemble for this request.
	Size size = http_conn->cur_handler->preassembly_limit;
        if (req->getContentLengthSpecified() && req->getContentLength() < size)
            size = req->getContentLength();

	bool alloc_new = true;
	if (http_conn->preassembly_buf) {
	    if (http_conn->preassembly_buf_size >= size)
		alloc_new = false;
	    else
		delete[] http_conn->preassembly_buf;
	}

	if (alloc_new) {
	    http_conn->preassembly_buf = new (std::nothrow) Byte [size];
            assert (http_conn->preassembly_buf);
	    http_conn->preassembly_buf_size = size;
	}

	if (mem.len() + http_conn->preassembled_len >= size
            || end_of_request)
        {
            {
                Size tocopy = size - http_conn->preassembled_len;
                if (tocopy > mem.len())
                    tocopy = mem.len();

		memcpy (http_conn->preassembly_buf + http_conn->preassembled_len,
			mem.mem(),
                        tocopy);

		*ret_accepted = tocopy;
		http_conn->preassembled_len += tocopy;
            }

            if (http_conn->cur_handler->parse_body_params) {
                req->parseParameters (
                        Memory (http_conn->preassembly_buf,
                                http_conn->preassembled_len));
            }

            if (http_conn->cur_handler->cb && http_conn->cur_handler->cb->httpRequest) {
                Result res = Result::Failure;
                if (!http_conn->cur_handler->cb.call_ret<Result> (
                            &res,
                            http_conn->cur_handler->cb->httpRequest,
                            /*(*/
                                req,
                                &http_conn->conn_sender,
                                Memory (http_conn->preassembly_buf,
                                        http_conn->preassembled_len),
                                &http_conn->cur_msg_data
                            /*)*/)
                    || !res)
                {
                    http_conn->cur_handler = NULL;
                }
            }
            http_conn->receiving_body = true;

            if (http_conn->cur_handler) {
                if (*ret_accepted < mem.len()) {
                    Size accepted = 0;
                    Result res = Result::Failure;
                    if (!http_conn->cur_handler->cb.call_ret<Result> (
                                &res,
                                http_conn->cur_handler->cb->httpMessageBody,
                                /*(*/
                                    req,
                                    &http_conn->conn_sender,
                                    mem.region (*ret_accepted),
                                    end_of_request,
                                    &accepted,
                                    http_conn->cur_msg_data
                                /*)*/)
                        || !res)
                    {
                        http_conn->cur_handler = NULL;
                    }

                    *ret_accepted += accepted;
                } else {
                    if (end_of_request && !req->getContentLengthSpecified()) {
                        Size dummy_accepted = 0;
                        Result res = Result::Failure;
                        if (!http_conn->cur_handler->cb.call_ret<Result> (
                                    &res,
                                    http_conn->cur_handler->cb->httpMessageBody,
                                    /*(*/
                                        req,
                                        &http_conn->conn_sender,
                                        Memory(),
                                        true /* end_of_request */,
                                        &dummy_accepted,
                                        http_conn->cur_msg_data
                                    /*)*/)
                            || !res)
                        {
                            http_conn->cur_handler = NULL;
                        }
                    }
                }
            }

            if (!http_conn->cur_handler)
                *ret_accepted = mem.len();
	} else {
	    memcpy (http_conn->preassembly_buf + http_conn->preassembled_len,
		    mem.mem(),
		    mem.len());
	    *ret_accepted = mem.len();
	    http_conn->preassembled_len += mem.len();
	}

	return;
    }

    Result res = Result::Failure;
    if (!http_conn->cur_handler->cb.call_ret<Result> (
                &res,
                http_conn->cur_handler->cb->httpMessageBody,
                /*(*/
                    req,
                    &http_conn->conn_sender,
                    mem,
                    end_of_request,
                    ret_accepted,
                    http_conn->cur_msg_data
                /*)*/)
        || !res)
    {
	http_conn->cur_handler = NULL;
	*ret_accepted = mem.len();
    }
}

void
HttpService::doCloseHttpConnection (HttpConnection * const http_conn,
                                    HttpRequest    * const req)
{
#warning I presume that HttpServer/Service guarantees synchronization domain.
#warning But calling doCloseHttpConnection from the dtor violates the rule.
/* ^^^
   Решу это привязкой объектов к потокам. Простого решения нет.
   Очевидно, что в деструкторе нужно вызывать callback, который гарантирует
   контекст синхронизации. Единственный способ это сделать -
   вызывать callback из конкретного потока.
*/

    if (http_conn->cur_handler) {
        if (req && !http_conn->receiving_body) {
            void *dummy_msg_data = NULL;
            http_conn->cur_handler->cb.call (
                    http_conn->cur_handler->cb->httpRequest,
                    /*(*/
                        req,
                        &http_conn->conn_sender,
                        (http_conn->cur_handler->preassembly ?
                                Memory (http_conn->preassembly_buf,
                                        http_conn->preassembled_len)
                                : Memory()),
                        &dummy_msg_data
                    /*)*/);
            if (dummy_msg_data)
                logW_ (_func, "msg_data is likely lost");

            http_conn->receiving_body = true;
        }

        if (!req || (req->hasBody() && http_conn->receiving_body)) {
            Size dummy_accepted = 0;
            http_conn->cur_handler->cb.call (
                    http_conn->cur_handler->cb->httpMessageBody,
                    /*(*/
                        req,
                        &http_conn->conn_sender,
                        Memory(),
                        true /* end_of_request */,
                        &dummy_accepted,
                        http_conn->cur_msg_data
                    /*)*/);
        }

	http_conn->cur_handler = NULL;
	logD (http_service, _func, "http_conn->cur_handler: 0x", fmt_hex, (UintPtr) http_conn->cur_handler);
    }

    CodeDepRef<HttpService> const self = http_conn->weak_http_service;
    if (!self)
        return;

    self->destroyHttpConnection (http_conn);
}

void
HttpService::httpClosed (HttpRequest * const req,
                         Exception   * const exc_,
			 void        * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    logD_ (_func, "closed, http_conn 0x", fmt_hex, (UintPtr) http_conn);

    if (exc_)
	logE_ (_func, exc_->toString());

    logD (http_service, _func, "http_conn 0x", fmt_hex, (UintPtr) http_conn, ", "
          "refcount: ", fmt_def, http_conn->getRefCount(), ", "
          "cur_handler: 0x", fmt_hex, (UintPtr) http_conn->cur_handler);

    doCloseHttpConnection (http_conn, req);
}

bool
HttpService::acceptOneConnection ()
{
    HttpConnection * const http_conn = new (std::nothrow) HttpConnection;
    assert (http_conn);

    IpAddress client_addr;
    {
	TcpServer::AcceptResult const res = tcp_server.accept (&http_conn->tcp_conn,
							       &client_addr);
	if (res == TcpServer::AcceptResult::Error) {
	    http_conn->unref ();
	    logE_ (_func, exc->toString());
	    return false;
	}

	if (res == TcpServer::AcceptResult::NotAccepted) {
	    http_conn->unref ();
	    return false;
	}

	assert (res == TcpServer::AcceptResult::Accepted);
    }

    logD_ (_func, "accepted, http_conn 0x", fmt_hex, (UintPtr) http_conn, " client ", fmt_def, client_addr);

    http_conn->weak_http_service = this;

    http_conn->cur_handler = NULL;
    http_conn->cur_msg_data = NULL;
    http_conn->receiving_body = false;
    http_conn->preassembly_buf = NULL;
    http_conn->preassembly_buf_size = 0;
    http_conn->preassembled_len = 0;

    http_conn->conn_sender.init (deferred_processor);
    http_conn->conn_sender.setConnection (&http_conn->tcp_conn);
    http_conn->conn_receiver.init (&http_conn->tcp_conn,
                                   deferred_processor);

    http_conn->http_server.init (
            CbDesc<HttpServer::Frontend> (&http_frontend, http_conn, http_conn),
            &http_conn->conn_receiver,
            &http_conn->conn_sender,
            page_pool,
            client_addr);

    mutex.lock ();
    http_conn->pollable_key = poll_group->addPollable (http_conn->tcp_conn.getPollable());
    if (!http_conn->pollable_key) {
	mutex.unlock ();

	http_conn->unref ();

	logE_ (_func, exc->toString());
	return true;
    }

    if (keepalive_timeout_microsec > 0) {
	// TODO There should be a periodical checker routine which would
	// monitor connection's activity. Currently, this is an overly
	// simplistic oneshot cutter, like a ticking bomb for every client.
	http_conn->conn_keepalive_timer =
                timers->addTimer_microseconds (CbDesc<Timers::TimerCallback> (connKeepaliveTimerExpired,
                                                                              http_conn,
                                                                              http_conn),
                                               keepalive_timeout_microsec,
                                               false /* periodical */,
                                               false /* auto_delete */);
    }

    conn_list.append (http_conn);
    mutex.unlock ();

    http_conn->conn_receiver.start ();
    return true;
}

TcpServer::Frontend const HttpService::tcp_server_frontend = {
    accepted
};

void
HttpService::accepted (void *_self)
{
    HttpService * const self = static_cast <HttpService*> (_self);

    logD (http_service, _func_);

    for (;;) {
	if (!self->acceptOneConnection ())
	    break;
    }
}

mt_mutex (mutex) void
HttpService::addHttpHandler_rec (CbDesc<HttpHandler> const &cb,
				 ConstMemory   const path_,
				 bool                preassembly,
				 Size          const preassembly_limit,
				 bool          const parse_body_params,
				 Namespace   * const nsp)
{
    if (preassembly_limit == 0)
        preassembly = false;

    ConstMemory path = path_;
    if (path.len() > 0 && path.mem() [0] == '/')
	path = path.region (1);

    Byte const *delim = (Byte const *) memchr (path.mem(), '/', path.len());
    if (!delim) {
	HandlerEntry * const handler_entry = nsp->handler_hash.addEmpty (path).getDataPtr();
	handler_entry->cb = cb;
	handler_entry->preassembly = preassembly;
	handler_entry->preassembly_limit = preassembly_limit;
	handler_entry->parse_body_params = parse_body_params;
	return;
    }

    ConstMemory const next_nsp_name = path.region (0, delim - path.mem());
    Namespace::NamespaceHash::EntryKey const next_nsp_key =
	    nsp->namespace_hash.lookup (next_nsp_name);
    Namespace *next_nsp;
    if (next_nsp_key) {
	next_nsp = next_nsp_key.getData();
    } else {
	StRef<Namespace> const new_nsp = st_grab (new (std::nothrow) Namespace);
	nsp->namespace_hash.add (next_nsp_name, new_nsp);
	next_nsp = new_nsp;
    }

    return addHttpHandler_rec (cb,
			       path.region (delim - path.mem() + 1),
			       preassembly,
			       preassembly_limit,
			       parse_body_params,
			       next_nsp);
}

void
HttpService::addHttpHandler (CbDesc<HttpHandler> const &cb,
			     ConstMemory const path,
			     bool        const preassembly,
			     Size        const preassembly_limit,
			     bool        const parse_body_params)
{
//    logD_ (_func, "Adding handler for \"", path, "\"");

    mutex.lock ();
    addHttpHandler_rec (cb,
			path,
			preassembly,
			preassembly_limit,
			parse_body_params,
			&root_namespace); 
    mutex.unlock ();
}

mt_throws Result
HttpService::bind (IpAddress const &addr)
{
    if (!tcp_server.bind (addr))
	return Result::Failure;

    return Result::Success;
}

mt_throws Result
HttpService::start ()
{
    if (!tcp_server.listen ())
	return Result::Failure;

    // TODO Remove pollable when done.
    if (!poll_group->addPollable (tcp_server.getPollable()))
	return Result::Failure;

    if (!tcp_server.start ()) {
        logF_ (_func, "tcp_server.start() failed: ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

void
HttpService::setConfigParams (Time const keepalive_timeout_microsec,
                              bool const no_keepalive_conns)
{
    mutex.lock ();
    this->keepalive_timeout_microsec = keepalive_timeout_microsec;
    this->no_keepalive_conns = no_keepalive_conns;
    mutex.unlock ();
}

mt_throws Result
HttpService::init (PollGroup         * const mt_nonnull poll_group,
		   Timers            * const mt_nonnull timers,
                   DeferredProcessor * const mt_nonnull deferred_processor,
		   PagePool          * const mt_nonnull page_pool,
		   Time                const keepalive_timeout_microsec,
		   bool                const no_keepalive_conns)
{
    this->poll_group         = poll_group;
    this->timers             = timers;
    this->deferred_processor = deferred_processor;
    this->page_pool          = page_pool;

    this->keepalive_timeout_microsec = keepalive_timeout_microsec;
    this->no_keepalive_conns = no_keepalive_conns;

    if (!tcp_server.open ())
	return Result::Failure;

    tcp_server.init (CbDesc<TcpServer::Frontend> (&tcp_server_frontend, this, getCoderefContainer()),
                     deferred_processor,
                     timers);

    return Result::Success;
}

HttpService::HttpService (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      poll_group         (coderef_container),
      timers             (coderef_container),
      deferred_processor (coderef_container),
      page_pool          (coderef_container),
      keepalive_timeout_microsec (0),
      no_keepalive_conns (false),
      tcp_server (coderef_container)
{
}

HttpService::~HttpService ()
{
  StateMutexLock l (&mutex);

  // TODO Call remaining messageBody() callbacks to release callers' resources.

    ConnectionList::iter iter (conn_list);
    while (!conn_list.iter_done (iter)) {
	HttpConnection * const http_conn = conn_list.iter_next (iter);
	releaseHttpConnection (http_conn);
	http_conn->unref ();
    }
}

}

