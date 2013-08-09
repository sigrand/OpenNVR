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


#ifndef LIBMARY__LIBMARY_THREAD_LOCAL__H__
#define LIBMARY__LIBMARY_THREAD_LOCAL__H__


#include <libmary/types.h>
#include <time.h>

#ifdef LIBMARY_ENABLE_MWRITEV
#include <sys/uio.h>
#endif

#include <libmary/exception_buffer.h>


#ifdef LIBMARY_TLOCAL
  #define LIBMARY__TLOCAL_SPEC LIBMARY_TLOCAL
#else
  #define LIBMARY__TLOCAL_SPEC
#endif

#ifdef LIBMARY__OLD_GTHREAD_API
  #ifndef LIBMARY_TLOCAL
    #define LIBMARY__TLOCAL_GPRIVATE    _libMary_tlocal_gprivate
  #endif
  #define LIBMARY__TLOCAL_GPRIVATE_DTOR _libMary_tlocal_gprivate_dtor
#else
  #ifndef LIBMARY_TLOCAL
   #define LIBMARY__TLOCAL_GPRIVATE      &_libMary_tlocal_gprivate
  #endif
  #define LIBMARY__TLOCAL_GPRIVATE_DTOR &_libMary_tlocal_gprivate_dtor
#endif


#include <libmary/object.h>


namespace M {

class Exception;

class CodeReferenced;
class Object;

#ifdef LIBMARY_ENABLE_MWRITEV
// DeferredConnectionSender's mwritev data.
class LibMary_MwritevData
{
public:
    bool           initialized;
    int           *fds;
    struct iovec **iovs;
    struct iovec  *iovs_heap;
    int           *num_iovs;
    int           *res;

    LibMary_MwritevData ()
	: initialized (false)
    {
    }
};
#endif

class LibMary_ThreadLocal
{
public:
    Object *deletion_queue;
    bool deletion_queue_processing;

    Count state_mutex_counter;

    Ref<ExceptionBuffer> exc_buffer;
    Exception *exc;
    IntrusiveList<ExceptionBuffer> exc_block_stack;
    IntrusiveList<ExceptionBuffer> exc_free_stack;
    Size exc_free_stack_size;

    Object::Shadow *last_coderef_container_shadow;

    char *strerr_buf;
    Size strerr_buf_size;

  // Time-related data fields

    Time time_seconds;
    Time time_microseconds;
    Time unixtime;

    Time time_log_frac;

    struct tm localtime;
    Time saved_unixtime;
    // Saved monotonic clock value in seconds.
    Time saved_monotime;

    char timezone_str [5];

#ifdef LIBMARY_PLATFORM_WIN32
    DWORD prv_win_time_dw;
    Time win_time_offs;
#endif

#ifdef LIBMARY_ENABLE_MWRITEV
    LibMary_MwritevData mwritev;
#endif

    LibMary_ThreadLocal ();
    ~LibMary_ThreadLocal ();
};

#ifdef LIBMARY_MT_SAFE
  #ifdef LIBMARY__OLD_GTHREAD_API
    #ifndef LIBMARY_TLOCAL
      extern GPrivate *_libMary_tlocal_gprivate;
    #endif
    extern GPrivate *_libMary_tlocal_gprivate_dtor;
  #else
    #ifndef LIBMARY_TLOCAL
      extern GPrivate _libMary_tlocal_gprivate;
    #endif
    extern GPrivate _libMary_tlocal_gprivate_dtor;
  #endif
#endif

#if defined LIBMARY_MT_SAFE && !defined LIBMARY_TLOCAL
    static inline mt_nonnull LibMary_ThreadLocal* libMary_getThreadLocal ()
    {
        LibMary_ThreadLocal *tlocal =
                static_cast <LibMary_ThreadLocal*> (g_private_get (LIBMARY__TLOCAL_GPRIVATE));
        if (tlocal)
            return tlocal;

        tlocal = new (std::nothrow) LibMary_ThreadLocal;
        assert (tlocal);
        g_private_set (LIBMARY__TLOCAL_GPRIVATE_DTOR, tlocal);
        g_private_set (LIBMARY__TLOCAL_GPRIVATE, tlocal);
        return tlocal;
    }
#else
    extern LIBMARY__TLOCAL_SPEC LibMary_ThreadLocal *_libMary_tlocal;

  #ifdef LIBMARY_MT_SAFE
    static inline mt_nonnull LibMary_ThreadLocal* libMary_getThreadLocal ()
    {
        LibMary_ThreadLocal *tlocal = _libMary_tlocal;
        if (tlocal)
            return tlocal;

        tlocal = new (std::nothrow) LibMary_ThreadLocal;
        assert (tlocal);
        _libMary_tlocal = tlocal;
	g_private_set (LIBMARY__TLOCAL_GPRIVATE_DTOR, tlocal);
        return tlocal;
    }
  #else
    #define libMary_getThreadLocal() (_libMary_tlocal)
  #endif
#endif

void libMary_threadLocalInit ();

void libMary_releaseThreadLocalForMainThread ();

}


#endif /* LIBMARY__LIBMARY_THREAD_LOCAL__H__ */

