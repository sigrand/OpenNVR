/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011 Dmitry Shatrov

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


#ifndef __LIBMARY__UTIL_DEV__H__
#define __LIBMARY__UTIL_DEV__H__


#include <libmary/types.h>
#include <libmary/output_stream.h>
#include <libmary/io.h>


namespace M {

void hexdump (OutputStream * mt_nonnull outs, ConstMemory const &mem);

static inline void hexdump (ConstMemory const &mem)
{
    hexdump (errs, mem);
}

}


#endif /* __LIBMARY__UTIL_DEV__H__ */

