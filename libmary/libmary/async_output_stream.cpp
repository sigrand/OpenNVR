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


#include <libmary/async_output_stream.h>


namespace M {

#ifndef LIBMARY_WIN32_IOCP
mt_throws AsyncIoResult
AsyncOutputStream::writev (struct iovec * const iovs,
			   Count          const num_iovs,
			   Size         * const ret_nwritten)
{
    if (ret_nwritten)
	*ret_nwritten = 0;

    Size total_written = 0;
    for (Count i = 0; i < num_iovs; ++i) {
        Size vec_written = 0;
        while (vec_written < iovs [i].iov_len) {
            Size nwritten = 0;
            AsyncIoResult const res = write (ConstMemory ((Byte const *) iovs [i].iov_base + vec_written,
                                                          iovs [i].iov_len - vec_written),
                                             &nwritten);
            total_written += nwritten;
            if (res != AsyncIoResult::Normal) {
                if (ret_nwritten)
                    *ret_nwritten = total_written;

                if (res == AsyncIoResult::Normal_Again)
                    return AsyncIoResult::Normal_Again;

                if (res == AsyncIoResult::Again) {
                    if (total_written > 0)
                        return AsyncIoResult::Normal_Again;

                    return AsyncIoResult::Again;
                }

                if (res == AsyncIoResult::Normal_Eof)
                    return AsyncIoResult::Normal_Eof;

                if (res == AsyncIoResult::Eof) {
                    if (total_written > 0)
                        return AsyncIoResult::Normal_Eof;

                    return AsyncIoResult::Eof;
                }

                assert (res == AsyncIoResult::Error);
                return AsyncIoResult::Error;
            }

            vec_written += nwritten;
        }
    }

    if (ret_nwritten)
	*ret_nwritten = total_written;

    return AsyncIoResult::Normal;
}
#endif /* LIBMARY_WIN32_IOCP */

}

