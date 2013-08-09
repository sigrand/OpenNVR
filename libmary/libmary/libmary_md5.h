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


#ifndef __LIBMARY__LIBMARY_MD5__H__
#define __LIBMARY__LIBMARY_MD5__H__


#include <libmary/types.h>


namespace M {

// The hash in ASCII form is always 32 characters long.
//
// @result should be at least 32 bytes long.
//
void getMd5HexAscii (ConstMemory src,
                     Memory      result);

}


#endif /* __LIBMARY__LIBMARY_MD5__H__ */

