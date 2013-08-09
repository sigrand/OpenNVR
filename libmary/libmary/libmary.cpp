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
#include <cstdlib>
#include <cstdio>
#include <locale.h>


#include <libmary/libmary.h>

#include <libmary/libmary_thread_local.h>

#ifdef LIBMARY_ENABLE_MWRITEV
#include <libmary/mwritev.h>
#endif


namespace M {

void libMary_platformInit ();

#ifdef LIBMARY_MT_SAFE
volatile gint _libMary_dummy_mb_int = 0;
#endif

OutputStream *outs;
OutputStream *errs;
OutputStream *logs;

Stat *_libMary_stat;

void libMaryInit ()
{
    {
	static bool initialized = false;

	if (initialized) {
	    return;
	}
	initialized = true;
    }

    // Setting numeric locale for snprintf() to behave uniformly in all cases.
    // Specifically, we need dot ('.') to be used as a decimal separator.
    if (setlocale (LC_NUMERIC, "C") == NULL)
        fprintf (stderr, "WARNING: Could not set LC_NUMERIC locale to \"C\"\n");

#ifndef LIBMARY_PLATFORM_WIN32
    // GStreamer calls setlocale(LC_ALL, ""), which is lame. We fight this with setenv().
    if (setenv ("LC_NUMERIC", "C", 1 /* overwrite */) == -1)
        perror ("WARNING: Could not set LC_NUMERIC environment variable to \"C\"");
#endif

#ifdef LIBMARY_MT_SAFE
  #ifdef LIBMARY__OLD_GTHREAD_API
    if (!g_thread_get_initialized ())
	g_thread_init (NULL);
  #endif
#endif

    _libMary_stat = new Stat;

    libMary_threadLocalInit ();
    libMary_platformInit ();

  // log*() logging is now available.

    if (!updateTime ())
        logE_ (_func, exc->toString());

#ifdef LIBMARY_ENABLE_MWRITEV
    libMary_mwritevInit ();
#endif

    randomSetSeed ((Uint32) getTime());
}

void libMaryRelease ()
{
  // Release thread-local data here?
  // This could be done after careful deinitialization is implemented.
    libMary_releaseThreadLocalForMainThread ();
}

}

