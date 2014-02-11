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


#ifndef LIBMARY__TYPES__H__
#define LIBMARY__TYPES__H__


#include <libmary/types_base.h>

#ifdef LIBMARY_PLATFORM_WIN32
// winsock2.h has a #warning telling to include it before windows.h
#include <winsock2.h>
#include <windows.h>
#endif

#include <libmary/memory.h>


namespace M {

typedef void (*VoidFunction) (void);
typedef void (GenericCallback) (void *cb_data);

class EmptyBase {};

ConstMemory _libMary_stripFuncFilePath (char const *str);

// TODO Move to log.h, append current session id string (log_prefix)
// Evil macro to save a few keystrokes for logE_ (_func, ...)

// _func2 and _func3 are a workaround to stringify __LINE__.

#define _func3(line, ...)                                                                       \
	_libMary_stripFuncFilePath (__FILE__),							\
	":" #line,								 		\
	":", __func__, ":",								        \
	ConstMemory ("                                         " /* 41 spaces */, 		\
                     ({ Size const file_len = _libMary_stripFuncFilePath (__FILE__).len();      \
                       file_len + sizeof (#line) + sizeof (__func__) + 3 < 40 + 1 ?             \
                               40 - file_len - sizeof (#line) - sizeof (__func__) - 3 + 1 : 1; })) \
        __VA_ARGS__

#define _func3_(line, ...)							 		\
	_libMary_stripFuncFilePath (__FILE__),							\
	":" #line,								 		\
	":" , __func__,								\
	ConstMemory ("                                         " /* 41 spaces */, 		\
                     ({ Size const file_len = _libMary_stripFuncFilePath (__FILE__).len();      \
                        file_len + sizeof (#line) + sizeof (__func__) + 2 < 40 + 1 ?	        \
                                40 - file_len - sizeof (#line) - sizeof (__func__) - 2 + 1 : 1; })) \
        __VA_ARGS__

// No line padding  #define _func3(line) __FILE__ ":" #line ":", __func__
#define _func2( line, ...) _func3 (line __VA_ARGS__)
#define _func2_(line, ...) _func3_(line __VA_ARGS__)

#define _func       _func2 (__LINE__)
#define _this_func  _func2 (__LINE__, , , fmt_hex, "0x", (ptrdiff_t) this, fmt_def, " ")
#define _self_func  _func2 (__LINE__, , , fmt_hex, "0x", (ptrdiff_t) self, fmt_def, " ")
#define _func_      _func2_(__LINE__)
#define _this_func_ _func2_(__LINE__, , , fmt_hex, " 0x", (ptrdiff_t) this, fmt_def, " ")
#define _self_func_ _func2_(__LINE__, , , fmt_hex, " 0x", (ptrdiff_t) self, fmt_def, " ")

class Result
{
public:
    enum Value {
	Failure = 0,
	Success = 1
    };
    operator Value () const { return value; }
    Result (Value const value) : value (value) {}
    Result () {}
private:
    Value value;
};

// One should be able to write if (!compare()), whch should mean the same as
// if (compare() == ComparisonResult::Equal).
//
class ComparisonResult
{
public:
    enum Value {
	Less    = -1,
	Equal   = 0,
	Greater = 1
    };
    operator Value () const { return value; }
    ComparisonResult (Value const value) : value (value) {}
    ComparisonResult () {}
private:
    Value value;
};

// TODO For writes, Eof is an error condition, actually.
//      It will _always_ be an error condition.
//      Eof for writes may be detected by examining 'exc'.
class IoResult
{
public:
    enum Value {
	Normal = 0,
	Eof,
	Error
    };
    operator Value () const { return value; }
    IoResult (Value const value) : value (value) {}
    IoResult () {}
private:
    Value value;
};

class SeekOrigin
{
public:
    enum Value {
	Beg = 0,
	Cur,
	End
    };
    operator Value () const { return value; }
    SeekOrigin (Value const value) : value (value) {}
    SeekOrigin () {}
private:
    Value value;
};

#ifdef LIBMARY_PLATFORM_WIN32
struct iovec {
    void   *iov_base;
    size_t  iov_len;
};

#define IOV_MAX 1024
#endif

class Object;

}

#include <libmary/virt_ref.h>

namespace M {

template <class T>
class CbDesc
{
public:
    T const        * const cb;
    void           * const cb_data;
    Object         * const coderef_container;

// Using VirtRef instead of a pointer to avoid loosing reference to 'ref_data'
// when using CbDesc<> as function's return value. There has been no real
// problems because of this so far, but this still looks dangerous.
//    VirtReferenced * const ref_data;
    VirtRef          const ref_data;

    T const * operator -> () const
    {
	return cb;
    }

    CbDesc (T const        * const cb,
	    void           * const cb_data,
	    Object         * const coderef_container,
	    VirtReferenced * const ref_data = NULL)
	: cb                (cb),
	  cb_data           (cb_data),
	  coderef_container (coderef_container),
	  ref_data          (ref_data)
    {
    }

    CbDesc ()
	: cb (NULL),
	  cb_data (NULL),
	  coderef_container (NULL),
	  ref_data (NULL)
    {
    }
};

class Format;

class AsyncIoResult
{
public:
    enum Value {
	Normal,
#ifndef LIBMARY_WIN32_IOCP
	// We've got the data and we know for sure that the following call to
	// read() will return EAGAIN.
	Normal_Again,
	// Normal_Eof is usually returned when we've received Hup event for
	// the connection, but there was some data to read.
	Normal_Eof,
#endif
	Again,
	Eof,
	Error
    };
    operator Value () const { return value; }
    AsyncIoResult (Value const value) : value (value) {}
    AsyncIoResult () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
private:
    Value value;
};

class FileAccessMode
{
public:
    enum Value {
        ReadOnly = 0,
        WriteOnly,
        ReadWrite
    };
    operator Value () const { return value; }
    FileAccessMode (Value value) : value (value) {}
    FileAccessMode () {}
private:
    Value value;
};

class FileOpenFlags
{
public:
    enum Value {
        Create   = 0x1,
        Truncate = 0x2,
        Append   = 0x4
    };
    operator Uint32 () const { return value; }
    FileOpenFlags (Value value) : value (value) {}
    FileOpenFlags () {}
private:
    Value value;
};

class FileType
{
public:
    enum Value {
        BlockDevice,
        CharacterDevice,
        Fifo,
        RegularFile,
        Directory,
        SymbolicLink,
        Socket
    };
    operator Value () const { return value; }
    FileType (Value const value) : value (value) {}
    FileType () {}
private:
    Value value;
};

class FileStat
{
public:
    unsigned long long size;
    FileType file_type;
};

}


#if defined __MACH__ || defined (LIBMARY_PLATFORM_WIN32)
static inline void* memrchr (const void *s, int c, size_t n)
{
    for (int i = n; i > 0; --i) {
        if (((char*) s) [i - 1] == c)
            return (char*) s + (i - 1);
    }

    return NULL;
}
#endif


// For class Format.
#include <libmary/util_str_base.h>


#endif /* LIBMARY__TYPES__H__ */

