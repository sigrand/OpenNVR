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

#include <scruffy/utf8_unichar_stream.h>


using namespace M;

namespace Scruffy {

UnicharStream::UnicharResult
Utf8UnicharStream::doGetNextUnichar (Unichar * const ret_uc)
{
    char c;
    ByteStream::ByteResult bres;
    bres = byte_stream->getNextByte (&c);
    if (bres == ByteStream::ByteEof)
        return UnicharEof;

    if (ret_uc)
        *ret_uc = (Unichar) c;

    return UnicharNormal;

#if 0
    char cbuf [7] = {};

    unsigned long i = 0;
    for (i = 0; i < 6; i++) {
	char c;
	ByteStream::ByteResult bres;
	bres = byte_stream->getNextByte (&c);
	if (bres == ByteStream::ByteEof) {
	    if (i != 0)
		break;

	    return UnicharEof;
	}

	if (bres != ByteStream::ByteNormal)
	    abortIfReached ();

	cbuf [i] = c;
	if (utf8_validate_sz (cbuf, NULL)) {
	    if (ret_uc != NULL)
		*ret_uc = utf8_valid_to_unichar (cbuf);

	    return UnicharNormal;
	}
    }

    errf->print ("Scruffy.Utf8UnicharStream: "
		 "input is not valid UTF-8")
	 .pendl ();

//    throw ParsingException (fpos);
    throw InternalException ();
#endif
}

unsigned
Utf8UnicharStream::skipNewline ()
{
    Unichar first_uc;
    if (doGetNextUnichar (&first_uc) != UnicharNormal)
        return 0;

    if (first_uc == 0x0d /* \r */) {
        Unichar second_uc;
        if (doGetNextUnichar (&second_uc) != UnicharNormal)
            return 0;

        if (second_uc == 0x0a /* \n */)
            return 2;

        return 0;
    }

    if (first_uc == 0x0a /* \n */)
        return 1;

    return 0;
}
    
UnicharStream::UnicharResult
Utf8UnicharStream::getNextUnichar (Unichar * const ret_uc)
    throw (InternalException)
{
//    errs->println ("--- utf8 getNextUnichar");

    byte_stream->setPosition (cur_pmark);

    Unichar uc;
    for (;;) {
        if (doGetNextUnichar (&uc) == UnicharEof) {
//            errs->println ("--- utf8 getNextUnichar: EOF");
            return UnicharEof;
        }

        ++fpos.char_pos;
        ++fpos.line_pos;
        cur_pmark = byte_stream->getPosition ();

        if (uc == 0x5c /* backslash */) {
            unsigned const newline_len = skipNewline ();
            if (newline_len > 0) {
                fpos.char_pos += newline_len;
                ++fpos.line;
                fpos.line_pos = 0;
                cur_pmark = byte_stream->getPosition ();
                continue;
            }

            byte_stream->setPosition (cur_pmark);
        }

        break;
    }

    bool is_newline = false;
    if (uc == 0x0d /* \r */) {
        Unichar second_uc;
        if (doGetNextUnichar (&second_uc) == UnicharNormal) {
            if (second_uc == 0x0a /* \n */) {
                uc = second_uc;
                ++fpos.char_pos;
                is_newline = true;
            } else {
                byte_stream->setPosition (cur_pmark);
            }
        } else {
            byte_stream->setPosition (cur_pmark);
        }
    } else
    if (uc == 0x0a /* \n */) {
        is_newline = true;
    }

    if (is_newline) {
        ++fpos.line;
        fpos.line_pos = 0;
    }

    if (ret_uc)
        *ret_uc = uc;

//    errs->println ("--- utf8 getNextUnichar: Normal");

    return UnicharNormal;
}

StRef<UnicharStream::PositionMarker>
Utf8UnicharStream::getPosition ()
    throw (InternalException)
{
    StRef<UnicharStream::PositionMarker> const ret_pmark =
            st_grab (static_cast <UnicharStream::PositionMarker*> (new (std::nothrow) PositionMarker));

    PositionMarker *pmark = static_cast <PositionMarker*> (ret_pmark.ptr ());
    pmark->byte_pmark = cur_pmark;
    pmark->fpos = fpos;

    return ret_pmark;
}

void
Utf8UnicharStream::setPosition (UnicharStream::PositionMarker *_pmark)
    throw (InternalException)
{
    assert (_pmark);

    PositionMarker *pmark = static_cast <PositionMarker*> (_pmark);
    cur_pmark = pmark->byte_pmark;
    fpos = pmark->fpos;
}

Pargen::FilePosition
Utf8UnicharStream::getFpos (UnicharStream::PositionMarker *_pmark)
{
    assert (_pmark);

    PositionMarker *pmark = static_cast <PositionMarker*> (_pmark);
    return pmark->fpos;
}

Pargen::FilePosition
Utf8UnicharStream::getFpos ()
{
    return fpos;
}

Utf8UnicharStream::Utf8UnicharStream (ByteStream *byte_stream)
    throw (InternalException)
{
    assert (byte_stream);

    this->byte_stream = byte_stream;
    cur_pmark = byte_stream->getPosition ();
}

}

