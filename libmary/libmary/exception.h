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


#ifndef LIBMARY__EXCEPTION__H__
#define LIBMARY__EXCEPTION__H__


#include <libmary/types.h>
#include <cstdlib>
#include <new>

#include <libmary/string.h>
#include <libmary/ref.h>
#include <libmary/libmary_thread_local.h>
#include <libmary/util_base.h>


namespace M {

class _libMary_ExcWrapper
{
public:
    Exception* operator -> () const
    {
        return libMary_getThreadLocal()->exc;
    }

    operator Exception* () const
    {
	return libMary_getThreadLocal()->exc;
    }
};

// TODO Use exc() to get the last exception.
extern _libMary_ExcWrapper exc;
//static inline Exception* exc () { return libMary_getThreadLocal()->exc; }

Ref<ExceptionBuffer> exc_swap ();

// Same as exc_swap(), but allows to optimize away refcount increments/decrements.
ExceptionBuffer* exc_swap_nounref ();

void exc_set (ExceptionBuffer *exc_buf);

// Same as exc_set(), but allows to optimize away refcount increments/decrements.
void exc_set_noref (ExceptionBuffer *exc_buf);

void exc_delete (ExceptionBuffer *exc_buf);

void exc_push_scope ();

void exc_pop_scope ();

void exc_none ();

// Note: T's constructor should not use exceptions.
//       We could allow that by wrapping 'new' within exc_push_scope/exc_pop_scope pair.
//       Consider adding exc_push_safector() which does such wrapping.
template <class T, class ...Args>
void exc_push__ (ConstMemory   const file,
                 ConstMemory   const func,
                 unsigned long const line,
                 Args const & ...args)
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();

    Exception * const cause = tlocal->exc;
    Exception * const data = tlocal->exc_buffer->push (sizeof (T), alignof (T));
    (void) static_cast <T*> (data);

    tlocal->exc = data;

    new (data) T (args...);
    data->cause = cause;
    data->file = file;
    data->func = func;
    data->line = line;
}

// See exc_push() for comments.
template <class T, class ...Args>
void exc_throw__ (ConstMemory   const file,
                  ConstMemory   const func,
                  unsigned long const line,
                  Args const & ...args)
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();

    Exception * const data = tlocal->exc_buffer->throw_ (sizeof (T), alignof (T));
    (void) static_cast <T*> (data);

    tlocal->exc = data;

    new (data) T (args...);
    data->cause = NULL;
    data->file = file;
    data->func = func;
    data->line = line;
}

// #define exc_throw(...) exc_throw_ (__FILE__, sizeof (__LINE__), __func__, sizeof (__func__), __LINE__, __VA_ARGS__)
#define exc_throw( T, ...) exc_throw__ <T> (ConstMemory (__FILE__), ConstMemory (__func__), __LINE__, __VA_ARGS__)
#define exc_throw_(T)      exc_throw__ <T> (ConstMemory (__FILE__), ConstMemory (__func__), __LINE__)
#define exc_push(  T, ...) exc_push__  <T> (ConstMemory (__FILE__), ConstMemory (__func__), __LINE__, __VA_ARGS__)
#define exc_push_( T)      exc_push__  <T> (ConstMemory (__FILE__), ConstMemory (__func__), __LINE__)


class InternalException : public Exception
{
public:
    enum Error {
	UnknownError,
	IncorrectUsage,
	BadInput,
	FrontendError,
	BackendError,
	BackendMalfunction,
	ProtocolError,
        IntegerOverflow,
        OutOfBounds,
	NotImplemented
    };

private:
    Error error;

public:
    Ref<String> toString ()
    {
	if (cause)
	    return catenateStrings ("InternalException: ", cause->toString ()->mem ());
	else
	    return grab (new (std::nothrow) String ("InternalException"));
    }

    InternalException (Error const error)
	: error (error)
    {
    }
};

class IoException : public Exception
{
public:
    Ref<String> toString ()
    {
	if (cause)
	    return catenateStrings ("IoException: ", cause->toString ()->mem ());
	else
	    return grab (new (std::nothrow) String ("IoException"));
    }
};

// Any error condition represented by an errno value.
// Not only for POSIX, but for Win32 as well.
//
class PosixException : public IoException // TODO There's no reason to inherit from IoException now.
{
public:
    int errnum;

    Ref<String> toString ()
    {
	return errnoToString (errnum);
    }

    PosixException (int const errnum)
	: errnum (errnum)
    {
    }
};

#if defined(LIBMARY_PLATFORM_WIN32) || defined(LIBMARY_PLATFORM_CYGWIN)
class WSAException : public IoException // TODO There's no reason to inherit from IoException now.
{
public:
    int wsa_error_code;

    Ref<String> toString ()
    {
        return wsaErrorToString (wsa_error_code);
    }

    WSAException (int const wsa_error_code)
        : wsa_error_code (wsa_error_code)
    {
    }
};

class Win32Exception : public IoException // TODO There's no reason to inherit from IoException now.
{
public:
    int error_code;

    Ref<String> toString ()
    {
        return win32ErrorToString (error_code);
    }

    Win32Exception (int const error_code)
        : error_code (error_code)
    {
    }
};
#endif

class NumericConversionException : public Exception
{
public:
    enum Kind {
	EmptyString,
	NonNumericChars,
	Overflow
    };

private:
    Kind kind;

public:
    Ref<String> toString ()
    {
	// TODO Catenate with exc->toString()
	//      What's a more effective method to catenate?

	switch (kind) {
	    case EmptyString:
		return grab (new (std::nothrow) String ("Empty string"));
	    case NonNumericChars:
		return grab (new (std::nothrow) String ("String contains non-numeric characters"));
	    case Overflow:
		return grab (new (std::nothrow) String ("Value overflow"));
	}

	unreachable ();
	return NULL;
    }

    NumericConversionException (Kind const kind)
	: kind (kind)
    {
    }
};

}


#endif /* LIBMARY__EXCEPTION__H__ */

