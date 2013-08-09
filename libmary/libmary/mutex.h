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


#ifndef LIBMARY__MUTEX__H__
#define LIBMARY__MUTEX__H__


#include <libmary/types.h>

#ifdef LIBMARY_MT_SAFE
  #ifdef __linux__
    #include <pthread.h>
  #else
    #include <glib.h>
  #endif
#endif


namespace M {

/*c */
// TODO Rename to RawMutex to prevent accidential use of Mutex instead of StateMutex.
class Mutex
{
#ifdef LIBMARY_MT_SAFE
  #if defined (__linux__)
    // Glib's mutexes and conds are crappy when it comes to performance,
    // especially after API change in 2.31. Every mutex is malloced
    // (even deprecated GStaticMutex), and actual pthread calls are several
    // layers deep.
private:
    pthread_mutex_t mutex;
public:
    void lock   () { pthread_mutex_lock   (&mutex); }
    void unlock () { pthread_mutex_unlock (&mutex); }
    pthread_mutex_t* get_pthread_mutex () { return &mutex; }
     Mutex () { pthread_mutex_init (&mutex, NULL /* mutexattr */); }
    ~Mutex () { pthread_mutex_destroy (&mutex); }
  #elif defined (LIBMARY__OLD_GTHREAD_API)
private:
    GStaticMutex mutex;
public:
    /*m Locks the mutex. */
    void lock () { g_static_mutex_lock (&mutex); }
    /*m Unlocks the mutex. */
    void unlock () { g_static_mutex_unlock (&mutex); }
    /* For internal use only: should not be expected to be present in future versions. */
    GMutex* get_glib_mutex () { return g_static_mutex_get_mutex (&mutex); }
     Mutex () { g_static_mutex_init (&mutex); }
    ~Mutex () { g_static_mutex_free (&mutex); }
  #else
private:
    GMutex mutex;
public:
    void lock ()   { g_mutex_lock   (&mutex); }
    void unlock () { g_mutex_unlock (&mutex); }
    GMutex* get_glib_mutex () { return &mutex; }
     Mutex () { g_mutex_init  (&mutex); }
    ~Mutex () { g_mutex_clear (&mutex); }
  #endif
#else
public:
    void lock   () {}
    void unlock () {}
#endif
};

class MutexLock
{
private:
    MutexLock& operator = (MutexLock const &);
    MutexLock (MutexLock const &);

#ifdef LIBMARY_MT_SAFE
private:
    Mutex * const mutex;

public:
    MutexLock (Mutex * const mt_nonnull mutex)
	: mutex (mutex)
    {
        mutex->lock ();
    }

    ~MutexLock ()
    {
	mutex->unlock ();
    }
#else
public:
    MutexLock (Mutex * const mt_nonnull /* mutex */) {}
#endif
};

class MutexUnlock
{
private:
    MutexUnlock& operator = (MutexUnlock const &);
    MutexUnlock (MutexUnlock const &);

#ifdef LIBMARY_MT_SAFE
private:
    Mutex * const mutex;

public:
    MutexUnlock (Mutex * const mt_nonnull mutex)
	: mutex (mutex)
    {
	mutex->unlock ();
    }

    ~MutexUnlock ()
    {
	mutex->lock ();
    }
#else
public:
    MutexUnlock (Mutex * const mt_nonnull /* mutex */) {}
#endif
};

}


#endif /* LIBMARY__MUTEX__H__ */

