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


#ifndef LIBMARY__STATE_MUTEX__H__
#define LIBMARY__STATE_MUTEX__H__


#include <libmary/mutex.h>


namespace M {

class StateMutex
{
#ifdef LIBMARY_MT_SAFE
private:
    Mutex mutex;
public:
  #ifdef __linux__
    pthread_mutex_t* get_pthread_mutex () { return mutex.get_pthread_mutex(); }
  #else
    /* For internal use only: should not be expected to be present in future versions. */
    GMutex* get_glib_mutex () { return mutex.get_glib_mutex(); }
  #endif
#endif
public:
#ifdef LIBMARY_MT_SAFE
    void lock   ();
    void unlock ();
#else
  // Note: To test 'tlocal->state_mutex_counter' in mt-unsafe mode,
  //       StateMutexLock and StateMutexUnlock should be changed
  //       to actually call StateMutex::lock() and StateMutex::unlock().
    void lock   () {}
    void unlock () {}
#endif
};

class StateMutexLock
{
private:
    StateMutexLock& operator = (StateMutexLock const &);
    StateMutexLock (StateMutexLock const &);

#ifdef LIBMARY_MT_SAFE
private:
    StateMutex * const mutex;

public:
    StateMutexLock (StateMutex * const mt_nonnull mutex)
        : mutex (mutex) { mutex->lock (); }

    ~StateMutexLock () { mutex->unlock (); }
#else
public:
    StateMutexLock (StateMutex * const /* mutex */) {}
#endif
};

class StateMutexUnlock
{
private:
    StateMutexUnlock& operator = (StateMutexUnlock const &);
    StateMutexUnlock (StateMutexUnlock const &);

#ifdef LIBMARY_MT_SAFE
private:
    StateMutex * const mutex;

public:
    StateMutexUnlock (StateMutex * const mt_nonnull mutex)
        : mutex (mutex) { mutex->unlock (); }

    ~StateMutexUnlock () { mutex->lock (); }
#else
public:
    StateMutexUnlock (StateMutex * const mt_nonnull /* mutex */) {}
#endif
};

}


#endif /* LIBMARY__STATE_MUTEX__H__ */

