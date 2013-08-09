/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011, 2012 Dmitry Shatrov

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


#ifndef LIBMARY__FILE__H__
#define LIBMARY__FILE__H__


#include <libmary/types.h>
#include <libmary/exception.h>
#include <libmary/input_stream.h>
#include <libmary/output_stream.h>


namespace M {

class File : public InputStream,
             public OutputStream
{
public:
    typedef M::FileAccessMode AccessMode;
    typedef M::FileOpenFlags  OpenFlags;

    typedef M::FileType FileType;
    typedef M::FileStat FileStat;

    virtual mt_throws Result seek (FileOffset offset,
				   SeekOrigin origin) = 0;

    virtual mt_throws Result tell (FileSize *ret_pos) = 0;

    virtual mt_throws Result sync () = 0;

    virtual mt_throws Result close (bool flush_data = true) = 0;

#ifdef LIBMARY_ENABLE_MWRITEV
    virtual int getFd () = 0;
#endif

    virtual ~File () {}
};

}


#endif /* LIBMARY__FILE__H__ */

