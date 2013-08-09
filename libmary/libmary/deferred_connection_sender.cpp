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

#include <libmary/list.h>

#ifdef LIBMARY_ENABLE_MWRITEV
#include <libmary/mwritev.h>
#endif
#include <libmary/log.h>

#include <libmary/deferred_connection_sender.h>


namespace M {

// TODO Consider calling informAll_unlocked() in ImmediateConnectionSender and DeferredConnectionSender

namespace {
LogGroup libMary_logGroup_mwritev ("deferred_sender_mwritev", LogLevel::I);
LogGroup libMary_logGroup_sender ("deferred_sender", LogLevel::I);
LogGroup libMary_logGroup_close ("deferred_sender_close", LogLevel::I);
}

#ifdef LIBMARY_ENABLE_MWRITEV
namespace {

enum {
    Mwritev_MaxFds       = 4096,
    Mwritev_MaxTotalIovs = 65536,
    Mwritev_MaxIovsPerFd = 1024
};

mt_sync (DeferredSender::pollIterationEnd)
mt_begin
mt_end

void mwritevInit (LibMary_MwritevData * const mwritev)
{
    mwritev->initialized = true;

    mwritev->fds = new int [Mwritev_MaxFds];
    assert (mwritev->fds);

    mwritev->iovs = new struct iovec* [Mwritev_MaxFds];
    assert (mwritev->iovs);

    mwritev->iovs_heap = new struct iovec [Mwritev_MaxTotalIovs];
    assert (mwritev->iovs_heap);

    mwritev->num_iovs = new int [Mwritev_MaxFds];
    assert (mwritev->num_iovs);

    mwritev->res = new int [Mwritev_MaxFds];
    assert (mwritev->res);
}

} // namespace {}
#endif

mt_unlocks (mutex) void
DeferredConnectionSender::toGlobOutputQueue (bool const add_ref,
                                             bool const unlock)
{
    if (in_output_queue) {
        if (unlock) {
            mutex.unlock ();
        }
	return;
    }

    in_output_queue = true;
    if (unlock)
        mutex.unlock ();

    // It is important to ref() before adding the sender to output_queue.
    if (add_ref) {
	Object * const coderef_container = getCoderefContainer();
	if (coderef_container)
	    coderef_container->ref ();
    }

    // TODO Move this to a method of dcs_queue.
    assert (dcs_queue);
    dcs_queue->queue_mutex.lock ();
    assert (!dcs_queue->released);
    dcs_queue->output_queue.append (this);
    dcs_queue->queue_mutex.unlock ();

    dcs_queue->deferred_processor->trigger ();
}

mt_unlocks (mutex) void
DeferredConnectionSender::closeIfNeeded (bool const deferred_event)
{
    logD (close, fmt_hex, (UintPtr) this, fmt_def, " ", _func,
	  "close_after_flush: ", close_after_flush, ", "
	  "gotDataToSend: ", conn_sender_impl.gotDataToSend());

    if (!closed
        && close_after_flush
	&& !conn_sender_impl.gotDataToSend())
    {
        closed = true;
	mutex.unlock ();

        if (deferred_event) {
            // It is safe to use dcs_queue's deferred processor registration.
            fireClosed_deferred (&dcs_queue->send_reg, NULL /* exc_buf */);
            if (frontend) {
                frontend.call_deferred (&dcs_queue->send_reg,
                                        frontend->closed,
                                        NULL /* extra_ref_data */,
                                        static_cast <Exception*> (NULL) /* exc_ */);
            }
        } else {
            fireClosed (NULL /* exc_ */);
            if (frontend)
                frontend.call (frontend->closed, /*(*/ (Exception*) NULL /* exc_ */ /*)*/);
        }
    } else {
	mutex.unlock ();
    }
}

#ifdef LIBMARY_WIN32_IOCP
void
DeferredConnectionSender::outputIoComplete (Exception  * const exc_,
                                            Overlapped * const /* overlapped */,
                                            Size         const /* bytes_transferred */,
                                            void       * const _self)
{
#else
Connection::OutputFrontend const DeferredConnectionSender::conn_output_frontend = {
    processOutput
};

void
DeferredConnectionSender::processOutput (void * const _self)
{
#endif
    DeferredConnectionSender * const self = static_cast <DeferredConnectionSender*> (_self);

    self->mutex.lock ();

#ifdef LIBMARY_WIN32_IOCP
    self->conn_sender_impl.outputComplete ();

    if (exc_) {
        self->mutex.unlock ();
        if (self->frontend && self->frontend->closed)
            self->frontend.call (self->frontend->closed, /*(*/ exc_ /*)*/);

        return;
    }
#endif

    self->ready_for_output = true;

    if (self->conn_sender_impl.gotDataToSend ())
	mt_unlocks (self->mutex) self->toGlobOutputQueue (true /* add_ref */, true /* unlock */);
    else
	self->mutex.unlock ();
}

void
DeferredConnectionSender::sendMessage (MessageEntry * const mt_nonnull msg_entry,
				       bool           const do_flush)
{
    logD (mwritev, _func, "msg_entry: 0x", fmt_hex, (UintPtr) msg_entry);

    mutex.lock ();
    conn_sender_impl.queueMessage (msg_entry);
    if (do_flush) {
	mt_unlocks (mutex) doFlush (true /* unlock */);
	return;
    }
    mutex.unlock ();
}

mt_mutex (mutex) void
DeferredConnectionSender::sendMessage_unlocked (MessageEntry * const mt_nonnull msg_entry,
                                                bool           const do_flush)
{
    conn_sender_impl.queueMessage (msg_entry);
    if (do_flush)
        flush_unlocked ();
}

mt_unlocks (mutex) void
DeferredConnectionSender::doFlush (bool const unlock)
{
    if (ready_for_output
	&& conn_sender_impl.gotDataToSend())
    {
	logD (sender, _func, "calling toGlobOutputQueue()");
	mt_unlocks (mutex) toGlobOutputQueue (true /* add_ref */, unlock);
    } else {
        if (unlock)
            mutex.unlock ();
    }
}

void
DeferredConnectionSender::flush ()
{
    mutex.lock ();
    mt_unlocks (mutex) doFlush (true /* unlock */);
}

mt_mutex (mutex) void
DeferredConnectionSender::flush_unlocked ()
{
    doFlush (false /* unlock */);
}

void
DeferredConnectionSender::closeAfterFlush ()
{
    mutex.lock ();
    logD (close, _func, "in_output_queue: ", in_output_queue);
    close_after_flush = true;
    mt_unlocks (mutex) closeIfNeeded (true /* deferred_event */);
}

void
DeferredConnectionSender::close ()
{
    mutex.lock ();
    if (closed) {
        mutex.unlock ();
        return;
    }
    closed = true;
    mutex.unlock ();

    // It is safe to use dcs_queue's deferred processor registration.
    fireClosed_deferred (&dcs_queue->send_reg, NULL /* exc_buf */);
    if (frontend) {
        frontend.call_deferred (&dcs_queue->send_reg,
                                frontend->closed,
                                NULL /* extra_ref_data */,
                                static_cast <Exception*> (NULL) /* exc_ */);
    }
}

mt_mutex (mutex) bool
DeferredConnectionSender::isClosed_unlocked ()
    { return closed; }

mt_mutex (mutex) Sender::SendState
DeferredConnectionSender::getSendState_unlocked ()
    { return conn_sender_impl.getSendState(); }

void
DeferredConnectionSender::lock ()
    { mutex.lock (); }

void
DeferredConnectionSender::unlock ()
    { mutex.unlock (); }

mt_const void
DeferredConnectionSender::setQueue (DeferredConnectionSenderQueue * const mt_nonnull dcs_queue)
{
    this->dcs_queue = dcs_queue;

    // It is safe to use dcs_queue's deferred processor registration.
    conn_sender_impl.init (&frontend, this /* sender */, &dcs_queue->send_reg);
}

DeferredConnectionSender::DeferredConnectionSender (Object * const coderef_container)
    : Sender                  (coderef_container),
      DependentCodeReferenced (coderef_container),
      dcs_queue               (coderef_container),
      conn_sender_impl        (
#ifdef LIBMARY_WIN32_IOCP
                               CbDesc<Overlapped::IoCompleteCallback> (&outputIoComplete, this, coderef_container),
#endif
                               /* TEST (set to true)  true */ false /* enable_processing_barrier */),
      closed                  (false),
      close_after_flush       (false),
      ready_for_output        (true),
      in_output_queue         (false)
{
}

DeferredConnectionSender::~DeferredConnectionSender ()
{
//    logD_ (_func, "0x", fmt_hex, (UintPtr) this);

    mutex.lock();

    // Currently, DeferredConnectionSender cannot be destroyed while it is
    // in output_queue, because it is referenced while it is in the queue.
    assert (!in_output_queue);

    conn_sender_impl.release ();
    mutex.unlock();
}

bool
DeferredConnectionSenderQueue::process (void * const _self)
{
    logD (sender, _func_);

    DeferredConnectionSenderQueue * const self = static_cast <DeferredConnectionSenderQueue*> (_self);

    ProcessingQueue processing_queue;

    self->queue_mutex.lock ();
    assert (!self->released);

    if (self->processing) {
	self->queue_mutex.unlock ();
	logW_ (_func, "Concurrent invocation");
	return false;
    }
    self->processing = true;

    {
	OutputQueue::iter iter (self->output_queue);
	while (!self->output_queue.iter_done (iter)) {
	    DeferredConnectionSender * const deferred_sender = self->output_queue.iter_next (iter);
	    // Note that deferred_sender->mutex is not locked here.

	    processing_queue.append (deferred_sender);
	    self->output_queue.remove (deferred_sender);
	}
    }

    self->queue_mutex.unlock ();

    bool extra_iteration_needed = false;

    ProcessingQueue::iter iter (processing_queue);
    while (!processing_queue.iter_done (iter)) {
	DeferredConnectionSender * const deferred_sender = processing_queue.iter_next (iter);
	logD (sender, _func, "deferred_sender: 0x", fmt_hex, (UintPtr) deferred_sender);

	// The only place where 'deferred_sender' may be removed from the queue
	// is its destructor, which won't be called because 'deferred_sender' is
	// refed here.
	deferred_sender->mutex.lock ();

	assert (deferred_sender->in_output_queue);
	deferred_sender->in_output_queue = false;

	// TODO markProcessingBarrier() is useless now, since we check
	// if the barrier has been hit without unlocking the mutex.
	deferred_sender->conn_sender_impl.markProcessingBarrier ();

	AsyncIoResult const res = deferred_sender->conn_sender_impl.sendPendingMessages ();
	if (res == AsyncIoResult::Error ||
	    res == AsyncIoResult::Eof)
	{
	    logD (sender, _func, "res: ", res);

	    deferred_sender->ready_for_output = false;

	    // exc is NULL for Eof.
	    if (res == AsyncIoResult::Error) {
		logE_ (_func, exc->toString());

                if (!deferred_sender->closed) {
                    deferred_sender->closed = true;

                    ExceptionBuffer * const exc_buf = exc_swap_nounref ();

                    deferred_sender->fireClosed_unlocked (exc_buf->getException());
                    if (deferred_sender->frontend && deferred_sender->frontend->closed) {
                        deferred_sender->mutex.unlock ();
                        deferred_sender->frontend.call (deferred_sender->frontend->closed,
                                                        /*(*/ exc_buf->getException() /*)*/);
                    } else {
                        deferred_sender->mutex.unlock ();
                    }

                    exc_delete (exc_buf);
                } else {
                    deferred_sender->mutex.unlock ();
                }
            } else {
                if (!deferred_sender->closed) {
                    deferred_sender->closed = true;

                    deferred_sender->fireClosed_unlocked (NULL /* exc_buf */);
                    if (deferred_sender->frontend && deferred_sender->frontend->closed) {
                        deferred_sender->mutex.unlock ();
                        deferred_sender->frontend.call (deferred_sender->frontend->closed,
                                                        /*(*/ static_cast <Exception*> (NULL) /* exc_ */ /*)*/);
                    } else {
                        deferred_sender->mutex.unlock ();
                    }
                } else {
                    deferred_sender->mutex.unlock ();
                }
            }

	    {
		Object * const coderef_container = deferred_sender->getCoderefContainer();
		if (coderef_container)
		    coderef_container->unref ();
	    }
	    continue;
	}

	if (res == AsyncIoResult::Again)
	    deferred_sender->ready_for_output = false;
	else
	    deferred_sender->ready_for_output = true;

	if (!deferred_sender->conn_sender_impl.processingBarrierHit()) {
	  // At this point, conn_sender_impl has either sent all data, or it has
	  // gotten EAGAIN from writev. In either case, we should have removed
	  // deferred_sender from output_queue.

	    mt_unlocks (deferred_sender->mutex) deferred_sender->closeIfNeeded (false /* deferred_event */);

            {
                Object * const coderef_container = deferred_sender->getCoderefContainer();
                if (coderef_container)
                    coderef_container->unref ();
            }
	} else {
            // TODO I recall thinking that processing barriers are stupid.
            //      Re-visit this topic.

	    // TEST
	    assert (0);

            // TODO toGlobOutputQueue calls trigger(), which is unnecessary here.
	    mt_unlocks (deferred_sender->mutex) deferred_sender->toGlobOutputQueue (false /* add_ref */, true /* unlock */);

// Overlaps with (!self->output_queue.isEmpty()) check below.
//	    extra_iteration_needed = true;
	}
    }

    self->queue_mutex.lock ();
    if (!self->output_queue.isEmpty())
	extra_iteration_needed = true;

    self->processing = false;
    self->queue_mutex.unlock ();

//    logD_ (_func, "extra_iteration_needed: ", extra_iteration_needed ? "true" : "false");
    return extra_iteration_needed;
}

#ifdef LIBMARY_ENABLE_MWRITEV
bool
DeferredConnectionSenderQueue::process_mwritev (void *_self)
{
    logD (sender, _func_);

    DeferredConnectionSenderQueue * const self = static_cast <DeferredConnectionSenderQueue*> (_self);

    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    LibMary_MwritevData * const mwritev = &tlocal->mwritev;

    ProcessingQueue processing_queue;

    self->queue_mutex.lock ();
    assert (!self->released);

    if (!mwritev->initialized)
	mwritevInit (mwritev);

    if (self->processing) {
	self->queue_mutex.unlock ();
	logW_ (_func, "Concurrent invocation");
	return false;
    }
    self->processing = true;

    {
	OutputQueue::iter iter (self->output_queue);
	while (!self->output_queue.iter_done (iter)) {
	    DeferredConnectionSender * const deferred_sender = self->output_queue.iter_next (iter);
	    processing_queue.append (deferred_sender);
	    self->output_queue.remove (deferred_sender);
	}
    }

    self->queue_mutex.unlock ();

    bool extra_iteration_needed = false;

    ProcessingQueue::iter start_iter (processing_queue);
    while (!processing_queue.iter_done (start_iter)) {
	ProcessingQueue::iter next_iter;

	Count fd_idx = 0;
	{
	    assert (Mwritev_MaxTotalIovs >= Mwritev_MaxIovsPerFd);
	    Count total_iovs = 0;
	    ProcessingQueue::iter iter = start_iter;
	    while (!processing_queue.iter_done (iter)) {
		if (fd_idx >= Mwritev_MaxFds) {
		    logD_ (_func, "max fds");
		    break;
		}

		DeferredConnectionSender * const deferred_sender = processing_queue.iter_next (iter);

		mwritev->fds [fd_idx] = deferred_sender->conn_sender_impl.getConnection()->getFd();

		deferred_sender->mutex.lock ();

		assert (deferred_sender->in_output_queue);
		deferred_sender->in_output_queue = false;

		deferred_sender->conn_sender_impl.markProcessingBarrier ();

		Count num_iovs = 0;
		deferred_sender->conn_sender_impl.sendPendingMessages_fillIovs (&num_iovs,
										mwritev->iovs_heap + total_iovs,
										Mwritev_MaxIovsPerFd,
                                                                                false /* deferred_events */);

		// TODO Try leaving the lock here and unlocking after mwritev() call.
		// TEST (uncomment)
//		deferred_sender->mutex.unlock ();

		mwritev->iovs [fd_idx] = mwritev->iovs_heap + total_iovs;
		mwritev->num_iovs [fd_idx] = num_iovs;

		total_iovs += num_iovs;

		++fd_idx;

		if (Mwritev_MaxTotalIovs - total_iovs < Mwritev_MaxIovsPerFd) {
		    logD_ (_func, "max total iovs");
		    break;
		}
	    }

	    next_iter = iter;
	}

	{
	    Result const res = libMary_mwritev (fd_idx,
						mwritev->fds,
						mwritev->iovs,
						mwritev->num_iovs,
						mwritev->res);
	    if (!res) {
		logE_ (_func, "libMary_mwritev() failed: ", errnoString (errno));
		self->processing = false;
		self->queue_mutex.unlock ();
		return false;
	    }
	}

	fd_idx = 0;
	{
	    ProcessingQueue::iter iter = start_iter;
	    while (!processing_queue.iter_done (iter) &&
		   iter != next_iter)
	    {
		DeferredConnectionSender * const deferred_sender = processing_queue.iter_next (iter);

		bool eintr = false;
		Size num_written = 0;
		int const posix_res = mwritev->res [fd_idx];
		AsyncIoResult async_res;
		if (posix_res >= 0) {
		    num_written = (Size) posix_res;
		    async_res = AsyncIoResult::Normal;
		} else
		if (posix_res == -EAGAIN ||
		    posix_res == -EWOULDBLOCK)
		{
		    async_res = AsyncIoResult::Again;
		} else
		if (posix_res == -EINTR) {
		    eintr = true;
		} else
		if (posix_res == -EPIPE) {
		    async_res = AsyncIoResult::Eof;
		} else  {
		    async_res = AsyncIoResult::Error;
		}

		logD (mwritev, _func, "async_res: ", async_res);

		if (async_res == AsyncIoResult::Error ||
		    async_res == AsyncIoResult::Eof)
		{
		    // TODO Questionable lock.
		    // TEST (uncomment)
//		    deferred_sender->mutex.lock ();
		    deferred_sender->ready_for_output = false;

		    // exc is NULL for Eof.
		    if (async_res == AsyncIoResult::Error) {
			exc_throw <PosixException> (-posix_res);
			logE_ (_func, exc->toString());

                        if (!closed) {
                            closed = true;

                            ExceptionBuffer * const exc_buf = exc_swap_nounref ();

                            deferred_sender->fireClosed_unlocked (exc_buf->getException());
                            if (deferred_sender->frontend && deferred_sender->frontend->closed) {
                                deferred_sender->mutex.unlock ();
                                deferred_sender->frontend.call (deferred_sender->frontend->closed,
                                                                /*(*/ exc_buf->getException() /*)*/);
                            } else {
                                deferred_sender->mutex.unlock ();
                            }

                            exc_delete (exc_buf);
                        } else {
                            deferred_sender->mutex.unlock ();
                        }
		    } else {
                        if (!closed) {
                            closed = true;

                            deferred_sender->fireClosed_unlocked (NULL /* exc_buf */);
                            if (deferred_sender->frontend && deferred_sender->frontend->closed) {
                                deferred_sender->mutex.unlock ();
                                deferred_sender->frontend.call (deferred_sender->frontend->closed,
                                                                /*(*/ NULL /* exc_ */ /*)*/);
                            } else {
                                deferred_sender->mutex.unlock ();
                            }
                        } else {
                            deferred_sender->mutex.unlock ();
                        }
                    }

		    {
			Object * const coderef_container = deferred_sender->getCoderefContainer();
			if (coderef_container)
			    coderef_container->unref ();
		    }

		    ++fd_idx;
		    continue;
		}

		if (async_res == AsyncIoResult::Again)
		    deferred_sender->ready_for_output = false;
		else
		    deferred_sender->ready_for_output = true;

		bool tmp_extra_iteration_needed;
		if (!eintr) {
		    // TEST (uncomment)
//		    deferred_sender->mutex.lock ();
		    deferred_sender->conn_sender_impl.sendPendingMessages_react (async_res,
                                                                                 num_written,
                                                                                 false /* deferred_events */);
		    tmp_extra_iteration_needed = deferred_sender->conn_sender_impl.processingBarrierHit();
		} else {
		    tmp_extra_iteration_needed = true;
		}

		if (!tmp_extra_iteration_needed) {
		    mt_unlocks (deferred_sender->mutex) deferred_sender->closeIfNeeded (false /* deferred_event */);

		    Object * const coderef_container = deferred_sender->getCoderefContainer ();
		    if (coderef_container)
			coderef_container->unref ();
		} else {
		    mt_unlocks (deferred_sender->mtuex) deferred_sender->closeIfNeeded (false /* deferred_event */);
		    extra_iteration_needed = true;
		}

		++fd_idx;
	    }
	}

	start_iter = next_iter;
    }

    self->queue_mutex.lock ();
    if (!self->output_queue.isEmpty())
	extra_iteration_needed = true;

    self->processing = false;
    self->queue_mutex.unlock ();

    return extra_iteration_needed;
}
#endif /* LIBMARY_ENABLE_MWRITEV */

mt_const void
DeferredConnectionSenderQueue::setDeferredProcessor (DeferredProcessor * const deferred_processor)
{
    this->deferred_processor = deferred_processor;

#ifdef LIBMARY_ENABLE_MWRITEV
    if (libMary_mwritevAvailable()) {
	send_task.cb = CbDesc<DeferredProcessor::TaskCallback> (
		process_mwritev, this /* cb_data */, getCoderefContainer());
    } else
#endif
    {
	send_task.cb = CbDesc<DeferredProcessor::TaskCallback> (
		process, this /* cb_data */, getCoderefContainer());
    }

    send_reg.setDeferredProcessor (deferred_processor);
    send_reg.scheduleTask (&send_task, true /* permanent */);
}

void
DeferredConnectionSenderQueue::release ()
{
//    logD_ (_func_);

    List<DeferredConnectionSender*> unref_list;

    queue_mutex.lock ();
    released = true;

    OutputQueue::iter iter (output_queue);
    while (!output_queue.iter_done (iter)) {
        DeferredConnectionSender * const deferred_sender = output_queue.iter_next (iter);

        Object * const coderef_container = deferred_sender->getCoderefContainer();
        if (coderef_container) {
            unref_list.append (deferred_sender);
            coderef_container->ref ();
        }
    }
    output_queue.clear ();

    queue_mutex.unlock ();

    {
      // Unrefing with 'queue_mutex' unlocked.
        List<DeferredConnectionSender*>::iter iter (unref_list);
        while (!unref_list.iter_done (iter)) {
            DeferredConnectionSender * const deferred_sender = unref_list.iter_next (iter)->data;

            deferred_sender->mutex.lock ();
            deferred_sender->in_output_queue = false;
            deferred_sender->mutex.unlock ();

            Object * const coderef_container = deferred_sender->getCoderefContainer();
            assert (coderef_container);
            coderef_container->unref ();
            coderef_container->unref ();
        }
    }
}

DeferredConnectionSenderQueue::DeferredConnectionSenderQueue (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      deferred_processor (coderef_container),
      processing (false),
      released (false)
{
}

DeferredConnectionSenderQueue::~DeferredConnectionSenderQueue ()
{
//    logD_ (_func_);

#if 0
#error TODO Deinitialization is broken.
#error MomentServer, ServerApp, DeferredConnectionSenderQueue, DeferredProcessor, and other objects
#error hold references to real-world objects, and their deletion occurs after
#error destruction of MomentServer''s subobjects begins.
#endif

    if (deferred_processor) {
	send_reg.revokeTask (&send_task);
	send_reg.release ();
    }

    {
        queue_mutex.lock ();

        if (!released)
            logF_ (_func, "release() has not been called");

        if (!output_queue.isEmpty())
            logF_ (_func, "output queue is not empty");

        queue_mutex.unlock ();
    }
}

}

