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


#include <libmary/log.h>

#include <libmary/multi_thread.h>


namespace M {

namespace {
class ThreadData : public Referenced
{
public:
    Cb<Thread::ThreadFunc> thread_cb;
};
}

void
MultiThread::wrapperThreadFunc (void *_self)
{
    MultiThread * const self = static_cast <MultiThread*> (_self);

  {
    self->mutex.lock ();
    if (self->abort_spawn) {
	self->mutex.unlock ();
	goto _return;
    }

    Cb<Thread::ThreadFunc> const tmp_cb = self->thread_cb;
    self->mutex.unlock ();

    tmp_cb.call_ ();

    self->mutex.lock ();
    assert (self->num_active_threads > 0);
    --self->num_active_threads;
    if (self->num_active_threads == 0) {
	self->thread_cb.reset ();
    }
    self->mutex.unlock ();
  }

_return:
    self->unref ();
}

mt_throws Result
MultiThread::spawn (bool const joinable)
{
    mutex.lock ();

    assert (num_active_threads == 0);
    for (Count i = 0; i < num_threads; ++i) {
	this->ref ();

	Ref<Thread> const thread = grab (new Thread (
		CbDesc<Thread::ThreadFunc> (wrapperThreadFunc,
					    this,
					    NULL /* coderef_container */,
					    NULL /* ref_data */)));

	thread_list.append (thread);
	if (!thread->spawn (joinable)) {
	    abort_spawn = true;
	    thread_cb.reset ();
	    mutex.unlock ();

	    this->unref ();
	    return Result::Failure;
	}
    }
    num_active_threads = num_threads;

    mutex.unlock ();

    return Result::Success;
}

// TODO Never fails?
mt_throws Result
MultiThread::join ()
{
    mutex.lock ();

    assert (num_active_threads <= num_threads);
    for (Count i = 0; i < num_threads; ++i) {
        assert (!thread_list.isEmpty());
	Ref<Thread> const thread = thread_list.getFirst();
	thread_list.remove (thread_list.getFirstElement());
	mutex.unlock ();

	if (!thread->join ())
	    logE_ (_func, "Thread::join() failed: ", exc->toString());

	mutex.lock ();
    }
    assert (num_active_threads == 0);

    mutex.unlock ();

    return Result::Success;
}

void
MultiThread::setNumThreads (Count const num_threads)
{
  StateMutexLock l (&mutex);
    this->num_threads = num_threads;
}

void
MultiThread::setThreadFunc (CbDesc<Thread::ThreadFunc> const &cb)
{
  StateMutexLock l (&mutex);
    this->thread_cb = cb;
}

}

