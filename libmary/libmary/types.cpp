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


#include <libmary/types.h>


namespace M {

ConstMemory
_libMary_stripFuncFilePath (char const * const str)
{
    if (!str)
        return ConstMemory ();

    size_t const len = strlen (str);

    size_t beg = 0;
    for (size_t i = len; i > 0; --i) {
        if (str [i - 1] == '/') {
            beg = i;
            break;
        }
    }

    if (len - beg >= 4
        && equal (ConstMemory (str + len - 4, 4), ".cpp"))
    {
        return ConstMemory (str + beg, len - beg - 4);
    }

    return ConstMemory (str + beg, len - beg);
}

Size
AsyncIoResult::toString_ (Memory const &mem,
			  Format const & /* fmt */) const
{
    switch (value) {
	case Normal:
	    return toString (mem, "AsyncIoResult::Normal");
#ifndef LIBMARY_WIN32_IOCP
	case Normal_Again:
	    return toString (mem, "AsyncIoResult::Normal_Again");
	case Normal_Eof:
	    return toString (mem, "AsyncIoResult::Normal_Eof");
#endif
	case Again:
	    return toString (mem, "AsyncIoResult::Again");
	case Eof:
	    return toString (mem, "AsyncIoResult::Eof");
	case Error:
	    return toString (mem, "AsyncIoResult::Error");
    }

    unreachable ();
    return 0;
}

}

