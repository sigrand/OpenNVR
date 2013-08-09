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


#ifndef LIBMARY__NATIVE_FILE_LINUX__H__
#define LIBMARY__NATIVE_FILE_LINUX__H__


#include <libmary/file.h>


namespace M {

class NativeFile : public File
{
private:
    int fd;

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

    mt_throws Result stat (FileStat * mt_nonnull ret_stat);

    mt_throws Result getModificationTime (struct tm * mt_nonnull ret_tm);

    void setFd (int fd);

    // Resets fd so that it won't be closed in the destructor.
    void resetFd ();

#ifdef LIBMARY_ENABLE_MWRITEV
    int getFd () { return fd; }
#endif

    mt_throws Result open (ConstMemory filename,
			   Uint32      open_flags,
			   AccessMode  access_mode);

    // Deprecated in favor of open().
    mt_throws NativeFile (ConstMemory filename,
			  Uint32      open_flags,
			  AccessMode  access_mode);

     NativeFile (int fd);
     NativeFile ();
    ~NativeFile ();
};

}


#endif /* LIBMARY__NATIVE_FILE_LINUX__H__ */

