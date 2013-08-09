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

#include <libmary/input_stream.h>

//#include <libmary/io.h>


namespace M {

mt_throws IoResult
InputStream::readFull (Memory   const mem,
		       Size   * const ret_nread)
{
    Size bread = 0;
    IoResult res = IoResult::Normal;

    while (bread < mem.len()) {
	Size last_read;
	res = read (mem.region (bread, mem.len() - bread), &last_read);
        if (res == IoResult::Error)
            break;

        if (res == IoResult::Eof) {
//            errs->println (_func, "EOF, bread: ", bread);

            if (bread > 0)
                res = IoResult::Normal;

            break;
        }

        assert (res == IoResult::Normal);

//        errs->println (_func, "NORMAL, last_read: ", last_read);
	bread += last_read;
    }

    if (ret_nread)
	*ret_nread = bread;

//    errs->println (_func, "res: ", (Uint32) res, ", bread: ", bread);

    return res;
}

}

