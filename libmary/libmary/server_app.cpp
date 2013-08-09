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
#include <cstdio>

#include <libmary/deferred_connection_sender.h>
#include <libmary/util_time.h>
#include <libmary/log.h>

#include <libmary/server_app.h>


namespace M {

namespace {
LogGroup libMary_logGroup_server_app ("server_app", LogLevel::I);
}

ActivePollGroup::Frontend ServerApp::poll_frontend = {
    pollIterationBegin,
    pollIterationEnd
};

CodeDepRef<ServerThreadContext>
ServerApp::SA_ServerContext::selectThreadContext ()
{
#ifdef LIBMARY_MT_SAFE
    ServerThreadContext *thread_ctx;

  StateMutexLock l (&server_app->mutex);

    if (server_app->thread_selector) {
	thread_ctx = &server_app->thread_selector->data->thread_ctx;
	server_app->thread_selector = server_app->thread_selector->next;
    } else {
	if (!server_app->thread_data_list.isEmpty()) {
	    thread_ctx = &server_app->thread_data_list.getFirst()->thread_ctx;
	    server_app->thread_selector = server_app->thread_data_list.getFirstElement()->next;
	} else {
	    thread_ctx = &server_app->main_thread_ctx;
	}
    }

    return thread_ctx;
#else
    return &server_app->main_thread_ctx;
#endif // LIBMARY_MT_SAFE
}

CodeDepRef<ServerThreadContext>
ServerApp::SA_ServerContext::getMainThreadContext ()
{
    return &server_app->main_thread_ctx;
}

void
ServerApp::informThreadStarted (Events * const events,
                                void   * const cb_data,
                                void   * const /* inform_data */)
{
    if (events->threadStarted)
        events->threadStarted (cb_data);
}

void
ServerApp::fireThreadStarted ()
{
    event_informer.informAll (informThreadStarted, NULL /* inform_cb_data */);
}

void
ServerApp::firstTimerAdded (void * const _active_poll_group)
{
    logD (server_app, _func_);
    ActivePollGroup * const active_poll_group = static_cast <ActivePollGroup*> (_active_poll_group);
    active_poll_group->trigger ();
}

void
ServerApp::pollIterationBegin (void * const _thread_ctx)
{
    ServerThreadContext * const thread_ctx = static_cast <ServerThreadContext*> (_thread_ctx);

    if (!updateTime ())
	logE_ (_func, "updateTime() failed: ", exc->toString());

    thread_ctx->getTimers()->processTimers ();
}

bool
ServerApp::pollIterationEnd (void * const _thread_ctx)
{
    ServerThreadContext * const thread_ctx = static_cast <ServerThreadContext*> (_thread_ctx);
    return thread_ctx->getDeferredProcessor()->process ();
}

static void deferred_processor_trigger (void * const _active_poll_group)
{
    ActivePollGroup * const active_poll_group = static_cast <ActivePollGroup*> (_active_poll_group);
    active_poll_group->trigger ();
}

static DeferredProcessor::Backend deferred_processor_backend = {
    deferred_processor_trigger
};

mt_throws Result
ServerApp::init ()
{
    if (!poll_group.open ())
	return Result::Failure;

    dcs_queue.setDeferredProcessor (&deferred_processor);

    main_thread_ctx.init (&timers,
			  &poll_group,
			  &deferred_processor,
			  &dcs_queue);

    poll_group.setFrontend (CbDesc<ActivePollGroup::Frontend> (
	    &poll_frontend, &main_thread_ctx, NULL /* coderef_container */));

    timers.setFirstTimerAddedCallback (CbDesc<Timers::FirstTimerAddedCallback> (
	    firstTimerAdded,
	    static_cast <ActivePollGroup*> (&poll_group),
	    NULL /* coderef_container */));

    deferred_processor.setBackend (CbDesc<DeferredProcessor::Backend> (
	    &deferred_processor_backend,
	    static_cast <ActivePollGroup*> (&poll_group) /* cb_data */,
	    NULL /* coderef_container */));

    return Result::Success;
}

#ifdef LIBMARY_MT_SAFE
void
ServerApp::threadFunc (void * const _self)
{
    ServerApp * const self = static_cast <ServerApp*> (_self);

    Ref<ThreadData> const thread_data = grab (new ThreadData);

    thread_data->dcs_queue.setDeferredProcessor (&thread_data->deferred_processor);

    thread_data->thread_ctx.init (&thread_data->timers,
				  &thread_data->poll_group,
				  &thread_data->deferred_processor,
				  &thread_data->dcs_queue);

    thread_data->poll_group.bindToThread (libMary_getThreadLocal());
    if (!thread_data->poll_group.open ()) {
	logE_ (_func, "poll_group.open() failed: ", exc->toString());
        goto _return;
    }

    thread_data->poll_group.setFrontend (CbDesc<ActivePollGroup::Frontend> (
	    &poll_frontend, &thread_data->thread_ctx, NULL /* coderef_container */));

    thread_data->timers.setFirstTimerAddedCallback (CbDesc<Timers::FirstTimerAddedCallback> (
	    firstTimerAdded,
	    static_cast <ActivePollGroup*> (&thread_data->poll_group),
	    NULL /* coderef_container */));

    thread_data->deferred_processor.setBackend (CbDesc<DeferredProcessor::Backend> (
	    &deferred_processor_backend,
	    static_cast <ActivePollGroup*> (&thread_data->poll_group) /* cb_data */,
	    NULL /* coderef_container */));

    self->mutex.lock ();
    if (self->should_stop.get()) {
	self->mutex.unlock ();
        goto _return;
    }

    self->thread_data_list.append (thread_data);
    self->mutex.unlock ();

    self->fireThreadStarted ();

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

_return:
    thread_data->dcs_queue.release ();
}
#endif // LIBMARY_MT_SAFE

mt_throws Result
ServerApp::run ()
{
    poll_group.bindToThread (libMary_getThreadLocal());

#ifdef LIBMARY_MT_SAFE
    if (!multi_thread->spawn (true /* joinable */)) {
	logE_ (_func, "multi_thread->spawn() failed: ", exc->toString());
	return Result::Failure;
    }
#endif

    for (;;) {
	logD (server_app, _func, "iteration");
	if (!poll_group.poll (timers.getSleepTime_microseconds())) {
	    logE_ (_func, "poll_group.poll() failed: ", exc->toString());
	    stop ();
#ifdef LIBMARY_MT_SAFE
	    multi_thread->join ();
#endif
	    return Result::Failure;
	}

	if (should_stop.get())
	    break;
    }

    stop ();
#ifdef LIBMARY_MT_SAFE
    multi_thread->join ();
#endif

    return Result::Success;
}

void
ServerApp::stop ()
{
    should_stop.set (1);

#ifdef LIBMARY_MT_SAFE
    mutex.lock ();

    // FIXME If stop() is called before all thread funcs have added corresponding
    //       entries to 'thread_data_list', then we'll loose a thread and then
    //       fail to join() it afterwards, causing hangup.
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

void
ServerApp::release ()
{
    dcs_queue.release ();
}

ServerApp::ServerApp (Object * const coderef_container,
		      Count    const num_threads)
    : DependentCodeReferenced (coderef_container),
      event_informer (coderef_container, &mutex),
      server_ctx (coderef_container,
                  this /* server_app */),
      main_thread_ctx (coderef_container),

      timers (coderef_container
#if 0
// This callback is set in ServerApp::init()
              ,
              CbDesc<Timers::FirstTimerAddedCallback> (firstTimerAdded,
                                                       &poll_group /* cb_data */,
                                                       NULL        /* coderef_container */)
#endif
              ),
      poll_group         (coderef_container),
      deferred_processor (coderef_container),
      dcs_queue          (coderef_container)
#ifdef LIBMARY_MT_SAFE
      , thread_selector (NULL)
#endif
{
#ifdef LIBMARY_MT_SAFE
    multi_thread = grab (new MultiThread (
	    num_threads,
	    CbDesc<Thread::ThreadFunc> (threadFunc,
					this /* cb_data */,
					getCoderefContainer(),
					NULL /* ref_data */)));
#else
    (void) num_threads;
#endif
}

ServerApp::~ServerApp ()
{
    logD_ (_func_);

    dcs_queue.release ();
}

}

