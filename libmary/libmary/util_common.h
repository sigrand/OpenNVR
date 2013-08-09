/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011, 2012 Dmitry Shatrov

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


#ifndef __LIBMARY__UTIL_COMMON__H__
#define __LIBMARY__UTIL_COMMON__H__


#include <libmary/types.h>
#include <libmary/exception.h>


namespace M {

void randomSetSeed (Uint32 seed);

Uint32 randomUint32 ();

template <class T>
mt_throws Result writeFull_common (T * const  dest,
				   ConstMemory const &mem,
				   Size * const ret_nwritten)
{
    Size total_written = 0;
    Result res = Result::Success;

    while (total_written < mem.len()) {
	Size last_written;
	res = dest->write (mem.region (total_written, mem.len() - total_written), &last_written);
	total_written += last_written;
	if (!res)
	    break;
    }

    if (ret_nwritten)
	*ret_nwritten = total_written;

    return res;
}

}


#endif /* __LIBMARY__UTIL_COMMON__H__ */

