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


#include <libmary/types.h>
#include <glib.h>

//#include <cstdio>

#include <libmary/exception.h>

#include <libmary/libmary_thread_local.h>


namespace M {

#ifdef LIBMARY_MT_SAFE
    static void tlocal_destructor (gpointer _tlocal);
  #ifdef LIBMARY__OLD_GTHREAD_API
    #ifndef LIBMARY_TLOCAL
      GPrivate *_libMary_tlocal_gprivate = NULL;
    #endif
    GPrivate *_libMary_tlocal_gprivate_dtor = NULL;
  #else
    #ifndef LIBMARY_TLOCAL
      GPrivate _libMary_tlocal_gprivate;
    #endif
    GPrivate _libMary_tlocal_gprivate_dtor = G_PRIVATE_INIT (tlocal_destructor);
  #endif

  #ifndef LIBMARY_TLOCAL
    static LibMary_ThreadLocal *_libMary_main_tlocal = NULL;
  #endif
#endif

#if !(defined LIBMARY_MT_SAFE && !defined LIBMARY_TLOCAL)
    LIBMARY__TLOCAL_SPEC LibMary_ThreadLocal *_libMary_tlocal = NULL;
#endif

LibMary_ThreadLocal::LibMary_ThreadLocal ()
    : deletion_queue (NULL),
      deletion_queue_processing (false),
      state_mutex_counter (0),

      exc (NULL),
      exc_free_stack_size (0),

      last_coderef_container_shadow (NULL),

      time_seconds (0),
      time_microseconds (0),
      unixtime (0),

      time_log_frac (0),

      saved_unixtime (0),
      saved_monotime (0)

#ifdef LIBMARY_PLATFORM_WIN32
      ,
      prv_win_time_dw (0),
      win_time_offs (0)
#endif
{
//    fprintf (stderr, "---  LibMary_ThreadLocal() 0x%lx\n", (unsigned long) this);

    exc_buffer = grab (new (std::nothrow) ExceptionBuffer (LIBMARY__EXCEPTION_BUFFER_SIZE));

    memset (&localtime, 0, sizeof (localtime));
    memset (timezone_str, ' ', sizeof (timezone_str));

    strerr_buf_size = 4096;
    strerr_buf = new (std::nothrow) char [strerr_buf_size];
    assert (strerr_buf);
}

LibMary_ThreadLocal::~LibMary_ThreadLocal ()
{
//    fprintf (stderr, "--- ~LibMary_ThreadLocal() 0x%lx\n", (unsigned long) this);

    // Exceptions cleanup
    while (!exc_block_stack.isEmpty() || !exc_free_stack.isEmpty()) {
        {
            IntrusiveList<ExceptionBuffer>::iterator iter (exc_block_stack);
            while (!iter.done()) {
                ExceptionBuffer * const exc_buf = iter.next ();
                delete exc_buf;
            }
            exc_block_stack.clear ();
        }

        {
            IntrusiveList<ExceptionBuffer>::iterator iter (exc_free_stack);
            while (!iter.done()) {
                ExceptionBuffer * const exc_buf = iter.next ();
                delete exc_buf;
            }
            exc_free_stack.clear ();
        }

        if (exc_buffer->getException())
            exc_none ();
        else
            exc_buffer = NULL;
    }

    delete[] strerr_buf;
}

#ifdef LIBMARY_MT_SAFE
static void
tlocal_destructor (gpointer const _tlocal)
{
//    fprintf (stderr, "--- tlocal_destructor: 0x%lx\n", (unsigned long) _tlocal);

  #ifndef LIBMARY_TLOCAL
    // All gprivates are reset to NULL by glib/pthreads before tlocal_destructor()
    // is called. We restore the right value for tlocal gprivate, which is safe
    // since it doesn't have an associated destructor callback.
    g_private_set (LIBMARY__TLOCAL_GPRIVATE, _tlocal);
  #endif

    // Exception dtors may call arbitrary code, so we're
    // clearing exceptions first.
    exc_none ();
//    fprintf (stderr, "--- tlocal_destructor: 0x%lx: exc_none() done\n", (unsigned long) _tlocal);

    LibMary_ThreadLocal * const tlocal = static_cast <LibMary_ThreadLocal*> (_tlocal);
    if (tlocal)
	delete tlocal;

  #ifdef LIBMARY_TLOCAL
    _libMary_tlocal = NULL;
  #endif
}
#endif

void
libMary_threadLocalInit ()
{
#ifdef LIBMARY_MT_SAFE
  #ifdef LIBMARY__OLD_GTHREAD_API
    _libMary_tlocal_gprivate_dtor = g_private_new (tlocal_destructor);
    #ifndef LIBMARY_TLOCAL
      _libMary_tlocal_gprivate = g_private_new (NULL /* notify */);
    #endif
  #endif

  #ifndef LIBMARY_TLOCAL
    _libMary_main_tlocal = new (std::nothrow) LibMary_ThreadLocal;
    assert (_libMary_main_tlocal);
    g_private_set (LIBMARY__TLOCAL_GPRIVATE, _libMary_main_tlocal);
  #endif
#else
    _libMary_tlocal = new (std::nothrow) LibMary_ThreadLocal;
    assert (_libMary_tlocal);
#endif
}

void
libMary_releaseThreadLocalForMainThread ()
{
    // Exception dtors may call arbitrary code, so we're
    // clearing exceptions first.
    exc_none ();

#if defined LIBMARY_MT_SAFE && !defined LIBMARY_TLOCAL
    if (_libMary_main_tlocal) {
        delete _libMary_main_tlocal;
        _libMary_main_tlocal = NULL;
    }
#else
    if (_libMary_tlocal) {
        delete _libMary_tlocal;
        _libMary_tlocal = NULL;
    }
#endif
}

}

