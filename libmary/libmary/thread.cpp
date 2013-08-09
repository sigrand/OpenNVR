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


#include <libmary/log.h>

#include <libmary/thread.h>


namespace M {

gpointer
Thread::wrapperThreadFunc (gpointer const _self)
{
    Thread * const self = static_cast <Thread*> (_self);

    try {
	self->mutex.lock ();
	Cb<ThreadFunc> const tmp_cb = self->thread_cb;
	self->mutex.unlock ();

	tmp_cb.call_ ();
    } catch (...) {
	logE_ (_func, "unhandled C++ exception");
    }

    self->mutex.lock ();
    self->thread_cb.reset ();
    self->mutex.unlock ();

    self->unref ();

    // TODO Release thread-local data, if any
    // libMary_releaseThreadLocal ();
#warning DO DO DO RELEASE !!!

    return (gpointer) 0;
}

mt_throws Result
Thread::spawn (bool const joinable)
{
    this->ref ();

    mutex.lock ();
    GError *error = NULL;
    GThread * const tmp_thread = g_thread_create (wrapperThreadFunc,
						  this,
						  joinable ? TRUE : FALSE,
						  &error);
    this->thread = tmp_thread;
    mutex.unlock ();

    if (tmp_thread == NULL) {
	exc_throw (InternalException, InternalException::BackendError);
	logE_ (_func, "g_thread_create() failed: ",
	       error->message, error->message ? strlen (error->message) : 0);
	g_clear_error (&error);

	this->unref ();

	return Result::Failure;
    }

    return Result::Success;
}

// TODO Never fails?
mt_throws Result
Thread::join ()
{
    mutex.lock ();
    assert (thread);
    GThread * const tmp_thread = thread;
    thread = NULL;
    mutex.unlock ();

    g_thread_join (tmp_thread);

    return Result::Success;
}

void Thread::setThreadFunc (CbDesc<ThreadFunc> const &cb)
{
    mutex.lock ();
    this->thread_cb = cb;
    mutex.unlock ();
}

}

