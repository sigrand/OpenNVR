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


#ifndef LIBMARY__UTIL_BASE__H__
#define LIBMARY__UTIL_BASE__H__


#include <cstdlib>


//#define unreachable_point(file, line, str) (unreachable_ (file ":" #line str))
//#define unreachable() (unreachable_point (__FILE__, __LINE__, ": unreachable point reached\n"))

#define unreachable3(line) (_libMary_unreachable_ (__FILE__ ":" #line "::", __func__))
#define unreachable2(line) unreachable3(line)
#define unreachable() unreachable2(__LINE__)

// Version of assert() which is never optimized out.
#define assert_hard(a) do { assert (a); if (!(a)) abort (); } while (0)


#include <libmary/types_base.h>
#include <libmary/memory.h>
#include <libmary/mutex.h>


extern "C" {
    void libmary_library_lock ();
    void libmary_library_unlock ();
}


namespace M {

void _libMary_unreachable_ (ConstMemory const &mem,
                            ConstMemory const &func);

/* 07.05.26
 * I introduced libraryLock and libraryUnlock to wrap
 * non-reentrant getservbyname library call.
 * It looks like Linux does not support reentrant
 * getservbyname_r call. */

/* 08.10.28
 * One should use libraryLock() in every case of using a single-threaded
 * library. Remember that such libraries could call non-reentrant glibc
 * functions, which means that they should all be synchronized with each
 * other. libraryLock/Unlock() is the synchronization mechanism to use
 * in case of using MyCpp. */

extern Mutex _libMary_library_mutex;
extern Mutex _libMary_helper_mutex;

/*m Aquires the lock that is used to protect non-reentrant library calls
 * (mainly for glibc calls). */
static inline void libraryLock ()
{
    _libMary_library_mutex.lock ();
}

/*m Releases the lock that is used to protect non-reentrant library calls
 * (mainly for glibc calls). */
static inline void libraryUnlock ()
{
    _libMary_library_mutex.unlock ();
}

static inline void helperLock ()
{
    _libMary_helper_mutex.lock ();
}

static inline void helperUnlock ()
{
    _libMary_helper_mutex.unlock ();
}

char* rawCollectBacktrace ();

}


#endif /* LIBMARY__UTIL_BASE__H__ */

