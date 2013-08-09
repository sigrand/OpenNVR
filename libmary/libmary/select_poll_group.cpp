/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011-2013 Dmitry Shatrov

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


#include <libmary/types.h>

#include <sys/time.h>
#include <sys/types.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef LIBMARY_PLATFORM_WIN32
#include <sys/select.h>
#endif

#include <libmary/log.h>
#include <libmary/util_str.h>
#include <libmary/util_time.h>
#include <libmary/util_posix.h>

#include <libmary/select_poll_group.h>


// Windows version of SelectPollGroup is no more since introduction of trigger pipe.
// A pair of TCP sockets could be used instead of pipe() on win32. Not implemented.


namespace M {

static LogGroup libMary_logGroup_select  ("select",         LogLevel::N);
static LogGroup libMary_logGroup_time    ("select_time",    LogLevel::N);

PollGroup::Feedback const SelectPollGroup::pollable_feedback = {
    requestInput,
    requestOutput
};

void
SelectPollGroup::requestInput (void * const _pollable_entry)
{
    logD (select, _func_);

    PollableEntry * const pollable_entry = static_cast <PollableEntry*> (_pollable_entry);
    // We assume that the poll group is always available when requestInput() is called.
    SelectPollGroup * const self = pollable_entry->select_poll_group;

    if (self->poll_tlocal && self->poll_tlocal == libMary_getThreadLocal()) {
        // No need to lock 'mutex', because 'need_input' is read from the same thread only.
	pollable_entry->need_input = true;
    } else {
	self->mutex.lock ();
	pollable_entry->need_input = true;
        if (! mt_unlocks (mutex) self->doTrigger ())
            logF_ (_func, "doTrigger() failed: ", exc->toString());
    }
}

void
SelectPollGroup::requestOutput (void * const _pollable_entry)
{
    logD (select, _func_);

    PollableEntry * const pollable_entry = static_cast <PollableEntry*> (_pollable_entry);
    // We assume that the poll group is always available when requestOutput() is called.
    SelectPollGroup * const self = pollable_entry->select_poll_group;

    if (self->poll_tlocal && self->poll_tlocal == libMary_getThreadLocal()) {
        // No need to lock 'mutex', because 'need_output' is read from the same thread only.
	pollable_entry->need_output = true;
    } else {
	self->mutex.lock ();
	pollable_entry->need_output = true;
        if (! mt_unlocks (mutex) self->doTrigger ())
            logF_ (_func, "doTrigger() failed: ", exc->toString());
    }
}

// The pollable should be available for unsafe callbacks while this method is called.
mt_throws PollGroup::PollableKey
SelectPollGroup::addPollable (CbDesc<Pollable> const &pollable,
			      bool const activate)
{
    PollableEntry * const pollable_entry = new (std::nothrow) PollableEntry;
    assert (pollable_entry);

    logD (select, _func, "cb_data: 0x", fmt_hex, (UintPtr) pollable.cb_data, ", "
	  "pollable_entry: 0x", fmt_hex, (UintPtr) pollable_entry);

    pollable_entry->select_poll_group = this;
    pollable_entry->valid = true;
    pollable_entry->activated = activate;
    pollable_entry->pollable = pollable;
    // We're making an unsafe call, assuming that the pollable is available.
    pollable_entry->fd = pollable->getFd (pollable.cb_data);
    pollable_entry->need_input = true;
    pollable_entry->need_output = true;

    // We're making an unsafe call, assuming that the pollable is available.
    //
    // We're counting on the fact that the poll group will always be available
    // when pollable_feedback callbacks are called - that's why we use NULL
    // for coderef_container.
    pollable->setFeedback (
	    Cb<Feedback> (&pollable_feedback, pollable_entry, NULL /* coderef_container */),
	    pollable.cb_data);

    mutex.lock ();
    if (activate)
	pollable_list.append (pollable_entry);
    else
	inactive_pollable_list.append (pollable_entry);

    if (activate
	&& !(poll_tlocal && poll_tlocal == libMary_getThreadLocal()))
    {
	if (!mt_unlocks (mutex) doTrigger ())
	    logF_ (_func, "doTrigger() failed: ", exc->toString());
    } else {
	mutex.unlock ();
    }

    return static_cast <void*> (pollable_entry);
}

mt_throws Result
SelectPollGroup::activatePollable (PollableKey const mt_nonnull key)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> ((void*) key);

    mutex.lock ();
    assert (!pollable_entry->activated);
    pollable_entry->activated = true;
    inactive_pollable_list.remove (pollable_entry);
    pollable_list.append (pollable_entry);

    if (!(poll_tlocal && poll_tlocal == libMary_getThreadLocal()))
	return mt_unlocks (mutex) doTrigger ();

    mutex.unlock ();
    return Result::Success;
}

void
SelectPollGroup::removePollable (PollableKey const mt_nonnull key)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> ((void*) key);

    logD (select, _func, "pollable_entry: 0x", fmt_hex, (UintPtr) pollable_entry);

    mutex.lock ();
    pollable_entry->valid = false;
    if (pollable_entry->activated) {
	pollable_list.remove (pollable_entry);
    } else {
	inactive_pollable_list.remove (pollable_entry);
    }
    pollable_entry->unref ();
    mutex.unlock ();
}

mt_throws Result
SelectPollGroup::poll (Uint64 const timeout_microsec)
{
    Time const start_microsec = getTimeMicroseconds ();

    logD (time, _func, fmt_hex, "start_microsec: 0x", start_microsec);

    Result ret_res = Result::Success;

    SelectedList selected_list;
    fd_set rfds, wfds, efds;
    for (;;) {
	logD (select, _func, "iteration");

	FD_ZERO (&rfds);
	FD_ZERO (&wfds);
	FD_ZERO (&efds);

	int largest_fd = trigger_pipe [0];
	FD_SET (trigger_pipe [0], &rfds);
	// We hope for the better and don't subscribe for errors on trigger_pipe[*].

	selected_list.clear ();

#ifdef LIBMARY_TRACE_SELECT_FDS
	Count num_rfds = 0;
	Count num_wfds = 0;
	Count num_efds = 0;
#endif
	{
	  StateMutexLock l (&mutex);

	    PollableList::iter iter (pollable_list);
	    while (!pollable_list.iter_done (iter)) {
		PollableEntry * const pollable_entry = pollable_list.iter_next (iter);

		if (pollable_entry->fd >= (int) FD_SETSIZE
                    || pollable_entry->fd < 0)
                {
		  // The log explodes because of this line.
		  // select() is useless when we've got many clients connected.
		    logW_ (_func, "fd ", pollable_entry->fd, " is larger than FD_SETSIZE (", FD_SETSIZE, "). "
			   "This fd will not be polled");
		    continue;
		}

		if (pollable_entry->fd > largest_fd)
		    largest_fd = pollable_entry->fd;

		FD_SET (pollable_entry->fd, &efds);
#ifdef LIBMARY_TRACE_SELECT_FDS
		++num_efds;
#endif
		if (pollable_entry->need_input) {
		    logD (select, _func, "adding pollable_entry 0x", fmt_hex, (UintPtr) pollable_entry, " to rfds");
		    FD_SET (pollable_entry->fd, &rfds);

#ifdef LIBMARY_TRACE_SELECT_FDS
		    ++num_rfds;
#endif
		}

		if (pollable_entry->need_output) {
		    logD (select, _func, "adding pollable_entry 0x", fmt_hex, (UintPtr) pollable_entry, " to wfds");
		    FD_SET (pollable_entry->fd, &wfds);

#ifdef LIBMARY_TRACE_SELECT_FDS
		    ++num_wfds;
#endif
		}

		selected_list.append (pollable_entry);
		pollable_entry->ref ();
	    }
	}
#ifdef LIBMARY_TRACE_SELECT_FDS
	logD_ (_func, "rfds: ", num_rfds, ", wfds: ", num_wfds, ", efds: ", num_efds);
#endif

	Time elapsed_microsec;
	int nfds = 0;
	{
	    Time const cur_microsec = getTimeMicroseconds ();

	    // vvv assert (cur_microsec >= start_microsec);
	    if (cur_microsec < start_microsec) {
		logF_ (_func, fmt_hex, "cur_microsec: ", cur_microsec, ", start_microsec: ", start_microsec);
		unreachable ();
	    }

	    elapsed_microsec = cur_microsec - start_microsec;

	    logD (select, _func,
                  "timeout_microsec: ",
                          timeout_microsec == (Uint64) -1 ? toString ("-1")
                                                          : toString (timeout_microsec),
                          ", "
		  "elapsed_microsec: ", elapsed_microsec);

	    bool null_timeout = true;
	    struct timeval timeout_val;
	    if (!got_deferred_tasks) {
		if (timeout_microsec != (Uint64) -1) {
		    null_timeout = false;
		    if (timeout_microsec > elapsed_microsec) {
			timeout_val.tv_sec  = (timeout_microsec - elapsed_microsec) / 1000000;
			timeout_val.tv_usec = (timeout_microsec - elapsed_microsec) % 1000000;

			logD (select, _func, "tv_sec: ", timeout_val.tv_sec, ", tv_usec: ", timeout_val.tv_usec);
		    } else {
			timeout_val.tv_sec = 0;
			timeout_val.tv_usec = 0;
		    }
		}
	    } else {
		logD (select, "got deferred tasks");
		null_timeout = false;
		timeout_val.tv_sec = 0;
		timeout_val.tv_usec = 0;
	    }

            mutex.lock ();
            if (triggered
                || (!null_timeout && timeout_val.tv_sec == 0 && timeout_val.tv_usec == 0))
            {
                block_trigger_pipe = true;

		null_timeout = false;
		timeout_val.tv_sec = 0;
		timeout_val.tv_usec = 0;
            } else {
                block_trigger_pipe = false;
            }
            mutex.unlock ();

	    nfds = select (largest_fd + 1, &rfds, &wfds, &efds, null_timeout ? NULL : &timeout_val);
#ifdef LIBMARY_PLATFORM_WIN32
            if (nfds == SOCKET_ERROR)
#else
	    if (nfds == -1)
#endif
            {
#ifdef LIBMARY_PLATFORM_WIN32
                int const wsa_error_code = WSAGetLastError();
                exc_throw (WSAException, wsa_error_code);
                logE_ (_func, "select() failed: ", wsaErrorToString (wsa_error_code));
#else
		if (errno == EINTR) {
		    SelectedList::iter iter (selected_list);
		    mutex.lock ();
		    while (!selected_list.iter_done (iter)) {
			PollableEntry * const pollable_entry = selected_list.iter_next (iter);
			pollable_entry->unref ();
		    }
		    mutex.unlock ();

		    // Re-initializing rfds, wfds, efds.
		    continue;
		}

		exc_throw (PosixException, errno);
		exc_push (InternalException, InternalException::BackendError);
		logE_ (_func, "select() failed: ", errnoString (errno));
#endif
		ret_res = Result::Failure;
		goto _select_interrupted;
	    } else
	    if (nfds < 0) {
		logE_ (_func, "unexpected return value from select(): ", nfds);
		exc_throw (InternalException, InternalException::BackendMalfunction);
		ret_res = Result::Failure;
		goto _select_interrupted;
	    }
	}

        mutex.lock ();
        block_trigger_pipe = true;
        bool const was_triggered = triggered;
        triggered = false;
        mutex.unlock ();

        got_deferred_tasks = false;

	if (frontend)
	    frontend.call (frontend->pollIterationBegin);

	// This condition is wrong because we must iterate over selected_list
	// to unref pollable entries in any case.
	/* if (nfds > 0) */ {
	    mutex.lock ();

	    SelectedList::iter iter (selected_list);
	    while (!selected_list.iter_done (iter)) {
		PollableEntry * const pollable_entry = selected_list.iter_next (iter);

		if (nfds > 0 &&
		    pollable_entry->valid)
		{
		    Uint32 event_flags = 0;

		    if (FD_ISSET (pollable_entry->fd, &rfds)) {
			pollable_entry->need_input = false;
			event_flags |= PollGroup::Input;
			logD (select, _func, "Input");
		    }

		    if (FD_ISSET (pollable_entry->fd, &wfds)) {
			pollable_entry->need_output = false;
			event_flags |= PollGroup::Output;
			logD (select, _func, "Output");
		    }

		    if (FD_ISSET (pollable_entry->fd, &efds)) {
			event_flags |= PollGroup::Error;
			logD (select, _func, "Error");
		    }

		    if (event_flags) {
			mutex.unlock ();
			pollable_entry->pollable.call (pollable_entry->pollable->processEvents, /*(*/ event_flags /*)*/);
			mutex.lock ();
		    }
		}

		pollable_entry->unref ();
	    } /* while (!iter_done) */

	    mutex.unlock ();
	}

	if (frontend) {
	    bool extra_iteration_needed = false;
	    frontend.call_ret (&extra_iteration_needed, frontend->pollIterationEnd);
	    if (extra_iteration_needed)
		got_deferred_tasks = true;
	}

        if (FD_ISSET (trigger_pipe [0], &rfds)) {
            if (!commonTriggerPipeRead (trigger_pipe [0])) {
                logF_ (_func, "commonTriggerPipeRead() failed: ", exc->toString());
                return Result::Failure;
            }
            break;
        }

        if (was_triggered)
            break;

	if (elapsed_microsec >= timeout_microsec) {
	  // Timeout expired.
	    break;
	}
    } // for (;;)

    return ret_res;

_select_interrupted:
    SelectedList::iter iter (selected_list);
    mutex.lock ();
    while (!selected_list.iter_done (iter)) {
	PollableEntry * const pollable_entry = selected_list.iter_next (iter);
	pollable_entry->unref ();
    }
    mutex.unlock ();

    return ret_res;
}

mt_unlocks (mutex) mt_throws Result
SelectPollGroup::doTrigger ()
{
    if (triggered) {
	mutex.unlock ();
	return Result::Success;
    }
    triggered = true;

    if (block_trigger_pipe) {
        mutex.unlock ();
        return Result::Success;
    }
    mutex.unlock ();

    return commonTriggerPipeWrite (trigger_pipe [1]);
}

mt_throws Result
SelectPollGroup::trigger ()
{
    logD (select, _func_);

    if (poll_tlocal && poll_tlocal == libMary_getThreadLocal()) {
	mutex.lock ();
	triggered = true;
	mutex.unlock ();
	return Result::Success;
    }

    mutex.lock ();
    return mt_unlocks (mutex) doTrigger ();
}

mt_throws Result
SelectPollGroup::open ()
{
    logD (select, _func_);

    if (!posix_createNonblockingPipe (&trigger_pipe))
	return Result::Failure;

    return Result::Success;
}

SelectPollGroup::SelectPollGroup (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      poll_tlocal (NULL),
      triggered (false),
      block_trigger_pipe (true),
      // Initializing to 'true' to process deferred tasks scheduled before we
      // enter poll() for the first time.
      got_deferred_tasks (true)
{
    trigger_pipe [0] = -1;
    trigger_pipe [1] = -1;
}

SelectPollGroup::~SelectPollGroup ()
{
    logD (select, _func_);

    mutex.lock ();
    {
	PollableList::iter iter (pollable_list);
	while (!pollable_list.iter_done (iter)) {
	    PollableEntry * const pollable_entry = pollable_list.iter_next (iter);
	    assert (pollable_entry->activated);
	    delete pollable_entry;
	}
    }

    {
	PollableList::iter iter (inactive_pollable_list);
	while (!inactive_pollable_list.iter_done (iter)) {
	    PollableEntry * const pollable_entry = inactive_pollable_list.iter_next (iter);
	    assert (!pollable_entry->activated);
	    delete pollable_entry;
	}
    }
    mutex.unlock ();

    for (int i = 0; i < 2; ++i) {
	if (trigger_pipe [i] != -1) {
	    for (;;) {
		int const res = close (trigger_pipe [i]);
		if (res == -1) {
		    if (errno == EINTR)
			continue;

		    logE_ (_func, "trigger_pipe[", i, "]: close() failed: ", errnoString (errno));
		} else
		if (res != 0) {
		    logE_ (_func, "trigger_pipe[", i, "]: close(): unexpected return value: ", res);
		}

		break;
	    }
	}
    }
}

}

