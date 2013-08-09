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


#ifndef LIBMARY__MULTI_THREAD__H__
#define LIBMARY__MULTI_THREAD__H__


#include <libmary/list.h>
#include <libmary/state_mutex.h>
#include <libmary/thread.h>


namespace M {

class MultiThread : public Object
{
private:
    StateMutex mutex;

    // Should be called only once. May be called again after join() completes.
    // Thread callback is reset when the thread exits.
    mt_mutex (mutex) Cb<Thread::ThreadFunc> thread_cb;

    mt_mutex (mutex) List< Ref<Thread> > thread_list;

    // Number of threads to be  spawned with spawn().
    mt_mutex (mutex) Count num_threads;
    // Number of threads currently active.
    mt_mutex (mutex) Count num_active_threads;

    mt_mutex (mutex) bool abort_spawn;

    static void wrapperThreadFunc (void *_thread_data);

public:
    // Should be called only once. May be called again after join() completes.
    mt_throws Result spawn (bool joinable);

    // Should be called once after every call to spawn(true /* joinable */).
    mt_throws Result join ();

    // Should be called only once. Has no effect after a call to spawn().
    // May be called again after join() completes.
    void setNumThreads (Count num_threads);

    // Should be called only once. May be called again after join() completes.
    // Thread callback is reset when the thread exits.
    void setThreadFunc (CbDesc<Thread::ThreadFunc> const &cb);

    MultiThread (Count num_threads = 1,
		 CbDesc<Thread::ThreadFunc> const &thread_cb = CbDesc<Thread::ThreadFunc> ())
	: thread_cb (thread_cb),
	  num_threads (num_threads),
	  num_active_threads (0),
	  abort_spawn (false)
    {
    }
};

}


#endif /* LIBMARY__MULTI_THREAD__H__ */

