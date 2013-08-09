#include <libmary/libmary.h>


using namespace M;

namespace {

TcpServer test_srv (NULL);
TcpConnection test_conn (NULL);
DefaultPollGroup test_poll_group (NULL);
Timers test_timers;

void testProcessInput (void * const /* cb_data */)
{
    logD_ (_func);
}

void testProcessError (Exception * const exc_,
		       void      * const /* cb_data */)
{
    if (exc_) {
	logD_ (_func, exc_->toString());
	return;
    }

    logD_ (_func);
}

Connection::InputFrontend const input_frontend = {
    testProcessInput,
    testProcessError
};

void testProcessOutput (void * const /* cb_data */)
{
    logD_ (_func);
}

Connection::OutputFrontend const output_frontend = {
    testProcessOutput
};

void testAccepted (void * const /* cb_data */)
{
    logD_ (_func_);

    for (;;) {
	{
	    TcpServer::AcceptResult const res = test_srv.accept (&test_conn);
	    if (res == TcpServer::AcceptResult::Error) {
		logE_ (_func, "exception: ", exc->toString());
		return;
	    }

	    if (res == TcpServer::AcceptResult::NotAccepted)
		return;

	    assert (res == TcpServer::AcceptResult::Accepted);
	}

	test_conn.Connection::setInputFrontend (Cb<Connection::InputFrontend> (
		&input_frontend, NULL /* cb_data */, NULL /* coderef_container */));
	test_conn.Connection::setOutputFrontend (Cb<Connection::OutputFrontend> (
		&output_frontend, NULL /* cb_data */, NULL /* coderef_container */));

	if (!test_poll_group.addPollable (test_conn.getPollable())) {
	    logE_ (_func, "addPollable() failed: ", exc->toString());
	    return;
	}

	// Ignoring errors.
	test_conn.write ("Hello, Friend!", NULL /* nwritten */);

	// TODO Set timeout, test timeouts.
    }
}

TcpServer::Frontend const srv_frontend = {
    testAccepted
};

void
doTimerIteration ()
{
    updateTime ();
    test_timers.processTimers ();
}

void
test_iteration_callback (void * const /* cb_data */)
{
    // TODO Поставили новый таймер асинхронно, а poll_group висит в более длинном таймауте: ?
    // => нужен trigger()
    // ...нужен callback "установлен новый таймер", и проверять, нужно ли делать trigger()
    // для этого таймера. --- достаточно вызывать callback только при установке таймера,
    // который должен сработать раньше, чем все существующие. И нужно ли при этом делать
    // trigger() - вопрос (...можно решить через thread-local storage).

    logD_ (_func_);
    doTimerIteration ();
}

void
test_timer_callback (void * const /* cb_data */)
{
    static unsigned n = 1;
    logD_ (_func, n);
    ++n;
}

SelectPollGroup::Frontend const poll_frontend = {
    test_timer_callback /* pollIterationBegin */,
    NULL /* pollIterationEnd */
};

Result doTest ()
{
    updateTime ();

    test_timers.addTimer (test_timer_callback, NULL, NULL, 1 /* time_seconds */, true /* periodical */);

    if (!test_srv.open ()) {
	logE_ (_func, "TcpServer::open() failed: ", exc->toString());
	return Result::Failure;
    }

    test_srv.setFrontend (Cb<TcpServer::Frontend> (
	    &srv_frontend, NULL /* cb_data */, NULL /* coderef_container */));

    IpAddress addr;
    if (!setIpAddress (ConstMemory(), 8080, &addr)) {
	logE_ (_func, "setIpAddress() failed");
	return Result::Failure;
    }

    if (!test_srv.bind (addr))
	goto exc_;

    if (!test_srv.listen ())
	goto exc_;

    if (!test_poll_group.open ())
	goto exc_;

    if (!test_poll_group.addPollable (test_srv.getPollable()))
	goto exc_;

    test_poll_group.setFrontend (Cb<SelectPollGroup::Frontend> (
	    &poll_frontend, NULL /* cb_data */, NULL /* coderef_container */));

    for (;;) {
	logD_ (_func, "sleep time: ", (Int64) test_timers.getSleepTime_microseconds ());
	if (!test_poll_group.poll (test_timers.getSleepTime_microseconds ()))
	    goto exc_;

	doTimerIteration ();
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

