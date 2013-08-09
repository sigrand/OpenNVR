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


#include <libmary/types.h>
#include <libmary/log.h>


#include <libmary/fixed_thread_pool.h>


namespace M {

void
FixedThreadPool::firstTimerAdded (void * const _active_poll_group)
{
    ActivePollGroup * const active_poll_group = static_cast <ActivePollGroup*> (_active_poll_group);
    active_poll_group->trigger ();
}

#ifdef LIBMARY_MT_SAFE
namespace {
void deferred_processor_trigger (void * const _active_poll_group)
{
    ActivePollGroup * const active_poll_group = static_cast <ActivePollGroup*> (_active_poll_group);
    active_poll_group->trigger ();
}

DeferredProcessor::Backend deferred_processor_backend = {
    deferred_processor_trigger
};
}

void
FixedThreadPool::threadFunc (void * const _self)
{
    FixedThreadPool * const self = static_cast <FixedThreadPool*> (_self);

    Ref<ThreadData> const thread_data = grab (new ThreadData);

    thread_data->dcs_queue.setDeferredProcessor (&thread_data->deferred_processor);

    thread_data->thread_ctx.init (&thread_data->timers,
				  &thread_data->poll_group,
				  &thread_data->deferred_processor,
				  &thread_data->dcs_queue);

    thread_data->poll_group.bindToThread (libMary_getThreadLocal());
    if (!thread_data->poll_group.open ()) {
	logE_ (_func, "poll_group.open() failed: ", exc->toString());
	return;
    }

    thread_data->poll_group.setFrontend (CbDesc<ActivePollGroup::Frontend> (
	    &poll_frontend, &thread_data->thread_ctx, NULL /* coderef_container */));

    thread_data->timers.setFirstTimerAddedCallback (CbDesc<Timers::FirstTimerAddedCallback> (
	    firstTimerAdded, &thread_data->poll_group, NULL /* coderef_container */));

    thread_data->deferred_processor.setBackend (CbDesc<DeferredProcessor::Backend> (
	    &deferred_processor_backend,
	    static_cast <ActivePollGroup*> (&thread_data->poll_group) /* cb_data */,
	    NULL /* coderef_container */));

    self->mutex.lock ();
    if (self->should_stop.get ()) {
	self->mutex.unlock ();
	return;
    }

    self->thread_data_list.append (thread_data);
    self->mutex.unlock ();

    for (;;) {
	if (!thread_data->poll_group.poll (thread_data->timers.getSleepTime_microseconds())) {
	    logE_ (_func, "poll_group.poll() failed: ", exc->toString());
	    // TODO This is a fatal error, but we should exit gracefully nevertheless.
	    abort ();
	    break;
	}

	if (self->should_stop.get())
	    break;
    }
}
#endif // LIBMARY_MT_SAFE

ActivePollGroup::Frontend FixedThreadPool::poll_frontend = {
    pollIterationBegin,
    pollIterationEnd
};

void
FixedThreadPool::pollIterationBegin (void * const _thread_ctx)
{
    ServerThreadContext * const thread_ctx = static_cast <ServerThreadContext*> (_thread_ctx);

    if (!updateTime ())
	logE_ (_func, "updateTime() failed: ", exc->toString());

    thread_ctx->getTimers()->processTimers ();
}

bool
FixedThreadPool::pollIterationEnd (void * const _thread_ctx)
{
    ServerThreadContext * const thread_ctx = static_cast <ServerThreadContext*> (_thread_ctx);
    return thread_ctx->getDeferredProcessor()->process ();
}

mt_throws CodeDepRef<ServerThreadContext>
FixedThreadPool::grabThreadContext (ConstMemory const & /* filename */)
{
#ifdef LIBMARY_MT_SAFE
    ServerThreadContext *thread_ctx;

  StateMutexLock l (&mutex);

    if (thread_selector) {
	thread_ctx = &thread_selector->data->thread_ctx;
	thread_selector = thread_selector->next;
    } else {
	if (!thread_data_list.isEmpty()) {
	    thread_ctx = &thread_data_list.getFirst()->thread_ctx;
	    thread_selector = thread_data_list.getFirstElement()->next;
	} else {
	    thread_ctx = main_thread_ctx;
	}
    }

    return thread_ctx;
#else
    return main_thread_ctx.ptr();
#endif // LIBMARY_MT_SAFE
}

void
FixedThreadPool::releaseThreadContext (ServerThreadContext * const /* thread_ctx */)
{
  // No-op
}

#if 0
// Unnecessary
mt_throws Result
FixedThreadPool::init ()
{
    return Result::Success;
}
#endif

mt_throws Result
FixedThreadPool::spawn ()
{
#ifdef LIBMARY_MT_SAFE
    if (!multi_thread->spawn (true /* joinable */)) {
	logE_ (_func, "multi_thread->spawn() failed: ", exc->toString());
	return Result::Failure;
    }
#endif

    return Result::Success;
}

void
FixedThreadPool::stop ()
{
#ifdef LIBMARY_MT_SAFE
    should_stop.set (1);

    mutex.lock ();

    {
	ThreadDataList::iter iter (thread_data_list);
	while (!thread_data_list.iter_done (iter)) {
	    ThreadData * const thread_data = thread_data_list.iter_next (iter)->data;
	    if (!thread_data->poll_group.trigger ())
		logE_ (_func, "poll_group.trigger() failed: ", exc->toString());
	}
    }

    mutex.unlock ();
#endif
}

FixedThreadPool::FixedThreadPool (Object * const coderef_container,
				  Count    const num_threads)
    : DependentCodeReferenced (coderef_container),
      main_thread_ctx (coderef_container)
#ifdef LIBMARY_MT_SAFE
      , thread_selector (NULL)
#endif
{
#ifdef LIBMARY_MT_SAFE
    multi_thread = grab (new MultiThread (
	    num_threads,
	    CbDesc<Thread::ThreadFunc> (threadFunc,
					this /* cb_data */,
					getCoderefContainer (),
					NULL /* ref_data */)));
#else
    (void) num_threads;
#endif
}

}

