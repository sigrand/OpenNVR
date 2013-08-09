/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2012 Dmitry Shatrov

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


#ifndef LIBMARY__ASYNC_FILE__H__
#define LIBMARY__ASYNC_FILE__H__


#include <libmary/types.h>
#include <libmary/async_input_stream.h>
#include <libmary/async_output_stream.h>

#ifdef LIBMARY_PLATFORM_WIN32
#include <libmary/connection.h>
#endif


namespace M {

class AsyncFile : 
#ifdef LIBMARY_PLATFORM_WIN32
                  public Connection
#else
                  public AsyncInputStream,
                  public AsyncOutputStream
#endif
{
public:
    virtual mt_throws Result seek (FileOffset offset,
                                   SeekOrigin origin) = 0;

    virtual mt_throws Result tell (FileSize *ret_pos) = 0;

    virtual mt_throws Result sync () = 0;

    virtual mt_throws Result close (bool flush_data = true) = 0;

    AsyncFile () : DependentCodeReferenced (NULL) {}

    virtual ~AsyncFile () {}
};

}


#endif /* LIBMARY__ASYNC_FILE__H__ */

