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


#ifndef LIBMARY__THREAD__H__
#define LIBMARY__THREAD__H__


#include <libmary/types.h>
#include <glib.h>

#include <libmary/exception.h>
#include <libmary/cb.h>
#include <libmary/state_mutex.h>


namespace M {

class Thread : public Object
{
private:
    StateMutex mutex;

public:
    typedef void ThreadFunc (void *cb_data);

private:
    // 'thread_cb' gets reset when the thread exits.
    mt_mutex (mutex) Cb<ThreadFunc> thread_cb;
    mt_mutex (mutex) GThread *thread;

    static gpointer wrapperThreadFunc (gpointer _self);

public:
    // Should be called only once. May be called again after join() completes.
    mt_throws Result spawn (bool joinable);

    // Should be called once after every call to spawn(true /* joinable */).
    mt_throws Result join ();

    // Should be called only once. May be called again after join() completes.
    // Thread callback is reset when the thread exits.
    void setThreadFunc (CbDesc<ThreadFunc> const &cb);

    Thread (CbDesc<ThreadFunc> const &thread_cb = CbDesc<ThreadFunc> ())
	: thread_cb (thread_cb),
	  thread (NULL)
    {}
};

}


#endif /* LIBMARY__THREAD__H__ */

