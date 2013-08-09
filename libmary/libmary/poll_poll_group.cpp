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
#include <errno.h>
#include <unistd.h>
#include <sys/poll.h>

#include <libmary/util_posix.h>
#include <libmary/log.h>

#include <libmary/poll_poll_group.h>


namespace M {

void
PollPollGroup::requestInput (void * const _pollable_entry)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> (_pollable_entry);
    // We assume that the poll group is always available when requestInput()
    // is called.
    PollPollGroup * const self = pollable_entry->poll_poll_group;

    if (self->poll_tlocal && self->poll_tlocal == libMary_getThreadLocal()) {
	pollable_entry->need_input = true;
    } else {
	self->mutex.lock ();
	pollable_entry->need_input = true;
        if (! mt_unlocks (mutex) self->doTrigger ())
            logF_ (_func, "doTrigger() failed: ", exc->toString());
    }
}

void
PollPollGroup::requestOutput (void * const _pollable_entry)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> (_pollable_entry);
    // We assume that the poll group is always available when requestOutput()
    // is called.
    PollPollGroup * const self = pollable_entry->poll_poll_group;

    if (self->poll_tlocal && self->poll_tlocal == libMary_getThreadLocal()) {
	pollable_entry->need_output = true;
    } else {
	self->mutex.lock ();
	pollable_entry->need_output = true;
        if (! mt_unlocks (mutex) self->doTrigger ())
            logF_ (_func, "doTrigger() failed: ", exc->toString());
    }
}

PollGroup::Feedback const PollPollGroup::pollable_feedback = {
    requestInput,
    requestOutput
};

// The pollable should be available for unsafe callbacks while this method is called.
mt_throws PollGroup::PollableKey
PollPollGroup::addPollable (CbDesc<Pollable> const &pollable,
			    bool activate)
{
    PollableEntry * const pollable_entry = new PollableEntry;

    pollable_entry->poll_poll_group = this;
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

    ++num_pollables;

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
PollPollGroup::activatePollable (PollableKey const mt_nonnull key)
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
PollPollGroup::removePollable (PollableKey const mt_nonnull key)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> ((void*) key);

    mutex.lock ();

    pollable_entry->valid = false;
    if (pollable_entry->activated)
	pollable_list.remove (pollable_entry);
    else
	inactive_pollable_list.remove (pollable_entry);

    pollable_entry->unref ();
    --num_pollables;

    mutex.unlock ();
}

mt_throws Result
PollPollGroup::poll (Uint64 const timeout_microsec)
{
    Time const start_microsec = getTimeMicroseconds ();

    Result ret_res = Result::Success;

    SelectedList selected_list;
    for (;;) {
	selected_list.clear ();

	Count cur_num_pollables = 1;
	struct pollfd pollfds [num_pollables];

	pollfds [0].fd = trigger_pipe [0];
	pollfds [0].events = POLLIN;

	{
	    mutex.lock ();
	    PollableList::iter iter (pollable_list);
	    while (!pollable_list.iter_done (iter)) {
		PollableEntry * const pollable_entry = pollable_list.iter_next (iter);
		selected_list.append (pollable_entry);
		pollfds [cur_num_pollables].fd = pollable_entry->fd;

		pollfds [cur_num_pollables].events =
#ifdef __linux__
			POLLRDHUP;
#else
			0;
#endif

		if (pollable_entry->need_input)
		    pollfds [cur_num_pollables].events |= POLLIN;

		if (pollable_entry->need_output)
		    pollfds [cur_num_pollables].events |= POLLOUT;

		pollable_entry->ref ();
		++cur_num_pollables;
	    }
	    assert (cur_num_pollables == num_pollables + 1);
	    mutex.unlock ();
	}

	Time elapsed_microsec;
	int nfds;
	{
	    Time cur_microsec = getTimeMicroseconds ();
	    if (cur_microsec < start_microsec)
		cur_microsec = start_microsec;

	    elapsed_microsec = cur_microsec - start_microsec;

            int timeout;
            if (!got_deferred_tasks) {
                if (timeout_microsec != (Uint64) -1) {
                    if (timeout_microsec > elapsed_microsec) {
                        Uint64 const tmp_timeout = (timeout_microsec - elapsed_microsec) / 1000;
                        timeout = (int) tmp_timeout;
                        if ((Uint64) timeout != tmp_timeout || timeout < 0)
                            timeout = Int_Max - 1;

                        if (timeout == 0)
                            timeout = 1;
                    } else {
                        timeout = 0;
                    }
                } else {
                    timeout = -1;
                }
            } else {
                timeout = 0;
            }

            mutex.lock ();
            if (triggered || timeout == 0) {
                block_trigger_pipe = true;
                timeout = 0;
            } else {
                block_trigger_pipe = false;
            }
            mutex.unlock ();

	    nfds = ::poll (pollfds, cur_num_pollables, timeout);
	    if (nfds == -1) {
		if (errno == EINTR) {
		    SelectedList::iter iter (selected_list);
		    mutex.lock ();
		    while (!selected_list.iter_done (iter)) {
			PollableEntry * const pollable_entry = selected_list.iter_next (iter);
			pollable_entry->unref ();
		    }
		    mutex.unlock ();

		    // Re-initializing pollfds.
		    continue;
		}
	    } else
	    if (nfds < 0) {
		logE_ (_func, "unexpected return value from poll(): ", nfds);
		exc_throw (InternalException, InternalException::BackendMalfunction);
		ret_res = Result::Failure;
		goto _poll_interrupted;
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

	bool trigger_pipe_ready = false;
	{
	    if (pollfds [0].revents & (POLLIN | POLLERR | POLLHUP
#ifdef __linux__
			| POLLRDHUP
#endif
			))
		trigger_pipe_ready = true;

	    mutex.lock ();

	    SelectedList::iter iter (selected_list);
	    Count i = 1;
	    while (!selected_list.iter_done (iter)) {
		PollableEntry * const pollable_entry = selected_list.iter_next (iter);

		if (nfds > 0 &&
		    pollable_entry->valid)
		{
		    Uint32 event_flags = 0;

		    if (pollfds [i].revents & POLLNVAL) {
			logW_ (_func, "POLLNVAL for pollable_entry "
			       "0x", fmt_hex, (UintPtr) pollable_entry, ", "
			       "fd ", pollable_entry->fd, " (", pollfds [i].fd, ")");
			++i;
			continue;
		    }

		    if (pollfds [i].revents & POLLIN) {
			pollable_entry->need_input = false;
			event_flags |= PollGroup::Input;
		    }

		    if (pollfds [i].revents & POLLOUT) {
			pollable_entry->need_output = false;
			event_flags |= PollGroup::Output;
		    }

		    if (pollfds [i].revents & POLLHUP
#ifdef __linux__
			|| pollfds [i].revents & POLLRDHUP
#endif
			)
		    {
			event_flags |= PollGroup::Hup;
		    }

		    if (pollfds [i].revents & POLLERR) {
			event_flags |= PollGroup::Error;
		    }

		    if (event_flags) {
			mutex.unlock ();
			pollable_entry->pollable.call (pollable_entry->pollable->processEvents, /*(*/ event_flags /*)*/);
			mutex.lock ();
		    }
		}

		pollable_entry->unref ();
		++i;
	    }
	    assert (i == cur_num_pollables);

	    mutex.unlock ();
	}

	if (frontend) {
	    bool extra_iteration_needed = false;
	    frontend.call_ret (&extra_iteration_needed, frontend->pollIterationEnd);
	    if (extra_iteration_needed)
		got_deferred_tasks = true;
	}

        if (trigger_pipe_ready) {
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
    }

    return ret_res;

_poll_interrupted:
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
PollPollGroup::doTrigger ()
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
PollPollGroup::trigger ()
{
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
PollPollGroup::open ()
{
    if (!posix_createNonblockingPipe (&trigger_pipe))
	return Result::Failure;

    return Result::Success;
}

PollPollGroup::PollPollGroup (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      poll_tlocal (NULL),
      num_pollables (0),
      triggered (false),
      block_trigger_pipe (true),
      // Initializing to 'true' to process deferred tasks scheduled before we
      // enter poll() for the first time.
      got_deferred_tasks (true)
{
    trigger_pipe [0] = -1;
    trigger_pipe [1] = -1;
}

PollPollGroup::~PollPollGroup ()
{
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

