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


#ifndef LIBMARY__COND__H__
#define LIBMARY__COND__H__


// Condition variables can only be sanely used in multithreaded code.
#ifdef LIBMARY_MT_SAFE

#include <libmary/mutex.h>
#include <libmary/state_mutex.h>


namespace M {

class Cond
{
#ifdef __linux__
private:
    pthread_cond_t cond;
public:
    void signal () { pthread_cond_signal (&cond); }
    void wait (Mutex      &mutex) { pthread_cond_wait (&cond, mutex.get_pthread_mutex()); }
    void wait (StateMutex &mutex) { pthread_cond_wait (&cond, mutex.get_pthread_mutex()); }
     Cond () { pthread_cond_init (&cond, NULL /* cond_attr */); }
    ~Cond () { pthread_cond_destroy (&cond); }
#elif defined (LIBMARY__OLD_GTHREAD_API)
private:
    GCond *cond;
public:
    void signal () { g_cond_signal (cond); }
    void wait (Mutex      &mutex) { g_cond_wait (cond, mutex.get_glib_mutex()); }
    void wait (StateMutex &mutex) { g_cond_wait (cond, mutex.get_glib_mutex()); }
     Cond () { cond = g_cond_new (); }
    ~Cond () { g_cond_free (cond); }
#else
private:
    GCond cond;
public:
    void signal () { g_cond_signal (&cond); }
    void wait (Mutex      &mutex) { g_cond_wait (&cond, mutex.get_glib_mutex()); }
    void wait (StateMutex &mutex) { g_cond_wait (&cond, mutex.get_glib_mutex()); }
     Cond () { g_cond_init  (&cond); }
    ~Cond () { g_cond_clear (&cond); }
#endif
};

}

#endif // LIBMARY_MT_SAFE


#endif /* LIBMARY__COND__H__ */

