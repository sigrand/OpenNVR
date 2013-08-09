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


#include <libmary/util_str.h>
#include <libmary/log.h>

#include <libmary/util_dev.h>


namespace M {
    
void
hexdump (OutputStream * const mt_nonnull outs,
	 ConstMemory const &mem)
{
    static char const ascii_tab [256] = {
      //  0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // 00
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // 10
        '.', '!', '"', '#', '$', '%', '&', '\'','(', ')', '*', '+', ',', '-', '.', '/', // 20
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?', // 30
        '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', // 40
        'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\',']', '^', '_', // 50
        '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', // 60
        'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', '.', // 70
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // 80
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // 90
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // a0
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // b0
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // c0
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // d0
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // e0
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // f0
    };

    Count const num_rows = 16;
    Count const row_neighbours = 8;

    Byte line_buf [256];
    Memory line_mem = Memory::forObject (line_buf);

    for (Size i = 0; i < mem.len(); ) {
	Size offs = 0;

	offs += toString (line_mem.safeRegion (offs), "0x");
	{
	    Format fmt;
	    fmt.num_base = 16;
	    fmt.min_digits = 4;
	    offs += toString (line_mem.safeRegion (offs), i, fmt);
	}
	offs += toString (line_mem.safeRegion (offs), " |");

        size_t j_limit = mem.len() - i;
        if (j_limit > num_rows)
            j_limit = num_rows;

        size_t neigh = row_neighbours;
        for (size_t j = 0; j < num_rows; ++j) {
            if (neigh == 0) {
		offs += toString (line_mem.safeRegion (offs), " ");
                neigh = row_neighbours;
            }
            --neigh;

            if (j < j_limit) {
		offs += toString (line_mem.safeRegion (offs), " ");
		{
		    Format fmt;
		    fmt.num_base = 16;
		    fmt.min_digits = 2;
		    offs += toString (line_mem.safeRegion (offs), mem.mem() [i + j], fmt);
		}
            } else {
		offs += toString (line_mem.safeRegion (offs), "   ");
            }
        }

	offs += toString (line_mem.safeRegion (offs), " | ");

        neigh = row_neighbours;
        for (size_t j = 0; j < j_limit; ++j) {
            if (neigh == 0) {
		offs += toString (line_mem.safeRegion (offs), " ");
                neigh = row_neighbours;
            }
            --neigh;

	    offs += toString (line_mem.safeRegion (offs), ConstMemory::forObject (ascii_tab [mem.mem() [i + j]]));
        }

	outs->print_ (ConstMemory::forObject (line_buf).safeRegion (0, offs));
	outs->print_ ("\n");

        i += j_limit;
    }

    outs->flush ();
}

}

