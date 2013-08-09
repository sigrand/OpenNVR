/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011-2013 Dmitry Shatrov

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


#ifndef LIBMARY__VFS_POSIX__H__
#define LIBMARY__VFS_POSIX__H__


#include <sys/types.h>
#include <dirent.h>

#include <libmary/vfs.h>
#include <libmary/native_file.h>


namespace M {

class VfsPosix : public Vfs
{
private:
    Ref<String> root_path;

    ConstMemory makePath (Ref<String> &str_holder,
			  ConstMemory  path_suffix);

    ConstMemory makePathCstr (Ref<String> &str_holder,
			      ConstMemory  path_suffix);

    class VfsPosixFile : public Vfs::VfsFile
    {
        friend class VfsPosix;

    private:
        NativeFile native_file;

    public:
      mt_iface (Vfs::VfsFile)
        File* getFile () { return &native_file; }
      mt_iface_end
    };

    class VfsPosixDirectory : public Vfs::VfsDirectory
    {
	friend class VfsPosix;

    private:
	DIR *dir;

	mt_throws Result open (ConstMemory dirname);

	VfsPosixDirectory () : dir (NULL) {}

    public:
      mt_iface (Vfs::Directory)
	mt_throws Result getNextEntry (Ref<String> &ret_name);
	mt_throws Result rewind ();
      mt_iface_end

	~VfsPosixDirectory ();
    };

public:
  mt_iface (Vfs)
    mt_throws Result stat (ConstMemory  name,
                           FileStat    * mt_nonnull ret_stat);

    mt_throws Ref<VfsFile> openFile (ConstMemory    _filename,
                                     Uint32         open_flags,
                                     FileAccessMode access_mode);

    mt_throws Ref<Vfs::VfsDirectory> openDirectory (ConstMemory _dirname);

    mt_throws Result createDirectory (ConstMemory _dirname);

    mt_throws Result removeFile (ConstMemory _filename);

    mt_throws RemoveDirectoryResult removeDirectory (ConstMemory _dirname);
  mt_iface_end

    VfsPosix (ConstMemory root_path);
};

}


#endif /* LIBMARY__VFS_POSIX__H__ */

