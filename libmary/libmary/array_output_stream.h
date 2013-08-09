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


#ifndef __LIBMARY__ARRAY_OUTPUT_STREAM__H__
#define __LIBMARY__ARRAY_OUTPUT_STREAM__H__


#include <libmary/output_stream.h>
#include <libmary/array.h>


namespace M {

class ArrayOutputStream : public OutputStream
{
private:
    Array * const arr;
    Size offset;

public:
    mt_iface (OutputStream)

      mt_throws Result write (ConstMemory   const mem,
			      Size        * const ret_nwritten)
      {
	  arr->set (offset, mem);
	  offset += mem.len();

	  if (ret_nwritten)
	      *ret_nwritten = mem.len();

	  return Result::Success;
      }

      mt_throws Result flush ()
      {
	// No-op
	  return Result::Success;
      }

    mt_iface_end

    ArrayOutputStream (Array * const mt_nonnull arr)
	: arr (arr),
	  offset (0)
    {
    }
};

}


#endif /* __LIBMARY__ARRAY_OUTPUT_STREAM__H__ */

