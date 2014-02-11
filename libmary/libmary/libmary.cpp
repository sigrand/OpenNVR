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

#ifdef LIBMARY_PERFORMANCE_TESTING
IStatMeasurer* measurer_ = NULL;
ITimeChecker* checker_ = NULL;
#endif

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

#ifdef LIBMARY_PLATFORM_WIN32
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(2, 2);
    int res = WSAStartup(wVersionRequested, &wsaData);
    if (res != 0) {
        logE_ (_func_, "WSAStartup failed");
    } else {
        if (LOBYTE(wsaData.wVersion) != 2 ||
            HIBYTE(wsaData.wVersion) != 2) {
            logE_ (_func_, "Could not find a requested version of Winsock.dll");
            WSACleanup();
        }
        else {
            logD_ (_func_, "The Winsock 2.2 dll was found");
        }
    }
#endif
}

#ifdef LIBMARY_PERFORMANCE_TESTING
void setMeasurer (IStatMeasurer* m) {
    measurer_ = m;
}

void setTimeChecker (ITimeChecker* t) {
    checker_ = t;
}
#endif

void libMaryRelease ()
{
  // Release thread-local data here?
  // This could be done after careful deinitialization is implemented.
    libMary_releaseThreadLocalForMainThread ();

#ifdef LIBMARY_PLATFORM_WIN32
    WSACleanup();
#endif
}

}

