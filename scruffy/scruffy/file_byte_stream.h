/*  MyLang - Utility library for writing parsers
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


#ifndef SCRUFFY__FILE_BYTE_STREAM__H__
#define SCRUFFY__FILE_BYTE_STREAM__H__


#include <libmary/libmary.h>

#include <scruffy/byte_stream.h>


namespace Scruffy {

using namespace M;

class FileByteStream : public ByteStream
{
protected:
    class PositionMarker : public ByteStream::PositionMarker
    {
    public:
	FileSize offset;
    };

    File *in_file;
    FileSize start_offset;

public:
  /* ByteStream interface */
  mt_iface (ByteStream)

    ByteResult getNextByte (char *c)
                     throw (InternalException);

    StRef<ByteStream::PositionMarker> getPosition ()
                                            throw (InternalException);

    void setPosition (ByteStream::PositionMarker *pmark)
               throw (InternalException);

  mt_iface_end

    FileByteStream (File *in_file)
	     throw (InternalException);
};

}


#endif /* SCRUFFY__FILE_BYTE_STREAM__H__ */

