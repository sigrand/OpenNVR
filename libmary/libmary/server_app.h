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


#ifndef LIBMARY__SERVER_APP__H__
#define LIBMARY__SERVER_APP__H__


#include <libmary/types.h>
#include <libmary/code_referenced.h>
#include <libmary/exception.h>
#include <libmary/timers.h>
#include <libmary/active_poll_group.h>
#include <libmary/deferred_processor.h>
#include <libmary/deferred_connection_sender.h>
#include <libmary/server_context.h>

#ifdef LIBMARY_MT_SAFE
  #include <libmary/multi_thread.h>
#endif


namespace M {

class ServerApp : public DependentCodeReferenced
{
private:
    StateMutex mutex;

public:
    struct Events
    {
        void (*threadStarted) (void *cb_data);
    };

private:
    class SA_ServerContext : public ServerContext
    {
    private:
	ServerApp * const server_app;

    public:
      mt_iface (ServerContext)

	CodeDepRef<ServerThreadContext> selectThreadContext  ();
        CodeDepRef<ServerThreadContext> getMainThreadContext ();

      mt_iface_end

	SA_ServerContext (Object    * const coderef_container,
                          ServerApp * const server_app)
	    : ServerContext (coderef_container),
              server_app (server_app)
	{
	}
    };

#ifdef LIBMARY_MT_SAFE
    class ThreadData : public Object
    {
    public:
	ServerThreadContext thread_ctx;

	Timers timers;
	DefaultPollGroup poll_group;
	DeferredProcessor deferred_processor;
	DeferredConnectionSenderQueue dcs_queue;

	ThreadData ()
	    : thread_ctx         (this /* coderef_container */),
              timers             (this /* coderef_container */),
              poll_group         (this /* coderef_container */),
              deferred_processor (this /* coderef_container */),
	      dcs_queue          (this /* coderef_container */)
	{
	}
    };
#endif

    Informer_<Events> event_informer;

    SA_ServerContext server_ctx;
    ServerThreadContext main_thread_ctx;

    Timers timers;
    DefaultPollGroup poll_group;
    DeferredProcessor deferred_processor;
    DeferredConnectionSenderQueue dcs_queue;

#ifdef LIBMARY_MT_SAFE
    mt_const Ref<MultiThread> multi_thread;

    typedef List< Ref<ThreadData> > ThreadDataList;
    mt_mutex (mutex) ThreadDataList thread_data_list;
    mt_mutex (mutex) ThreadDataList::Element *thread_selector;
#endif

    AtomicInt should_stop;

    static void informThreadStarted (Events *events,
                                     void   *cb_data,
                                     void   *inform_data);

    void fireThreadStarted ();

    static void firstTimerAdded (void *_active_poll_group);

    static void threadFunc (void *_self);

  mt_iface (ActivePollGroup::Frontend)
    static ActivePollGroup::Frontend poll_frontend;
    static void pollIterationBegin (void *_thread_ctx);
    static bool pollIterationEnd   (void *_thread_ctx);
  mt_iface_end

public:
    Informer_<Events>* getEventInformer ()
    {
        return &event_informer;
    }

    ServerContext* getServerContext ()
    {
	return &server_ctx;
    }

    mt_throws Result init ();

    mt_throws Result run ();

    void stop ();

    // Should be called before run().
    mt_const void setNumThreads (Count const num_threads)
    {
#ifdef LIBMARY_MT_SAFE
	multi_thread->setNumThreads (num_threads);
#else
	(void) num_threads;
#endif
    }

    void release ();

    ServerApp (Object *coderef_container,
	       Count   num_threads = 0);

    ~ServerApp ();
};

}


#endif /* LIBMARY__SERVER_APP__H__ */

