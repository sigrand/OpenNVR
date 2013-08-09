/*  Pargen - Flexible parser generator
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


#include <libmary/libmary.h>

#include <pargen/file_token_stream.h>


#define DEBUG(a) ;
// Flow
#define DEBUG_FLO(a) ;
// Internal
#define DEBUG_INT(a) ;


using namespace M;

namespace Pargen {

static inline bool
is_whitespace (unsigned char const c)
{
    return (c == ' ' || c == '\t' || c == '\v' || c == '\r' || c == '\n');
}

static inline bool
is_newline (unsigned char const c)
{
    return c == '\n';
}

static bool is_character (unsigned char const c,
                          bool          const minus_is_alpha)
{
    if ((c >= '0' && c <= '9') ||
	(c >= 'a' && c <= 'z') ||
	(c >= 'A' && c <= 'Z') ||
	(c == '_'))
    {
	return true;
    }

    if (minus_is_alpha && c == '-')
        return true;

    return false;
}

// A token is one of the following:
// a) a string of consequtive alphanumeric characters;
// b) a single non-alphanumeric character that is not whitespace.
mt_throws Result
FileTokenStream::getNextToken (ConstMemory * const ret_mem)
{
    // TODO This code is inefficient becasue it uses read(4Kb)+seek for
    //      every token. Using CachedFile is not enough because there's still
    //      a lot of unnecessary memory copying (bulk reads).
    //      Lexing logics should really be monolithic.

    // NOTE: Changing the size of 'buf' is useful for debugging.
    //
    // [10.09.07] I guess 4096 is way too much.
    unsigned char buf [4096];
    FileSize buf_start_pos;

    token_len = 0;

    for (;;) {
	if (!file->tell (&buf_start_pos))
            return Result::Failure;

	Size nread;
	IoResult const res = file->read (Memory::forObject (buf), &nread);
        if (res == IoResult::Error)
            return Result::Failure;

	if (res == IoResult::Eof) {
	    if (token_len > 0) {
                *ret_mem = ConstMemory (token_buf, token_len);
                return Result::Success;
            }

            *ret_mem = ConstMemory();
            return Result::Success;
	}

        assert (res == IoResult::Normal);

	if (nread == 0)
	    continue;

	DEBUG (
	  errs->print (_func, "\n");
	  hexdump (errs, ConstMemory::forObject (buf).region (0, nread));
	)

	// start_offset is offset to the first non-whitespace character in the buffer.
	unsigned long start_offset = 0;
	if (token_len == 0) {
	  // We've got no bytes for the token so far. That means that we're
	  // in the process of skipping leading whitespace. Let's skip some more.

	    bool got_newline = false;

	    for (start_offset = 0; start_offset < nread; start_offset++) {
		if (is_newline (buf [start_offset])) {
		    cur_line ++;
		    cur_line_start = buf_start_pos + start_offset + 1;
		    got_newline = true;
		} else
		if (!is_whitespace (buf [start_offset])) {
		  // This is where we detect the beginning of a new token.

		    assert (buf_start_pos + start_offset >= cur_line_start);
		    cur_line_pos = cur_line_start;
		    cur_char_pos = buf_start_pos + start_offset;

		    break;
		}
	    }

	    assert (start_offset <= nread);

	    if (got_newline && report_newlines) {
//		file->seekSet (buf_start_pos + start_offset);
                if (!file->seek (buf_start_pos + start_offset, SeekOrigin::Beg))
                    return Result::Failure;

		DEBUG (
                  errs->println (_func, "returning a newline");
		)
                *ret_mem = ConstMemory ("\n");
                return Result::Success;
	    }

	    if (start_offset == nread) {
	      // All of the bytes read are whitespace. We need to read some more.
		continue;
	    }
	}

      // Searching for the whitespace after the token, which marks token's end.

	// The naming is now a bit confusing.
	// 'got_whsp' means that we've got an end of the token in the buffer,
	// hence we know the whole token and it can be returned to the caller.
	// 'whsp' is the offset from the start of the buffer to the end of the token.
	unsigned long whsp;
	bool got_whsp = false;
	if (!is_character (buf [start_offset], minus_is_alpha)) {
	  // We've got a symbol that is not a character nor whitespace.
	  // This symbol should constitute a separate token.
	  // Hence, if the token has already started, then this symbol marks
	  // the end of the token. Otherwise, the symbol is the token.

	    if (token_len == 0) {
		whsp = start_offset + 1;
		got_whsp = true;
	    } else {
		whsp = start_offset;
		got_whsp = true;
	    }
	} else {
	    for (whsp = start_offset + 1; whsp < nread; whsp++) {
		if (!is_character (buf [whsp], minus_is_alpha)) {
		    got_whsp = true;
		    break;
		}
	    }
	}

	if (whsp > start_offset) {
            if (token_len + (whsp - start_offset) > max_token_len) {
                // Token length limit exceeded.
                exc_throw (InternalException, InternalException::BadInput);
                return Result::Failure;
            }

            memcpy (token_buf + token_len, buf + start_offset, whsp - start_offset);
            token_len += (whsp - start_offset);
	}

	if (got_whsp) {
//	    file->seekSet (buf_start_pos + whsp);
            if (!file->seek (buf_start_pos + whsp, SeekOrigin::Beg))
                return Result::Failure;

            DEBUG_INT (
              errs->println (_func, "token: \"", ConstMemory (token_buf, token_len), "\"");
            )
            *ret_mem = ConstMemory (token_buf, token_len);
            return Result::Success;
	}
    } /* for (;;) */

    unreachable ();
    return Result::Failure;
}

mt_throws Result
FileTokenStream::getPosition (PositionMarker * const mt_nonnull ret_pmark)
{
    if (!file->tell (&ret_pmark->body.offset))
        return Result::Failure;

    ret_pmark->body.cur_line = cur_line;
    ret_pmark->body.cur_line_start = cur_line_start;
    return Result::Success;
}

mt_throws Result
FileTokenStream::setPosition (PositionMarker const * const mt_nonnull pmark)
{
  DEBUG_FLO (
    errs->println (_func_);
  )

    cur_line = pmark->body.cur_line;
    cur_line_start = pmark->body.cur_line_start;
    cur_char_pos = pmark->body.offset;

//	file->seekSet (pmark->body.offset);
    if (!file->seek (pmark->body.offset, SeekOrigin::Beg))
        return Result::Failure;

    return Result::Success;
}

mt_throws Result
FileTokenStream::getFilePosition (FilePosition * const ret_fpos)
{
    if (ret_fpos) {
        *ret_fpos = FilePosition (cur_line,
                                  cur_line_pos,
                                  cur_char_pos);
    }

    return Result::Success;
}

#if 0
unsigned long
FileTokenStream::getLine ()
{
    return cur_line;
}

Ref<String>
FileTokenStream::getLineStr ()
    throw (InternalException)
try {
    Uint64 tmp_pos = file->tell ();
    file->seekSet (cur_line_start);

    Ref<Buffer> buf = grab (new Buffer (4096 + 1));
    unsigned long line_len = file->readLine (buf->getMemoryDesc (0, buf->getSize ()));

    file->seekSet (tmp_pos);

    return grab (new String (buf->getMemoryDesc (0, line_len)));
} catch (Exception &exc) {
    throw InternalException (String::nullString (), exc.clone ());
}
#endif

FileTokenStream::FileTokenStream (File * const mt_nonnull file,
				  bool   const report_newlines,
                                  bool   const minus_is_alpha,
                                  Size   const max_token_len)
    : file            (file),
      report_newlines (report_newlines),
      minus_is_alpha  (minus_is_alpha),
      cur_line        (0),
      cur_line_start  (0),
      cur_line_pos    (0),
      cur_char_pos    (0),
      token_len       (0),
      max_token_len  (max_token_len)
{
    assert (file);

    token_buf = new (std::nothrow) Byte [max_token_len];
    assert (token_buf);
}

FileTokenStream::~FileTokenStream ()
{
    delete token_buf;
}

}

