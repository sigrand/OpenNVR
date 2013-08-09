/*  Pargen - Flexible parser generator
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


#include <pargen/memory_token_stream.h>


#define DEBUG(a)


using namespace M;

namespace Pargen {

static inline bool is_whitespace (unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\v' || c == '\r' || c == '\n';
}

static inline bool is_newline (unsigned char c)
{
    return c == '\n';
}

mt_throws Result
MemoryTokenStream::getNextToken (ConstMemory * const ret_mem)
{
    DEBUG (
      logD_ (_func_);
    )

    if (!ret_mem)
        return Result::Success;

    Byte const * const buf = mem.mem();
    Size const len = mem.len();
    Size pos = cur_pos;
    for (Size const pos_end = len; pos < pos_end;) {
        char c = buf [pos];

        if (is_whitespace (c)) {
            bool newline = false;
            do {
                if (is_newline (buf [pos])) {
                    newline = true;

                    ++cur_line;
                    cur_line_start = pos + 1;
                }

                ++pos;
            } while (pos < len && is_whitespace (buf [pos]));

            if (newline && report_newlines) {
                DEBUG (
                  logD_ (_func, "reporting newline");
                )
                *ret_mem = newline_replacement;
                cur_pos = pos;
                return Result::Success;
            }

            cur_pos = pos;
            continue;
        }

        if (c == '/' && pos + 1 < pos_end) {
            if (buf [pos + 1] == '/') {
              // Single-line comment
                DEBUG (
                  logD_ (_func, "single-line comment");
                )
                pos += 2;
                for (; pos < pos_end; ++pos) {
                    if (is_newline (buf [pos])) {
                        ++pos;
                        break;
                    }
                }
                cur_pos = pos;
                continue;
            } else
            if (buf [pos + 1] == '*') {
              // Multiline comment
                DEBUG (
                  logD_ (_func, "multi-line comment");
                )
                pos += 2;
                for (; pos < pos_end; ++pos) {
                    if (is_newline (buf [pos])) {
                        ++cur_line;
                        cur_line_start = pos + 1;
                    } else
                    if (buf [pos] == '*' && pos + 1 < pos_end) {
                        if (buf [pos + 1] == '/') {
                            pos += 2;
                            break;
                        }
                    }
                }
                cur_pos = pos;
                continue;
            }
        }

        if (c == '"') {
            DEBUG (
              logD_ (_func, "string literal begin");
            )

            ++pos;
            bool escaped = false;
            bool escape_offs = 0;
            for (; pos < pos_end; ++pos) {
                if (pos - cur_pos - 1 >= max_token_len) {
                    // TODO Throw ParsingException
                    exc_throw (InternalException, InternalException::BadInput);
                    return Result::Failure;
                }

                if (escape_offs > 0)
                    token_buf [pos - cur_pos - 1 - escape_offs] = buf [pos];

                if (escaped) {
                    if (is_newline (buf [pos])) {
                      // Multiline string literal
                        ++escape_offs;

                        ++cur_line;
                        cur_line_start = pos + 1;
                    }

                    escaped = false;
                    continue;
                }

                if (is_newline (buf [pos])) {
                    // TODO Throw ParsingException
                    exc_throw (InternalException, InternalException::BadInput);
                    return Result::Failure;
                } else
                if (buf [pos] == '"') {
                    if (escape_offs > 0)
                        *ret_mem = ConstMemory (token_buf, pos - cur_pos - 1 - escape_offs);
                    else
                        *ret_mem = ConstMemory (buf + cur_pos + 1, pos - cur_pos - 1);

                    DEBUG (
                      logD_ (_func, "string literal end: ", *ret_mem);
                    )

                    cur_pos = pos + 1;
                    return Result::Success;
                } else
                if (buf [pos] == '\\') {
                    escaped = true;
                    ++escape_offs;
                    memcpy (token_buf, buf + cur_pos + 1, pos - cur_pos - 1);
                }
            }
        } else
        if (c >= '0' && c <= '9') {
            DEBUG (
              logD_ (_func, "numeric literal begin");
            )
            ++pos;
            for (; pos < pos_end; ++pos) {
                unsigned char const c = buf [pos];
                if (!((c >= '0' && c <= '9') ||
                      (c >= 'a' && c <= 'z') ||
                      (c >= 'A' && c <= 'Z') ||
                      c == '.'))
                {
                    *ret_mem = ConstMemory (buf + cur_pos, pos - cur_pos);
                    DEBUG (
                      logD_ (_func, "numeric literal end: ", *ret_mem);
                    )
                    cur_pos = pos;
                    return Result::Success;
                }
            }
        } else
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '_' ||
            (minus_is_alpha && c == '-'))
        {
            DEBUG (
              logD_ (_func, "literal begin");
            )
            ++pos;
            for (; pos < pos_end; ++pos) {
                c = buf [pos];
                if (!((c >= '0' && c <= '9') ||
                      (c >= 'a' && c <= 'z') ||
                      (c >= 'A' && c <= 'Z') ||
                      c == '_' ||
                      (minus_is_alpha && c == '-')))
                {
                    *ret_mem = ConstMemory (buf + cur_pos, pos - cur_pos);
                    DEBUG (
                      logD_ (_func, "literal end: ", *ret_mem);
                    )
                    cur_pos = pos;
                    return Result::Success;
                }
            }
        } else {
          // Single-char token
            *ret_mem = ConstMemory (buf + pos, 1);
            DEBUG (
              logD_ (_func, "single-char token: '", *ret_mem, "'");
            )
            ++pos;
            cur_pos = pos;
            return Result::Success;
        }

        ++pos;
    }

    DEBUG (
      logD_ (_func, "remainder: ", *ret_mem);
    )
    *ret_mem = ConstMemory (buf + cur_pos, pos - cur_pos);
    cur_pos = len;
    return Result::Success;
}

mt_throws Result
MemoryTokenStream::getPosition (PositionMarker * mt_nonnull ret_pmark)
{
    ret_pmark->body.offset = cur_pos;
    ret_pmark->body.cur_line = cur_line;
    ret_pmark->body.cur_line_start = cur_line_start;
    return Result::Success;
}

mt_throws Result
MemoryTokenStream::setPosition (PositionMarker const * const pmark)
{
    if (!pmark) {
        cur_pos = 0;
        return Result::Success;
    }

    cur_pos = (Size) pmark->body.offset;
    return Result::Success;
}

mt_throws Result
MemoryTokenStream::getFilePosition (FilePosition * const ret_fpos)
{
    if (ret_fpos) {
        *ret_fpos = FilePosition (cur_line,
                                  cur_pos - cur_line_start,
                                  cur_pos);
    }

    return Result::Success;
}

void
MemoryTokenStream::init (ConstMemory const mem,
                         bool        const report_newlines,
                         ConstMemory const newline_replacement,
                         bool        const minus_is_alpha,
                         Uint64      const max_token_len)
{
    this->mem = mem;

    this->report_newlines = report_newlines;
    this->newline_replacement = newline_replacement;
    this->minus_is_alpha = minus_is_alpha;

    this->max_token_len = max_token_len;

    token_buf = new (std::nothrow) Byte [max_token_len];
    assert (token_buf);
}

MemoryTokenStream::MemoryTokenStream ()
    : token_buf      (NULL),
      cur_pos        (0),
      cur_line       (0),
      cur_line_start (0)
{
}

MemoryTokenStream::~MemoryTokenStream ()
{
    delete[] token_buf;
}

}

