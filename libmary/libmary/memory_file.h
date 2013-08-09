/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2013 Dmitry Shatrov

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


#ifndef LIBMARY__MEMORY_FILE__H__
#define LIBMARY__MEMORY_FILE__H__


#include <libmary/file.h>


namespace M {

class MemoryFile : public File
{
protected:
    Memory array_mem;
    Size pos;

public:
  mt_iface (File)

    mt_iface (InputStream)

      mt_throws IoResult read (Memory  mem,
                               Size   *ret_nread);

    mt_iface_end

    mt_iface (OutputStream)

      mt_throws Result write (ConstMemory  mem,
                              Size        *ret_nwritten);

      mt_throws Result flush ();

    mt_iface_end

    mt_throws Result seek (FileOffset offset,
                           SeekOrigin origin);

    mt_throws Result tell (FileSize *ret_pos);

    mt_throws Result sync ();

    mt_throws Result close (bool flush_data = true);

  mt_iface_end

    MemoryFile (Memory mem);
};

}


#endif /* LIBMARY__MEMORY_FILE__H__ */

