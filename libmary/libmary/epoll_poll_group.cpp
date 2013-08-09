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
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>

#include <libmary/util_posix.h>
#include <libmary/log.h>

#include <libmary/epoll_poll_group.h>


//#define LIBMARY__EPOLL_POLL_GROUP__DEBUG


namespace M {

static LogGroup libMary_logGroup_epoll ("epoll", LogLevel::I);

mt_mutex (mutex) void
EpollPollGroup::processPollableDeletionQueue ()
{
    PollableDeletionQueue::iter iter (pollable_deletion_queue);
    while (!pollable_deletion_queue.iter_done (iter)) {
	PollableEntry * const pollable_entry = pollable_deletion_queue.iter_next (iter);
	delete pollable_entry;
    }
    pollable_deletion_queue.clear ();
}

mt_throws PollGroup::PollableKey
EpollPollGroup::addPollable (CbDesc<Pollable> const &pollable,
			     bool const activate)
{
    PollableEntry * const pollable_entry = new (std::nothrow) PollableEntry;
    assert (pollable_entry);

    logD (epoll, _func, "0x", fmt_hex, (UintPtr) this, ": "
	  "pollable_entry: 0x", fmt_hex, (UintPtr) pollable_entry, ", "
	  "cb_data: 0x", fmt_hex, (UintPtr) pollable.cb_data);

    pollable_entry->epoll_poll_group = this;
    pollable_entry->pollable = pollable;
    // We're making an unsafe call, assuming that the pollable is available.
    pollable_entry->fd = pollable->getFd (pollable.cb_data);
    pollable_entry->valid = true;

    mutex.lock ();
    pollable_list.append (pollable_entry);
    mutex.unlock ();

    if (activate) {
	if (!doActivate (pollable_entry))
	    goto _failure;
    }

    return pollable_entry;

_failure:
    mutex.lock ();
    pollable_list.remove (pollable_entry);
    mutex.unlock ();
    delete pollable_entry;

    return NULL;
}

mt_throws Result
EpollPollGroup::doActivate (PollableEntry * const mt_nonnull pollable_entry)
{
    logD (epoll, _func, "0x", fmt_hex, (UintPtr) this, ": "
	  "pollable_entry: 0x", fmt_hex, (UintPtr) pollable_entry);

    struct epoll_event event;
    event.events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP;
    event.data.u64 = 0; // For valgrind.
    event.data.ptr = pollable_entry;
    int const res = epoll_ctl (efd, EPOLL_CTL_ADD, pollable_entry->fd, &event);
    if (res == -1) {
	exc_throw (PosixException, errno);
	logE_ (_func, "epoll_ctl() failed: ", errnoString (errno));
	return Result::Failure;
    }

    if (res != 0) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	logE_ (_func, "epoll_ctl(): unexpected return value: ", res);
	return Result::Failure;
    }

    if (!trigger ()) {
	logF_ (_func, "trigger() failed: ", exc->toString());
	return Result::Failure;
    }

  // We've added a new pollable to the poll group. We don't yet know its state
  // and we use edge-triggered events, hence we should assume initially that
  // input and output events should be reported without waiting for an edge.
  // Epoll appears to handle this for us, delivering ET I/O events which occured
  // before EPOLL_CTL_ADD.

    return Result::Success;
}

mt_throws Result
EpollPollGroup::activatePollable (PollableKey const mt_nonnull key)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> ((void*) key);

    logD (epoll, _func, "0x", fmt_hex, (UintPtr) this, ": "
	  "pollable_entry: 0x", fmt_hex, (UintPtr) pollable_entry);

    return doActivate (pollable_entry);
}

void
EpollPollGroup::removePollable (PollableKey const mt_nonnull key)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> ((void*) key);

    logD (epoll, _func, "pollable_entry: 0x", fmt_hex, (UintPtr) pollable_entry);

    {
	int const res = epoll_ctl (efd, EPOLL_CTL_DEL, pollable_entry->fd, NULL /* event */);
	if (res == -1) {
	    logF_ (_func, "epoll_ctl() failed: ", errnoString (errno));
	} else {
	    if (res != 0)
		logF_ (_func, "epoll_ctl(): unexpected return value: ", res);
	}
    }

    mutex.lock ();
    pollable_entry->valid = false;
    pollable_list.remove (pollable_entry);
    pollable_deletion_queue.append (pollable_entry);
    mutex.unlock ();
}

mt_throws Result
EpollPollGroup::poll (Uint64 const timeout_microsec)
{
    logD (epoll, _func, "timeout: ", timeout_microsec);

    Time const start_microsec = getTimeMicroseconds ();

    struct epoll_event events [4096];
    bool first = true;
    for (;;) {
	Time cur_microsec = first ? (first = false, start_microsec) : getTimeMicroseconds ();
        if (cur_microsec < start_microsec)
            cur_microsec = start_microsec;

	Time const elapsed_microsec = cur_microsec - start_microsec;

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
	    // We've got deferred tasks to process, hence we shouldn't block.
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

#ifdef LIBMARY__EPOLL_POLL_GROUP__DEBUG
        logD_ (_this_func, "calling epoll_wait()");
#endif

#if 0
        if (timeout != 0) {
            updateTime ();
            logD_ (_func, "epoll_wait... (", timeout, ")");
        }
#endif

	int const nfds = epoll_wait (efd, events, sizeof (events) / sizeof (events [0]), timeout);

#if 0
        if (timeout != 0) {
            updateTime ();
            logD_ (_func, "...epoll_wait, nfds: ", nfds);
        }
#endif

#ifdef LIBMARY__EPOLL_POLL_GROUP__DEBUG
        logD_ (_this_func, "epoll_wait() returned");
#endif

	if (nfds == -1) {
	    if (errno == EINTR)
		continue;

	    exc_throw (PosixException, errno);
	    logF_ (_func, "epoll_wait() failed: ", errnoString (errno));
	    return Result::Failure;
	}

	if (nfds < 0 || (Size) nfds > sizeof (events) / sizeof (events [0])) {
	    logF_ (_func, "epoll_wait(): unexpected return value: ", nfds);
	    return Result::Failure;
	}

      // Trigger optimization:
      //
      // After trigger() returns, two events MUST happen:
      //   1. pollIterationBegin() and pollIterationEnd() must be called,
      //      i.e. a full poll iteration must occur;
      //   2. poll() should return.
      //
      // For poll/select/epoll, we use pipes to implement trigger().
      // Writing and reading to/from a pipe is expensive. To reduce
      // the number of writes, we do the following:
      //
      // 1. write() makes sense only when we're blocked in epoll_wait(),
      //    i.e. when epoll_wait with a non-zero timeout is in progress.
      //    'block_trigger_pipe' is set to 'false' when we're (supposedly)
      //    blocked in epoll_wait().
      // 2. 'triggered' flag indicates that trigger() has been called.
      //    This flag should be checked and cleared right after epoll_wait
      //    to ensure that every trigger() results in a full extra poll
      //    iteration.

        // This lock() acts as a memory barrier which ensures that we see
        // valid contents of PollableEntry objects.
        mutex.lock ();
        block_trigger_pipe = true;
        bool const was_triggered = triggered;
        triggered = false;
        mutex.unlock ();

	got_deferred_tasks = false;

	if (frontend)
	    frontend.call (frontend->pollIterationBegin);

	bool trigger_pipe_ready = false;
        for (int i = 0; i < nfds; ++i) {
            PollableEntry * const pollable_entry = static_cast <PollableEntry*> (events [i].data.ptr);
            uint32_t const epoll_event_flags = events [i].events;
            Uint32 event_flags = 0;

            if (pollable_entry == NULL) {
              // Trigger pipe event.
                if (epoll_event_flags & EPOLLIN)
                    trigger_pipe_ready = true;

                if (epoll_event_flags & EPOLLOUT)
                    logW_ (_func, "Unexpected EPOLLOUT event for trigger pipe");

                if (epoll_event_flags & EPOLLHUP   ||
                    epoll_event_flags & EPOLLRDHUP ||
                    epoll_event_flags & EPOLLERR)
                {
                    logF_ (_func, "Trigger pipe error: 0x", fmt_hex, epoll_event_flags);
                }

                continue;
            }

            if (epoll_event_flags & EPOLLIN)
                event_flags |= PollGroup::Input;

            if (epoll_event_flags & EPOLLOUT)
                event_flags |= PollGroup::Output;

            if (epoll_event_flags & EPOLLHUP ||
                epoll_event_flags & EPOLLRDHUP)
            {
                event_flags |= PollGroup::Hup;
            }

            if (epoll_event_flags & EPOLLERR)
                event_flags |= PollGroup::Error;

            if (event_flags) {
#ifdef LIBMARY__EPOLL_POLL_GROUP__DEBUG
                logD_ (_this_func, "calling pollable->processEvents()");
#endif

                logD (epoll, _this_func, "pollable_entry: 0x", fmt_hex, (UintPtr) pollable_entry, ", "
                      "events: 0x", event_flags, " ",
                      (event_flags & PollGroup::Input  ? "I" : ""),
                      (event_flags & PollGroup::Output ? "O" : ""),
                      (event_flags & PollGroup::Error  ? "E" : ""),
                      (event_flags & PollGroup::Hup    ? "H" : ""));

                pollable_entry->pollable.call (
                        pollable_entry->pollable->processEvents, /*(*/ event_flags /*)*/);

#ifdef LIBMARY__EPOLL_POLL_GROUP__DEBUG
                logD_ (_this_func, "pollable->processEvents() returned");
#endif
            }
        } // for (;;) - for all fds.

	if (frontend) {
	    bool extra_iteration_needed = false;
	    frontend.call_ret (&extra_iteration_needed, frontend->pollIterationEnd);
	    if (extra_iteration_needed)
		got_deferred_tasks = true;
	}

        mutex.lock ();
	processPollableDeletionQueue ();
        mutex.unlock ();

        if (trigger_pipe_ready) {
	    logD (epoll, _func, "trigger pipe break");

            if (!commonTriggerPipeRead (trigger_pipe [0])) {
                logF_ (_func, "commonTriggerPipeRead() failed: ", exc->toString());
                return Result::Failure;
            }
            break;
        }

        if (was_triggered) {
	    logD (epoll, _func, "trigger break");
            break;
        }

	if (elapsed_microsec >= timeout_microsec) {
	  // Timeout expired.
	    break;
	}
    } // for (;;)

    return Result::Success;
}

mt_throws Result
EpollPollGroup::trigger ()
{
#if 0
// In the current form, 'poll_tlocal' is excessive, overlaps with 'block_trigger_pipe'.
// It would make slightly more sense if we used thread-local 'triggered' flag here
// instead of general mutex-protected 'triggered'.

    if (poll_tlocal && poll_tlocal == libMary_getThreadLocal()) {
	mutex.lock ();
	triggered = true;
	mutex.unlock ();
	return Result::Success;
    }
#endif

    mutex.lock ();
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

mt_const mt_throws Result
EpollPollGroup::open ()
{
    efd = epoll_create (1 /* size, unused */);
    if (efd == -1) {
	exc_throw (PosixException, errno);
	logF_ (_func, "epoll_create() failed: ", errnoString (errno));
	return Result::Failure;
    }
    logD_ (_this_func, "epoll fd: ", efd);

    if (!posix_createNonblockingPipe (&trigger_pipe)) {
	return Result::Failure;
    }
    logD_ (_this_func, "trigger_pipe fd: read ", trigger_pipe [0], ", write ", trigger_pipe [1]);

    {
	struct epoll_event event;
	event.events = EPOLLET | EPOLLIN | EPOLLRDHUP;
	event.data.u64 = 0; // For valgrind.
	event.data.ptr = NULL; // 'NULL' tells that this is trigger pipe.
	int const res = epoll_ctl (efd, EPOLL_CTL_ADD, trigger_pipe [0], &event);
	if (res == -1) {
	    exc_throw (PosixException, errno);
	    logF_ (_func, "epoll_ctl() failed: ", errnoString (errno));
	    return Result::Failure;
	}

	if (res != 0) {
	    exc_throw (InternalException, InternalException::BackendMalfunction);
	    logF_ (_func, "epoll_ctl(): unexpected return value: ", res);
	    return Result::Failure;
	}
    }

    return Result::Success;
}

EpollPollGroup::EpollPollGroup (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      poll_tlocal (NULL),
      efd (-1),
      triggered (false),
      block_trigger_pipe (true),
      // Initializing to 'true' to process deferred tasks scheduled before we
      // enter poll() for the first time.
      got_deferred_tasks (true)
{
    trigger_pipe [0] = -1;
    trigger_pipe [1] = -1;
}

EpollPollGroup::~EpollPollGroup ()
{
    mutex.lock ();
    {
	PollableList::iter iter (pollable_list);
	while (!pollable_list.iter_done (iter)) {
	    PollableEntry * const pollable_entry = pollable_list.iter_next (iter);
	    delete pollable_entry;
	}
    }

    {
	PollableDeletionQueue::iter iter (pollable_deletion_queue);
	while (!pollable_deletion_queue.iter_done (iter)) {
	    PollableEntry * const pollable_entry = pollable_deletion_queue.iter_next (iter);
	    delete pollable_entry;
	}
    }
    mutex.unlock ();

    if (efd != -1) {
	for (;;) {
	    int const res = close (efd);
	    if (res == -1) {
		if (errno == EINTR)
		    continue;

		logE_ (_func, "close() failed (efd): ", errnoString (errno));
	    } else
	    if (res != 0) {
		logE_ (_func, "close() (efd): unexpected return value: ", res);
	    }

	    break;
	}
    }

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

