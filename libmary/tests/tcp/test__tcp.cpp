#include <libmary/libmary.h>


using namespace M;

namespace {

TcpConnection *test_conn = NULL;
Timers test_timers;
Timers::TimerKey conn_timeout_key;

void testDoInput ()
{
    logD_ (_func_);

    Byte buf [4096];
    Size nread;
    for (;;) {
	AsyncIoResult const res = test_conn->read (Memory::forObject (buf), &nread);
	switch (res) {
	    case AsyncIoResult::Again:
		logD_ (_func, "read(): Again");
		return;
	    case AsyncIoResult::Eof:
		// TODO Quit program by breaking main loop.
		logD_ (_func, "read(): Eof");
		return;
	    case AsyncIoResult::Error:
		logE_ (_func, "read() failed: ", exc->toString());
		return;
	    case AsyncIoResult::Normal:
		break;
	    default:
		assert (0);
	}

	logD_ (_func, "read(): ", nread, " bytes");

	errs->print (ConstMemory (buf, nread));
    }
}

void testProcessInput (void * const /* cb_data */)
{
    logD_ (_func_);
    testDoInput ();
}

void testProcessError (Exception * const exc_,
		       void      * const /* cb_data */)
{
    if (exc_) {
	logD_ (_func, exc_->toString());
	return;
    }

    logD_ (_func_);
}

Connection::InputFrontend const input_frontend = {
    testProcessInput,
    testProcessError
};

void testProcessOutput (void * const /* cb_data */)
{
    logD_ (_func_);
}

Connection::OutputFrontend const output_frontend = {
    testProcessOutput
};

void testConnected (Exception * const exc_,
		    void      * const /* cb_data */)
{
    logD_ (_func_);

    test_timers.deleteTimer (conn_timeout_key);

    if (exc_) {
	logE_ (_func, exc_->toString());
	return;
    }

    testDoInput ();
}

void testConnectionTimeout (void * const /* cb_data */)
{
    logD_ (_func, "connection timeout");
    // TODO Release 'conn_timeout_key' timer.
//    test_conn->close ();
}

TcpConnection::Frontend tcp_frontend = {
    testConnected
};

Result doTest ()
{
  {
    updateTime ();

    TcpConnection conn (NULL);
    test_conn = &conn;
    conn.Connection::setInputFrontend (Cb<Connection::InputFrontend> (
	    &input_frontend, NULL /* cb_data */, NULL /* coderef_container */));
    conn.Connection::setOutputFrontend (Cb<Connection::OutputFrontend> (
	    &output_frontend, NULL /* cb_data */, NULL /* coderef_container */));
    conn.setFrontend (Cb<TcpConnection::Frontend> (
	    &tcp_frontend, NULL /* frontend_cb_data */, NULL /* coderef_container */));

    IpAddress addr;
    if (!setIpAddress ("127.0.0.1:8080", &addr)) {
	logE_ (_func, "setIpAddress() failed");
	return Result::Failure;
    }

    // TODO TcpConnection::connect() API has changed.
    if (!conn.connect (addr))
	goto exc_;

    conn_timeout_key = test_timers.addTimer (testConnectionTimeout, NULL, NULL, 5 /* time_seconds */);

    DefaultPollGroup poll_group (NULL);
    if (!poll_group.open ())
	goto exc_;

    if (!poll_group.addPollable (conn.getPollable()))
	goto exc_;

    for (;;) {
	if (!poll_group.poll (test_timers.getSleepTime_microseconds ()))
	    goto exc_;

	updateTime ();
	test_timers.processTimers ();
    }

    return Result::Success;
  }

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

