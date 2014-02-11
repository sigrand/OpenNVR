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
#include <cctype>
#include <cstdio>

#include <errno.h>
#include <time.h>

#include <libmary/log.h>
#include <libmary/util_str.h>

#include <libmary/util_time.h>

#include <glib.h>


#ifdef LIBMARY_PLATFORM_WIN32
#include <windows.h>
#ifdef LIBMARY_WIN32_SECURE_CRT
extern "C" {
    typedef int errno_t;

    errno_t localtime_s (struct tm    *tm,
                         time_t const *time);

    errno_t gmtime_s (struct tm    *tm,
                      time_t const *time);
}
#endif
#endif


namespace M {

namespace {
LogGroup libMary_logGroup_time ("time", LogLevel::None);
}

static void dumpTime (LibMary_ThreadLocal * const tlocal)
{
    fprintf (stderr,
             "--- tlocal 0x%lx\n"
             "time_seconds: %lu, "
             "time_microseconds: %lu, "
             "unixtime: %lu\n",
             (ptrdiff_t) tlocal,
             (unsigned long) tlocal->time_seconds,
             (unsigned long) tlocal->time_microseconds,
             (unsigned long) tlocal->unixtime);

    fprintf (stderr,
             "localtime: %lu/%lu/%lu %lu:%lu:%lu.%lu\n",
             (unsigned long) tlocal->localtime.tm_year + 1900,
             (unsigned long) tlocal->localtime.tm_mon + 1,
             (unsigned long) tlocal->localtime.tm_mday,
             (unsigned long) tlocal->localtime.tm_hour,
             (unsigned long) tlocal->localtime.tm_min,
             (unsigned long) tlocal->localtime.tm_sec,
             (unsigned long) tlocal->time_log_frac);

    fprintf (stderr,
             "saved_unixtime: %lu, "
             "saved_monotime: %lu\n",
             (unsigned long) tlocal->saved_unixtime,
             (unsigned long) tlocal->saved_monotime);

#ifdef LIBMARY_PLATFORM_WIN32
    fprintf (stderr,
             "prv_win_time_dw: %lu, "
             "win_time_offs: %lu\n",
             (unsigned long) tlocal->prv_win_time_dw,
             (unsigned long) tlocal->win_time_offs);
#endif
}

mt_throws Result updateTime ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();

#ifdef LIBMARY_PLATFORM_WIN32
    // replace timeGetTime() with GetTickCount() for a while, since GetTickCount provided by kernel.dll
    DWORD const win_time_dw =  GetTickCount();
    if (tlocal->prv_win_time_dw >= win_time_dw) {
        tlocal->win_time_offs += 0x100000000;
    }
    tlocal->prv_win_time_dw = win_time_dw;
    DWORD const win_time = win_time_dw + tlocal->win_time_offs;

    Time const new_seconds = (Time) win_time / 1000;
    Time const new_microseconds = (Time) win_time * 1000;

//    tlocal->time_log_frac = new_microseconds % 1000000 / 100;
#else
  #ifdef __MACH__
    gint64 const mono_time = g_get_monotonic_time ();
    Time const new_seconds = mono_time / 1000000;
    Time const new_microseconds = mono_time;
  #else
    struct timespec ts;
    // Note that clock_gettime is well-optimized on Linux x86_64 and does not carry
    // full syscall overhead (depends on system configuration).
    int const res = clock_gettime (CLOCK_MONOTONIC, &ts);
    if (res == -1) {
	exc_throw (PosixException, errno);
	exc_push (InternalException, InternalException::BackendError);
	logE_ (_func, "clock_gettime() failed: ", errnoString (errno));
	return Result::Failure;
    } else
    if (res != 0) {
	exc_throw (InternalException, InternalException::BackendError);
	logE_ (_func, "clock_gettime(): unexpected return value: ", res);
	return Result::Failure;
    }

    logD (time, _func, "tv_sec: ", ts.tv_sec, ", tv_nsec: ", ts.tv_nsec);

    Time const new_seconds = ts.tv_sec;
    Time const new_microseconds = (Uint64) ts.tv_sec * 1000000 + (Uint64) ts.tv_nsec / 1000;

//    tlocal->time_log_frac = ts.tv_nsec % 1000000000 / 100000;
  #endif
#endif

    tlocal->time_log_frac = new_microseconds % 1000000 / 100;

    if (new_seconds >= tlocal->time_seconds)
	tlocal->time_seconds = new_seconds;
    else
	logW_ (_func, "seconds backwards: ", new_seconds, " (was ", tlocal->time_seconds, ")");

    if (new_microseconds >= tlocal->time_microseconds)
	tlocal->time_microseconds = new_microseconds;
    else
	logW_ (_func, "microseconds backwards: ", new_microseconds, " (was ", tlocal->time_microseconds, ")");

    logD (time, _func, fmt_hex, tlocal->time_seconds, ", ", tlocal->time_microseconds);

    if (tlocal->saved_monotime < tlocal->time_seconds
        || tlocal->saved_unixtime == 0)
    {
	// Updading saved unixtime once in a minute.
	if (tlocal->time_seconds - tlocal->saved_monotime >= 60
            || tlocal->saved_unixtime == 0)
        {
	    // Obtaining current unixtime. This is an extra syscall.
	    tlocal->saved_unixtime = time (NULL);
	    tlocal->saved_monotime = tlocal->time_seconds;
	}

      // Updating localtime (broken-down time).

	time_t const cur_unixtime = tlocal->saved_unixtime + (tlocal->time_seconds - tlocal->saved_monotime);
	tlocal->unixtime = cur_unixtime;
	// Note that we call tzset() in libMary_posixInit() for localtime_r() to work correctly.
#ifdef LIBMARY_PLATFORM_WIN32
  #ifdef LIBMARY_WIN32_SECURE_CRT
        if (localtime_s (&tlocal->localtime, &cur_unixtime) != 0)
            logF_ (_func, "localtime_s() failed");
  #else
        libraryLock ();
        struct tm * const tmp_localtime = localtime (&cur_unixtime);
        if (tmp_localtime) {
            tlocal->localtime = *tmp_localtime;
        }
        libraryUnlock ();
        if (!tmp_localtime)
            logF_ (_func, "localtime() failed");
  #endif
#else
	localtime_r (&cur_unixtime, &tlocal->localtime);
#endif
    }

//    long const timezone_abs = timezone >= 0 ? timezone : -timezone;
    long const timezone_abs = 0;
//    tlocal->timezone_str [0] = timezone >= 0 ? '+' : '-';
    tlocal->timezone_str [0] = '+';
    tlocal->timezone_str [1] = '0' + timezone_abs / 36000;
    tlocal->timezone_str [2] = '0' + timezone_abs /  3600 % 10;
    tlocal->timezone_str [3] = '0' + timezone_abs /   600 %  6;
    tlocal->timezone_str [4] = '0' + timezone_abs /    60 % 10;

    return Result::Success;
}

void splitTime (Time        const unixtime,
		struct tm * const mt_nonnull ret_tm)
{
    time_t tmp_unixtime = (time_t) unixtime;
#ifdef LIBMARY_PLATFORM_WIN32
  #ifdef LIBMARY_WIN32_SECURE_CRT
    if (localtime_s (ret_tm, &tmp_unixtime) != 0)
      logF_ (_func, "localtime_s() failed");
  #else
    libraryLock ();
    struct tm * const tmp_localtime = localtime (&tmp_unixtime);
    if (tmp_localtime) {
        *ret_tm = *tmp_localtime;
    }
    libraryUnlock ();
    if (!tmp_localtime)
        logF_ (_func, "localtime() failed");
  #endif
#else
    localtime_r (&tmp_unixtime, ret_tm);
#endif
}

void uSleep (unsigned long const microseconds)
{
    g_usleep ((gulong) microseconds);
}

// It is guaranteed that there's 12 months and every month is 3 chars long.
static char const *months [] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

Result unixtimeToStructTm (Time        const unixtime,
                           struct tm * const mt_nonnull ret_tm)
{
    time_t t = unixtime;

#ifdef LIBMARY_PLATFORM_WIN32
  #ifdef LIBMARY_WIN32_SECURE_CRT
    if (gmtime_s (ret_tm, &t) != 0)
  #else
    libraryLock ();
    struct tm * const tmp_tm = gmtime (&t);
    if (tmp_tm) {
        *ret_tm = *tmp_tm;
    }
    libraryUnlock ();
    if (!tmp_tm)
  #endif
#else
    if (!gmtime_r (&t, ret_tm))
#endif
    {
	logE_ (_func, "gmtime_r() failed");
        return Result::Failure;
    }

    return Result::Success;
}

// HTTP date formatting (RFC2616, RFC822, RFC1123).
//
// @time - unixtime.
//
Size unixtimeToString (Memory const mem,
                       Time   const unixtime)
{
    struct tm tm;
    if (!unixtimeToStructTm (unixtime, &tm)) {
	logE_ (_func, "gmtime_r() failed");

	if (mem.len() > 0)
	    mem.mem() [0] = 0;

        return 0;
    }

    return timeToHttpString (mem, &tm);
}

Size timeToHttpString (Memory     mem,
                       struct tm * mt_nonnull tm)
{
    static char const *days [] = {
	    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

    size_t const res = strftime ((char*) mem.mem(), mem.len(), "---, %d --- %Y %H:%M:%S GMT", tm);
    if (res == 0 || res >= mem.len()) {
	logE_ (_func, "strftime() failed");

	if (mem.len() > 0)
	    mem.mem() [0] = 0;

	return 0;
    }

    assert (tm->tm_wday < 7);
    memcpy (mem.mem(), days [tm->tm_wday], 3);

    assert (tm->tm_mon < 12);
    memcpy (mem.mem() + 8, months [tm->tm_mon], 3);

    return (Size) res;
}

static Result parseMonth (ConstMemory   const mem,
                          unsigned    * const mt_nonnull ret_month,
                          Size        * const mt_nonnull ret_len)
{
    *ret_len = 3;

    if (mem.len() < 3)
        return Result::Failure;

    unsigned i = 0;
    for (; i < 12; ++i) {
        if (equal (mem.region (0, 3), ConstMemory (months [i], 3)))
            break;
    }

    if (i >= 12)
        return Result::Failure;

    *ret_month = i;
    return Result::Success;
}

static Result parseUint32 (ConstMemory   const mem,
                           Uint32      * const mt_nonnull ret_number,
                           Size        * const mt_nonnull ret_pos,
                           Size        * const ret_len = NULL)
{
    unsigned pos = *ret_pos;

    for (; pos < mem.len(); ++pos) {
        if (isdigit (mem.mem() [pos]))
            break;
    }

    if (pos >= mem.len())
        return Result::Failure;

    Size len = 0;
    for (Size const end = mem.len() - pos; len < end; ++len) {
        if (!isdigit (mem.mem() [pos + len]))
            break;
    }

    if (ret_len)
        *ret_len = len;

    Byte const *endptr;
    if (!strToUint32 (mem.region (pos, len), ret_number, &endptr, 10 /* base */))
        return Result::Failure;

    pos = endptr - mem.mem();

    *ret_pos = pos;
    return Result::Success;
}

// tm_wday, tm_yday, tm_isdst fields of struct tm are ignored.
//
Result parseHttpTime (ConstMemory   const mem,
                      struct tm   * const mt_nonnull ret_tm)
{
    Size pos = 0;

    memset (ret_tm, 0, sizeof (*ret_tm));

    // Skipping to day of week.
    for (; pos < mem.len(); ++pos) {
        if (isalpha (mem.mem() [pos]))
            break;
    }

    // Skipping day of week.
    for (; pos < mem.len(); ++pos) {
        if (!isalpha (mem.mem() [pos]))
            break;
    }

    bool is_asctime = false;
    for (; pos < mem.len(); ++pos) {
        if (isalpha (mem.mem() [pos])) {
            is_asctime = true;
            break;
        }

        if (isdigit (mem.mem() [pos]))
            break;
    }

    if (pos >= mem.len())
        return Result::Failure;

    if (is_asctime) {
      // Month
        unsigned month;
        Size month_len;
        if (!parseMonth (mem.region (pos, mem.len() - pos), &month, &month_len))
            return Result::Failure;

        pos += month_len;

        ret_tm->tm_mon = (int) month;
    }

    {
      // Day
        Uint32 day;
        if (!parseUint32 (mem, &day, &pos))
            return Result::Failure;

        ret_tm->tm_mday = (int) day;
    }

    if (!is_asctime) {
        {
          // Month
            for (; pos < mem.len(); ++pos) {
                if (isalpha (mem.mem() [pos]))
                    break;
            }

            if (pos >= mem.len())
                return Result::Failure;

            unsigned month;
            Size month_len;
            if (!parseMonth (mem.region (pos, mem.len() - pos), &month, &month_len))
                return Result::Failure;

            pos += month_len;

            ret_tm->tm_mon = (int) month;
        }

        {
          // Year
            Uint32 year;
            Size len;
            if (!parseUint32 (mem, &year, &pos, &len))
                return Result::Failure;

            if (len == 2) {
              // I didn't find any reference on how to decode 2-digit years properly.
              // The following heuristic was the first to come to mind.
                if (year >= 70)
                    year += 1900;
                else
                    year += 2000;
            }

            ret_tm->tm_year = year - 1900;
        }
    }

    {
      // Hours
        Uint32 hour;
        if (!parseUint32 (mem, &hour, &pos))
            return Result::Failure;

        ret_tm->tm_hour = hour;
    }

    {
      // Minutes
        Uint32 minute;
        if (!parseUint32 (mem, &minute, &pos))
            return Result::Failure;

        ret_tm->tm_min = minute;
    }

    {
      // Seconds
        Uint32 second;
        if (!parseUint32 (mem, &second, &pos))
            return Result::Failure;

        ret_tm->tm_sec = second;
    }

    if (is_asctime) {
        Uint32 year;
        if (!parseUint32 (mem, &year, &pos))
            return Result::Failure;

        ret_tm->tm_year = year - 1900;
    }

    return Result::Success;
}

Result parseDuration (ConstMemory   const mem,
		      Time        * const mt_nonnull ret_duration)
{
    Size offs = 0;

    Uint32 num [3] = { 0, 0, 0 };

    int i;
    for (i = 0; i < 3; ++i) {
	while (offs < mem.len() && mem.mem() [offs] == ' ')
	    ++offs;

	Byte const *endptr;
	if (!strToUint32 (mem.region (offs), &num [i], &endptr, 10 /* base */))
	    return Result::Failure;

	offs = endptr - mem.mem();

	while (offs < mem.len() && mem.mem() [offs] == ' ')
	    ++offs;

	if (i < 2) {
	    if (mem.mem() [offs] != ':')
		break;

	    ++offs;
	}
    }

    while (offs < mem.len() && mem.mem() [offs] == ' ')
	++offs;

    if (offs != mem.len())
	return Result::Failure;

    switch (i) {
	case 0:
	    *ret_duration = (Time) num [0];
	    break;
	case 1:
	    *ret_duration = ((Time) num [0] * 60) + ((Time) num [1]);
	    break;
	case 2:
	    unreachable ();
	    break;
	case 3:
	    *ret_duration = ((Time) num [0] * 3600) + ((Time) num [1] * 60) + ((Time) num [2]);
	    break;
	default:
	    unreachable ();
    }

    return Result::Success;
}

// @left and @right must be for the same timezone.
//
// tm_wday, tm_yday, tm_isdst fields of struct tm are ignored.
//
ComparisonResult compareTime (struct tm * mt_nonnull left,
                              struct tm * mt_nonnull right)
{
    if (left->tm_year > right->tm_year)
        return ComparisonResult::Greater;

    if (left->tm_year < right->tm_year)
        return ComparisonResult::Less;

    if (left->tm_mon > right->tm_mon)
        return ComparisonResult::Greater;

    if (left->tm_mon < right->tm_mon)
        return ComparisonResult::Less;

    if (left->tm_mday > right->tm_mday)
        return ComparisonResult::Greater;

    if (left->tm_mday < right->tm_mday)
        return ComparisonResult::Less;

    if (left->tm_hour > right->tm_hour)
        return ComparisonResult::Greater;

    if (left->tm_hour < right->tm_hour)
        return ComparisonResult::Less;

    if (left->tm_min > right->tm_min)
        return ComparisonResult::Greater;

    if (left->tm_min < right->tm_min)
        return ComparisonResult::Less;

    if (left->tm_sec > right->tm_sec)
        return ComparisonResult::Greater;

    if (left->tm_sec < right->tm_sec)
        return ComparisonResult::Less;

    return ComparisonResult::Equal;
}

}

