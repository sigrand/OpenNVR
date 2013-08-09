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


#ifndef PARGEN__FILE_TOKEN_STREAM__H__
#define PARGEN__FILE_TOKEN_STREAM__H__


#include <libmary/libmary.h>

#include <pargen/token_stream.h>


namespace Pargen {

using namespace M;

class FileTokenStream : public TokenStream
{
private:
    File *file;

    bool const report_newlines;
    bool const minus_is_alpha;

    unsigned long cur_line;
    Uint64 cur_line_start;
    Uint64 cur_line_pos;
    Uint64 cur_char_pos;

    Byte *token_buf;
    Size token_len;
    Size const max_token_len;

public:
  mt_iface (TokenStream)
    mt_throws Result getNextToken (ConstMemory *ret_mem);

    mt_throws Result getPosition (PositionMarker * mt_nonnull ret_pmark);

    mt_throws Result setPosition (PositionMarker const *pmark);

    mt_throws Result getFilePosition (FilePosition *ret_fpos);

#if 0
    unsigned long getLine ();

    Ref<String> getLineStr ()
		     throw (InternalException);
#endif
  mt_iface_end

    FileTokenStream (File * mt_nonnull file,
		     bool  report_newlines = false,
                     bool  minus_is_alpha  = false,
                     Size  max_token_len = 4096);

    ~FileTokenStream ();
};

}


#endif /* PARGEN__FILE_TOKEN_STREAM__H__ */

