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


#include <libmary/util_common.h>

#include <libmary/output_stream.h>


namespace M {

mt_throws Result
OutputStream::writev (struct iovec * const iovs,
		      Count          const num_iovs,
		      Size         * const ret_nwritten)
{
    if (ret_nwritten)
	*ret_nwritten = 0;

    Size total_written = 0;
    for (Count i = 0; i < num_iovs; ++i) {
	Size nwritten;
	Result const res = write (ConstMemory ((Byte const *) iovs [i].iov_base, iovs [i].iov_len), &nwritten);
	total_written += nwritten;
	if (!res) {
	    if (ret_nwritten)
		*ret_nwritten = total_written;

	    return res;
	}
    }

    if (ret_nwritten)
	*ret_nwritten = total_written;

    return Result::Success;
}

mt_throws Result
OutputStream::writeFull (ConstMemory   const mem,
			 Size        * const nwritten)
{
    return writeFull_common (this, mem, nwritten);
}

}

