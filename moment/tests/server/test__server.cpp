#include <moment/libmoment.h>


using namespace M;
using namespace Moment;


namespace {

DefaultPollGroup poll_group;
Timers timers;

PagePool page_pool (4096 /* page_size */, 128 /* min_pages */);

TcpServer tcp_server;

// TODO Support several simultaneous connections
TcpConnection tcp_conn;
RtmpConnection rtmp_conn (&timers, &page_pool);
RtmpDirectBackend rtmp_be (&rtmp_conn, &tcp_conn);
RtmpServer rtmp_server (&rtmp_conn);

void accepted (void * const)
{
    logD_ (_func_);

    for (;;) {
	{
	    TcpServer::AcceptResult const res = tcp_server.accept (&tcp_conn);
	    if (res == TcpServer::AcceptResult::Error) {
		logE_ (_func, "accept() failed: ", exc->toString ());
		return;
	    }

	    if (res == TcpServer::AcceptResult::NotAccepted)
		break;

	    assert (res == TcpServer::AcceptResult::Accepted);
	}

	rtmp_conn.setWritevFd (tcp_conn.getFd ());

	if (!poll_group.addPollable (&tcp_conn)) {
	    logE_ (_func, "addPollable() failed: ", exc->toString ());
	    return;
	}

	rtmp_conn.startServer ();
    }
}

TcpServer::Frontend const tcp_server_frontend = {
    accepted
};

void
doTimerIteration ()
{
    updateTime ();
    timers.processTimers ();
}

void
pollIterationBegin (void * const)
{
    logD_ (_func_);
    doTimerIteration ();
}

void
pollIterationEnd (void * const)
{
    logD_ (_func_);
    RtmpConnection::pollIterationEnd ();
}

PollGroup::Frontend const poll_frontend = {
    pollIterationBegin,
    pollIterationEnd
};

Result
doTest ()
{
    updateTime ();

    if (!tcp_server.open ()) {
	logE_ (_func, "TcpServer::open() failed: ", exc->toString ());
	return Result::Failure;
    }

    tcp_server.setFrontend (&tcp_server_frontend, NULL /* frontend_cb_data */);

    IpAddress addr;
    if (!setIpAddress (ConstMemory(), 8080, &addr)) {
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

    poll_group.setFrontend (&poll_frontend, NULL /* cb_data */);

    for (;;) {
	if (!poll_group.poll (timers.getSleepTime_microseconds ()))
	    goto exc_;

	doTimerIteration ();
    }

    return Result::Success;

exc_:
    logE_ (_func, exc->toString());
    return Result::Failure;
}

}

class Foo
{
public:
    static void bar ()
    {
	errs->print (__PRETTY_FUNCTION__).print ("\n");
	errs->print (__func__).print ("\n");
	errs->print (__FUNCTION__).print ("\n");
    }
};

int main (void)
{
    libMaryInit ();

    Foo::bar ();

    if (!doTest ())
	return EXIT_FAILURE;

    return 0;
}

