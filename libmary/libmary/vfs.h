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


#ifndef LIBMARY__VFS__H__
#define LIBMARY__VFS__H__


#include <libmary/types.h>
#include <libmary/exception.h>
#include <libmary/referenced.h>
#include <libmary/string.h>
#include <libmary/native_file.h>


namespace M {

class Vfs : public Referenced
{
public:
    typedef NativeFile::FileType FileType;
    typedef NativeFile::FileStat FileStat;

    class VfsFile : public Referenced
    {
    public:
        virtual File* getFile () = 0;
    };

    class VfsDirectory : public BasicReferenced
    {
    public:
	virtual mt_throws Result getNextEntry (Ref<String> &ret_name) = 0;
	virtual mt_throws Result rewind () = 0;
    };

    virtual mt_throws Result stat (ConstMemory  name,
                                   FileStat    * mt_nonnull ret_stat) = 0;

    virtual mt_throws Ref<VfsFile> openFile (ConstMemory    filename,
                                             Uint32         open_flags,
                                             FileAccessMode access_mode) = 0;

    virtual mt_throws Ref<VfsDirectory> openDirectory (ConstMemory dirname) = 0;

    virtual mt_throws Result createDirectory (ConstMemory dirname) = 0;

    virtual mt_throws Result removeFile (ConstMemory filename) = 0;

    class RemoveDirectoryResult
    {
    public:
        enum Value {
            Success,
            Failure,
            NotEmpty
        };
        operator Value () const { return value; }
        RemoveDirectoryResult (Value const value) : value (value) {}
        RemoveDirectoryResult () {}
    private:
        Value value;
    };

    virtual mt_throws RemoveDirectoryResult removeDirectory (ConstMemory dirname) = 0;

    mt_throws Result createSubdirs (ConstMemory dirs_path);

    mt_throws Result createSubdirsForFilename (ConstMemory const filename)
    {
        if (Byte const * const file_part = (Byte const*) memrchr (filename.mem(), '/', filename.len()))
            return createSubdirs (filename.region (0, file_part - filename.mem()));

        return Result::Success;
    }

    mt_throws Result removeSubdirs (ConstMemory dirs_path);

    mt_throws Result removeSubdirsForFilename (ConstMemory const filename)
    {
        if (Byte const * const file_part = (Byte const*) memrchr (filename.mem(), '/', filename.len()))
            return removeSubdirs (filename.region (0, file_part - filename.mem()));

        return Result::Success;
    }

    static mt_throws Ref<Vfs> createDefaultLocalVfs (ConstMemory root_path);

    virtual ~Vfs () {}
};

}


#endif /* LIBMARY__VFS__H__ */

