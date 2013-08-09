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


#ifndef SCRUFFY__UTF8_UNICHAR_STREAM__H__
#define SCRUFFY__UTF8_UNICHAR_STREAM__H__


#include <libmary/libmary.h>

#include <scruffy/unichar_stream.h>
#include <scruffy/byte_stream.h>


namespace Scruffy {

using namespace M;

class Utf8UnicharStream : public UnicharStream
{
protected:
    class PositionMarker : public UnicharStream::PositionMarker
    {
    public:
	StRef<ByteStream::PositionMarker> byte_pmark;
        Pargen::FilePosition fpos;
    };

    StRef<ByteStream> byte_stream;
    StRef<ByteStream::PositionMarker> cur_pmark;

    Pargen::FilePosition fpos;

    UnicharResult doGetNextUnichar (Unichar *ret_uc);

    unsigned skipNewline ();

public:
  mt_iface (UnicharStream)

    UnicharResult getNextUnichar (Unichar *ret_uc)
			    throw (InternalException);

    StRef<UnicharStream::PositionMarker> getPosition ()
			    throw (InternalException);

    void setPosition (UnicharStream::PositionMarker *pmark)
			    throw (InternalException);

    Pargen::FilePosition getFpos (UnicharStream::PositionMarker *_pmark);

    Pargen::FilePosition getFpos ();

  mt_iface_end

    Utf8UnicharStream (ByteStream *byte_stream)
		throw (InternalException);
};

}


#endif /* SCRUFFY__UTF8_UNICHAR_STREAM__H__ */

