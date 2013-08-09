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


#include <libmary/libmary_md5.h>

#include "md5/md5.h"


namespace M {

void getMd5HexAscii (ConstMemory const src,
                     Memory      const result)
{
    assert (result.len() >= 32);

    md5_state_t state;
    md5_byte_t digest [16];

    md5_init (&state);
    md5_append (&state, (md5_byte_t const *) src.mem(), src.len());
    md5_finish (&state, digest);

    for (int i = 0; i < 16; ++i) {
        unsigned char const low  =  digest [i] & 0x0f;
        unsigned char const high = (digest [i] & 0xf0) >> 4;

        unsigned long const pos = i << 1;

        if (high < 10)
            result.mem() [pos] = high + '0';
        else
            result.mem() [pos] = high - 10 + 'a';

        if (low < 10)
            result.mem() [pos + 1] = low + '0';
        else
            result.mem() [pos + 1] = low - 10 + 'a';
    }
}

}

