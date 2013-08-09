/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2013 Dmitry Shatrov

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


#ifndef LIBMARY__FAST_MUTEX__H__
#define LIBMARY__FAST_MUTEX__H__


#include <libmary/types.h>
#ifdef LIBMARY_MT_SAFE
  #include <libmary/atomic.h>
  #include <libmary/cond.h>
#endif


namespace M {

#ifdef LIBMARY_MT_SAFE
// This mutex implementation is presumably faster (1 atomic op for lock/unlock)
// in non-contended cases, and slower in contended ones.
// FastMutex is used for Object::Shadow::shadow_mutex, which is assumed to have
// very low contention. This speeds up working with weak references.
//
class FastMutex
{
private:
    AtomicInt lock_cnt;

    bool unlocked;

    Mutex mutex;
    Cond cond;

public:
    void lock ()
    {
        // full memory barrier
        if (lock_cnt.fetchAdd (1) == 0)
            return;

        mutex.lock ();
        while (!unlocked) {
            cond.wait (mutex);
        }
        unlocked = false;
        mutex.unlock ();
    }

    void unlock ()
    {
        // full memory barrier
        if (lock_cnt.fetchAdd (-1) == 1)
            return;

        mutex.lock ();
        unlocked = true;
        cond.signal ();
        mutex.unlock ();
    }

    FastMutex ()
        : unlocked (false)
    {
    }
};
#else
class FastMutex
{
public:
    void lock   () {}
    void unlock () {}
};
#endif

}


#endif /* LIBMARY__FAST_MUTEX__H__ */

