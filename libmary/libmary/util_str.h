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


#ifndef __LIBMARY__UTIL_STR__H__
#define __LIBMARY__UTIL_STR__H__


#include <libmary/util_str_base.h>
#include <libmary/exception.h>

namespace M {

mt_throws Result strToInt32 (char const  *cstr,
			     Int32       *ret_val,
			     char const **ret_endptr,
			     int          base = 0);

mt_throws Result strToInt32 (ConstMemory   mem,
			     Int32        *ret_val,
			     Byte const  **ret_endptr,
			     int           base = 0);

mt_throws Result strToInt64 (char const  *cstr,
			     Int64       *ret_val,
			     char const **ret_endptr,
			     int          base = 0);

mt_throws Result strToInt64 (ConstMemory   mem,
			     Int64        *ret_val,
			     Byte const  **ret_endptr,
			     int           base = 0);

mt_throws Result strToUint32 (char const  *cstr,
			      Uint32      *ret_val,
			      char const **ret_endptr,
			      int          base = 0);

mt_throws Result strToUint32 (ConstMemory   mem,
			      Uint32       *ret_val,
			      Byte const  **ret_endptr,
			      int           base = 0);

mt_throws Result strToUint64 (char const  *cstr,
			      Uint64      *ret_val,
			      char const **ret_endptr,
			      int          base = 0);

mt_throws Result strToUint64 (ConstMemory   mem,
			      Uint64       *ret_val,
			      Byte const  **ret_endptr,
			      int           base = 0);

mt_throws Result strToInt32_safe (char const *cstr,
				  Int32 *ret_val,
				  int base = 0);

mt_throws Result strToInt32_safe (ConstMemory const &mem,
				  Int32 *ret_val,
				  int base = 0);

mt_throws Result strToInt64_safe (char const *cstr,
				  Int64 *ret_val,
				  int base = 0);

mt_throws Result strToInt64_safe (ConstMemory const &mem,
				  Int64 *ret_val,
				  int base = 0);

mt_throws Result strToUint32_safe (char const *cstr,
				   Uint32 *ret_val,
				   int base = 0);

mt_throws Result strToUint32_safe (ConstMemory const &mem,
				   Uint32 *ret_val,
				   int base = 0);

mt_throws Result strToUint64_safe (char const *cstr,
				   Uint64 *ret_val,
				   int base = 0);

mt_throws Result strToUint64_safe (ConstMemory const &mem,
				   Uint64 *ret_val,
				   int base = 0);

mt_throws Result strToDouble_safe (char const *cstr,
				   double *ret_val);

mt_throws Result strToDouble_safe (ConstMemory const &mem,
				   double *ret_val);

}


#endif /* __LIBMARY__UTIL_STR__H__ */

