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


#ifndef LIBMARY__UTIL_TIME__H__
#define LIBMARY__UTIL_TIME__H__


#include <libmary/types.h>

#include <time.h>


namespace M {

// TODO 1 раз в минуту делать gettimeofday и поддерживать реальное время дня (для логов).

// Retreives cached time in seconds.
static inline Time getTime ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    return tlocal->time_seconds;
}

static inline Time getTimeMilliseconds ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    return tlocal->time_microseconds / 1000;
}

static inline Time getUnixtime ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    return tlocal->unixtime;
}

// Retrieves cached time in microseconds.
static inline Time getTimeMicroseconds ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    return tlocal->time_microseconds;
}

mt_throws Result updateTime ();

void splitTime (Time       unixtime,
		struct tm * mt_nonnull ret_tm);

void uSleep (unsigned long const microseconds);

static inline void sSleep (unsigned long const seconds)
{
    unsigned long const microseconds = seconds * 1000000;
    assert (microseconds > seconds);
    uSleep (microseconds);
}

Result unixtimeToStructTm (Time       unixtime,
                           struct tm * mt_nonnull ret_tm);

enum {
    unixtimeToString_BufSize = 30
};

Size unixtimeToString (Memory mem,
                       Time   time);

Size timeToHttpString (Memory     mem,
                       struct tm * mt_nonnull tm);

Result parseHttpTime (ConstMemory  mem,
                      struct tm   * mt_nonnull ret_tm);

Result parseDuration (ConstMemory  mem,
		      Time        * mt_nonnull ret_duration);

ComparisonResult compareTime (struct tm * mt_nonnull left,
                              struct tm * mt_nonnull right);

}


#endif /* LIBMARY__UTIL_TIME__H__ */

