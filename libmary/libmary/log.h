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


#ifndef LIBMARY__LOG__H__
#define LIBMARY__LOG__H__


#include <libmary/types.h>
#include <cstdio>

#include <libmary/exception.h>
#include <libmary/mutex.h>
#include <libmary/output_stream.h>
#include <libmary/util_time.h>


namespace M {

extern OutputStream *logs;

class LogLevel
{
public:
    enum Value {
	All     =  1000,
        Stream  =  1500,
	Debug   =  2000,
	Info    =  3000,
	Warning =  4000,
	Access  =  5000,
	Error   =  6000,
	High    =  7000,
	Failure =  8000,
	None    = 10000,
	// Short loglevel name aliases are useful to enable/disable certain
	// loglevels from source quickly. Don't use them if you don't need
	// to flip between loglevels from time to time.
        S = Stream,
	D = Debug,
	I = Info,
	W = Warning,
	A = Access,
	E = Error,
	H = High,
	F = Failure,
	N = None
    };
    operator unsigned () const { return value; }
    LogLevel (unsigned const value) : value (value) {}
    LogLevel () {}

    char const * toCompactCstr () const;

    static Result fromString (ConstMemory  str,
			      LogLevel    * mt_nonnull ret_loglevel);

private:
    unsigned value;
};

class LogGroup
{
private:
    unsigned loglevel;

public:
    void setLogLevel (unsigned const loglevel) { this->loglevel = loglevel; }
    LogLevel getLogLevel () { return loglevel; }

    LogGroup (ConstMemory const &group_name, unsigned loglevel);
};

extern LogGroup libMary_logGroup_default;
extern LogLevel libMary_globalLogLevel;

static inline void setGlobalLogLevel (LogLevel const loglevel)
{
    libMary_globalLogLevel = loglevel;
}

static inline LogGroup* getDefaultLogGroup ()
{
    return &libMary_logGroup_default;
}

static inline unsigned getDefaultLogLevel ()
{
    return (unsigned) libMary_logGroup_default.getLogLevel ();
}

// TODO Rename to logLevelOn_()
static inline bool defaultLogLevelOn (unsigned const loglevel)
{
    return loglevel >= (unsigned) libMary_globalLogLevel &&
           loglevel >= getDefaultLogLevel();
}

#define logLevelOn_(loglevel) (defaultLogLevelOn (loglevel))

#define logLevelOn(group, loglevel)				\
	((loglevel) >= (unsigned) libMary_globalLogLevel &&	\
	 (loglevel) >= (unsigned) libMary_logGroup_ ## group .getLogLevel())

extern Mutex _libMary_log_mutex;

static inline void logLock ()
{
    _libMary_log_mutex.lock ();
}

static inline void logUnlock ()
{
    _libMary_log_mutex.unlock ();
}

// Note that it is possible to substitute variadic templates with a number of
// plaina templates while preserving the same calling syntax.

// TODO Roll this into new va-arg logs->print().
static inline void _libMary_do_log_unlocked (Format const & /* fmt */)
{
  // No-op
}

template <class T, class ...Args>
void _libMary_do_log_unlocked (Format const &fmt, T const &value, Args const &...args)
{
    logs->print_ (value, fmt);
    _libMary_do_log_unlocked (fmt, args...);
}

template <class ...Args>
void _libMary_do_log_unlocked (Format const & /* fmt */, Format const &new_fmt, Args const &...args)
{
    _libMary_do_log_unlocked (new_fmt, args...);
}

template <class ...Args>
void _libMary_log_unlocked (char const * const loglevel_str, Args const &...args)
{
    exc_push_scope ();

    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();

    Format fmt;
    fmt.min_digits = 2;

    Format fmt_frac;
    fmt_frac.min_digits = 4;

#if 0
    _libMary_do_log_unlocked (
	    fmt_def, "[", tlocal->localtime.tm_year + 1900, "/", fmt, tlocal->localtime.tm_mon + 1, "/", tlocal->localtime.tm_mday, " ",
	    tlocal->localtime.tm_hour, ":", tlocal->localtime.tm_min, ":", tlocal->localtime.tm_sec, " ",
	    ConstMemory::forObject (tlocal->timezone_str), "]",
	    loglevel_str);
#endif

    _libMary_do_log_unlocked (
	    fmt_def, tlocal->localtime.tm_year + 1900, "/", fmt, tlocal->localtime.tm_mon + 1, "/", tlocal->localtime.tm_mday, " ",
	    tlocal->localtime.tm_hour, ":", tlocal->localtime.tm_min, ":", tlocal->localtime.tm_sec, ".", fmt_frac, tlocal->time_log_frac,
	    loglevel_str);

    _libMary_do_log_unlocked (fmt_def, args...);
    logs->print_ ("\n", fmt_def);
    logs->flush ();

    exc_pop_scope ();
}

template <class ...Args>
void _libMary_log (char const * const loglevel_str, Args const &...args)
{
    logLock ();
    _libMary_log_unlocked (loglevel_str, args...);
    logUnlock ();
}

void _libMary_log_printLoglevel (LogLevel loglevel);

extern char const _libMary_loglevel_str_S [4];
extern char const _libMary_loglevel_str_D [4];
extern char const _libMary_loglevel_str_I [4];
extern char const _libMary_loglevel_str_W [4];
extern char const _libMary_loglevel_str_A [9];
extern char const _libMary_loglevel_str_E [4];
extern char const _libMary_loglevel_str_H [4];
extern char const _libMary_loglevel_str_F [4];
extern char const _libMary_loglevel_str_N [4];

// These macros allow to avoid evaluation of args when we're not going to put
// the message in the log.

#define _libMary_log_macro(log_func, group, loglevel, ...)				\
	do {										\
	    if (mt_unlikely ((loglevel) >= libMary_logGroup_ ## group .getLogLevel() &&	\
			     (loglevel) >= libMary_globalLogLevel))			\
	    {										\
		(log_func) (" ", (loglevel).toCompactCstr(), " ", __VA_ARGS__);		\
	    }										\
	} while (0)

#define _libMary_log_macro_s(log_func, group, loglevel, loglevel_str, ...)		\
	do {										\
	    if (mt_unlikely ((loglevel) >= libMary_logGroup_ ## group .getLogLevel() &&	\
			     (loglevel) >= libMary_globalLogLevel))			\
	    {										\
		(log_func) ((loglevel_str), __VA_ARGS__);				\
	    }										\
	} while (0)

#define log__(...)          _libMary_log_macro_s (_libMary_log,          default, LogLevel::None, _libMary_loglevel_str_N, __VA_ARGS__)
#define log_unlocked__(...) _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::None, _libMary_loglevel_str_N, __VA_ARGS__)

// log() macro would conflict with log() library function definition
#define log_plain(group, loglevel, ...)          _libMary_log_macro (_libMary_log,          group,   (loglevel), __VA_ARGS__)
#define log_unlocked(group, loglevel, ...) _libMary_log_macro (_libMary_log_unlocked, group,   (loglevel), __VA_ARGS__)
#define log_(loglevel, ...)                _libMary_log_macro (_libMary_log,          default, (loglevel), __VA_ARGS__)
#define log_unlocked_(loglevel, ...)       _libMary_log_macro (_libMary_log_unlocked, default, (loglevel), __VA_ARGS__)

#define logS(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::S, _libMary_loglevel_str_S, __VA_ARGS__)
#define logS_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::S, _libMary_loglevel_str_S, __VA_ARGS__)
#define logS_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::S, _libMary_loglevel_str_S, __VA_ARGS__)
#define logS_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::S, _libMary_loglevel_str_S, __VA_ARGS__)

#define logD(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::D, _libMary_loglevel_str_D, __VA_ARGS__)
#define logD_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::D, _libMary_loglevel_str_D, __VA_ARGS__)
#define logD_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::D, _libMary_loglevel_str_D, __VA_ARGS__)
#define logD_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::D, _libMary_loglevel_str_D, __VA_ARGS__)

#define logI(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::I, _libMary_loglevel_str_I, __VA_ARGS__)
#define logI_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::I, _libMary_loglevel_str_I, __VA_ARGS__)
#define logI_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::I, _libMary_loglevel_str_I, __VA_ARGS__)
#define logI_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::I, _libMary_loglevel_str_I, __VA_ARGS__)

#define logW(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::W, _libMary_loglevel_str_W, __VA_ARGS__)
#define logW_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::W, _libMary_loglevel_str_W, __VA_ARGS__)
#define logW_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::W, _libMary_loglevel_str_W, __VA_ARGS__)
#define logW_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::W, _libMary_loglevel_str_W, __VA_ARGS__)

#define logA(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::A, _libMary_loglevel_str_A, __VA_ARGS__)
#define logA_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::A, _libMary_loglevel_str_A, __VA_ARGS__)
#define logA_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::A, _libMary_loglevel_str_A, __VA_ARGS__)
#define logA_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::A, _libMary_loglevel_str_A, __VA_ARGS__)

#define logE(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::E, _libMary_loglevel_str_E, __VA_ARGS__)
#define logE_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::E, _libMary_loglevel_str_E, __VA_ARGS__)
#define logE_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::E, _libMary_loglevel_str_E, __VA_ARGS__)
#define logE_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::E, _libMary_loglevel_str_E, __VA_ARGS__)

#define logH(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::H, _libMary_loglevel_str_H, __VA_ARGS__)
#define logH_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::H, _libMary_loglevel_str_H, __VA_ARGS__)
#define logH_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::H, _libMary_loglevel_str_H, __VA_ARGS__)
#define logH_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::H, _libMary_loglevel_str_H, __VA_ARGS__)

#define logF(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::F, _libMary_loglevel_str_F, __VA_ARGS__)
#define logF_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::F, _libMary_loglevel_str_F, __VA_ARGS__)
#define logF_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::F, _libMary_loglevel_str_F, __VA_ARGS__)
#define logF_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::F, _libMary_loglevel_str_F, __VA_ARGS__)

typedef void (*LogStreamReleaseCallback) (void *cb_data);

void setLogStream (OutputStream             *new_logs,
                   LogStreamReleaseCallback  new_logs_release_cb,
                   void                     *new_logs_release_cb_data,
                   bool                      add_buffered_stream);

}


#endif /* LIBMARY__LOG__H__ */

