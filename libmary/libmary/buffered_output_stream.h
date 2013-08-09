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


#ifndef __LIBMARY__BUFFERED_OUTPUT_STREAM__H__
#define __LIBMARY__BUFFERED_OUTPUT_STREAM__H__


#include <libmary/output_stream.h>


namespace M {

class BufferedOutputStream : public OutputStream
{
private:
    OutputStream * const outs;

    Byte *data_buf;
    Size  data_len;

    Size in_buffer;

    mt_throws Result flushBuffer ();

public:
    mt_throws Result write (ConstMemory  mem,
			    Size        *ret_nwritten);

    mt_throws Result flush ();

    BufferedOutputStream (OutputStream * const mt_nonnull outs,
			  Size buf_len);

    ~BufferedOutputStream ();
};

}


#endif /* __LIBMARY__BUFFERED_OUTPUT_STREAM__H__ */

