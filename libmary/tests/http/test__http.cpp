#include <libmary/libmary.h>


using namespace M;

namespace {

DefaultPollGroup poll_group;

TcpServer tcp_server;

TcpConnection tcp_conn;
ConnectionReceiver conn_receiver (&tcp_conn);
// TODO ConnectionSender
HttpServer http_server;

void request (HttpRequest * const req,
	      void        * const /* cb_data */)
{
    logD_ (_func, req->getPath());

    // TEST
    tcp_conn.write ("HTTP/1.1 200 OK\r\nContent-length: 4\r\n\r\nFINE", NULL /* ret_nwritten */);
    tcp_conn.flush ();
    // TODO Allow for closing connections.
//    tcp_conn.close ();
}

void messageBody (HttpRequest * const req,
		  ConstMemory const &mem,
		  void        * const /* cb_data */)
{
    logD_ (_func, mem);
}

void closed (Exception * const exc,
	     void      * const /* cb_data */)
{
    logD_ (_func_);
}

HttpServer::Frontend const http_frontend = {
    request,
    messageBody,
    closed
};

void accepted (void * const)
{
    logD_ (_func_);

    {
	TcpServer::AcceptResult const res = tcp_server.accept (&tcp_conn);
	if (res == TcpServer::AcceptResult::Error) {
	    logE_ (_func, exc->toString ());
	    return;
	}

	if (res == TcpServer::NotAccepted)
	    return;

	assert (res == TcpServer::Accepted);
    }

    http_server.setFrontend (Cb<HttpServer::Frontend> (&http_frontend, NULL));
    conn_receiver.setFrontend (http_server.getReceiverFrontend ());

    if (!poll_group.addPollable (&tcp_conn))
	logE_ (_func, exc->toString());
	return;
    }
}

TcpServer::Frontend const tcp_server_frontend = {
    accepted
};

Result doTest ()
{
    if (!tcp_server.open ())
	goto exc_;
    tcp_server.setFrontend (&tcp_server_frontend, NULL);

    IpAddress addr;
    if (!setIpAddress (ConstMemory(), 8081, &addr)) {
	logE_ (_func, "setIpAddress() failed");
	return Result::Failure;
    }

    if (!tcp_server.bind (addr))
	goto exc_;
    if (!tcp_server.listen ())
	goto exc_;

    if (!poll_group.open ())
	goto exc_;
    if (!poll_group.addPollable (&tcp_server))
	goto exc_;

    for (;;) {
	if (!poll_group.poll ((Time) -1))
	    goto exc_;
    }

    return Result::Success;

exc_:
    logE_ (_func, exc->toString());
    return Result::Failure;
}

}

int main (void)
{
    libMaryInit ();

    if (!doTest ())
	return EXIT_FAILURE;

    return 0;
}

