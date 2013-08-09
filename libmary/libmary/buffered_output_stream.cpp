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


#include <libmary/buffered_output_stream.h>


namespace M {

mt_throws Result
BufferedOutputStream::flushBuffer ()
{
    Size towrite = in_buffer;
    Size total_written = 0;
    while (towrite > 0) {
	Size nwritten;
	if (!outs->write (ConstMemory (data_buf + total_written, towrite), &nwritten)) {
	    // Note that we do not move the data to the beginning of the buffer,
	    // which means that stream's state becomes invalid.
	    // No further writes/flushes are allowed.
	    return Result::Failure;
	}

	assert (nwritten <= towrite);
	towrite -= nwritten;
	total_written += nwritten;
    }

    in_buffer = 0;

    return Result::Success;
}

mt_throws Result
BufferedOutputStream::write (ConstMemory   const mem,
			     Size        * const ret_nwritten)
{
    if (ret_nwritten)
	*ret_nwritten = 0;

    Byte const *src_buf = mem.mem();
    Size src_len = mem.len();

    for (;;) {
	Size towrite = src_len;
	if (data_len - in_buffer < towrite)
	    towrite = data_len - in_buffer;

	memcpy (data_buf + in_buffer, src_buf, towrite);
	in_buffer += towrite;

	if (towrite == src_len)
	    break;

	if (!flushBuffer ())
	    return Result::Failure;

	src_buf += towrite;
	src_len -= towrite;
    }

    if (ret_nwritten)
	*ret_nwritten = mem.len();

    return Result::Success;
}

mt_throws Result
BufferedOutputStream::flush ()
{
    if (!flushBuffer ())
	return Result::Failure;

    return outs->flush ();
}

BufferedOutputStream::BufferedOutputStream (OutputStream * const mt_nonnull outs,
					    Size const buf_len)
    : outs (outs),
      in_buffer (0)
{
    data_buf = new Byte [buf_len];
    assert (data_buf);
    data_len = buf_len;
}

BufferedOutputStream::~BufferedOutputStream ()
{
    delete[] data_buf;
}

}

