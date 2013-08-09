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


#include <libmary/libmary.h>

#include <scruffy/file_byte_stream.h>


// Internal
#define DEBUG_INT(a)


using namespace M;

namespace Scruffy {
    
ByteStream::ByteResult
FileByteStream::getNextByte (char *c_ret)
    throw (InternalException)
{
/*
    errf->print ("Scruffy.FileByteStream.getNextByte: "
		 "start_offset: ")
	 .print ((unsigned long long) start_offset)
	 .pendl ();
*/

//    in_file->seekSet (start_offset);
    if (!in_file->seek (start_offset, SeekOrigin::Beg))
        throw InternalException (InternalException::BackendError);

    unsigned char c;
    {
        IoResult const res = in_file->readFull (Memory::forObject (c), NULL /* ret_nread */);
        if (res == IoResult::Error)
            throw InternalException (InternalException::BackendError);

        if (res == IoResult::Eof) {
//            errs->println ("--- FileByteStream: EOF");
            return ByteEof;
        }

        assert (res == IoResult::Normal);
    }

    if (!in_file->tell (&start_offset))
        throw InternalException (InternalException::BackendError);

    if (c_ret != NULL)
	*c_ret = (char) c;

/*
    errf->print ("Scruffy.FileByteStream.getNextByte: "
		 "retuning byte: ")
	 .print ((unsigned long) (unsigned char) c)
	 .pendl ();
*/

//    errs->println ("--- FileByteStream: NORMAL");

    return ByteNormal;
}

StRef<ByteStream::PositionMarker>
FileByteStream::getPosition ()
    throw (InternalException)
{
    StRef<ByteStream::PositionMarker> const ret_pmark =
            st_grab (static_cast <ByteStream::PositionMarker*> (new (std::nothrow) PositionMarker));

    PositionMarker *pmark = static_cast <PositionMarker*> (ret_pmark.ptr ());
    pmark->offset = start_offset;

/*
    errf->print ("Scruffy.FileByteStream.getPosition: "
		 "returning ")
	 .print ((unsigned long long) start_offset)
	 .pendl ();
*/

    return ret_pmark;
}

void
FileByteStream::setPosition (ByteStream::PositionMarker *_pmark)
    throw (InternalException)
{
    PositionMarker *pmark = static_cast <PositionMarker*> (_pmark);
    start_offset = pmark->offset;

/*
    errf->print ("Scruffy.FileByteStream.setPosition: "
		 "position set to ")
	 .print ((unsigned long long) pmark->offset)
	 .pendl ();
*/
}

FileByteStream::FileByteStream (File *in_file)
    throw (InternalException)
{
    assert (in_file);

    this->in_file = in_file;
    if (!in_file->tell (&start_offset))
        throw InternalException (InternalException::BackendError);
}

}

