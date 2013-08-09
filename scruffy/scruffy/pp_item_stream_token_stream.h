/*  Scruffy - C/C++ parser and source code analyzer
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


#ifndef SCRUFFY__PP_ITEM_STREAM_TOKEN_STREAM__H__
#define SCRUFFY__PP_ITEM_STREAM_TOKEN_STREAM__H__


#include <pargen/token_stream.h>

#include <scruffy/pp_item_stream.h>


namespace Scruffy {

using namespace M;

class PpItemStreamTokenStream : public Pargen::TokenStream
{
protected:
#if 0
    class PositionMarker : public Pargen::TokenStream::PositionMarker
    {
    public:
	StRef<PpItemStream::PositionMarker> pp_pmark;
    };
#endif

    StRef<PpItemStream> pp_stream;
    StRef<PpItemStream::PositionMarker> cur_pmark;

    StRef<String> newline_replacement;

public:
  mt_iface (Pargen::TokenStream)

    mt_throws Result getNextToken (ConstMemory *ret_mem);

    mt_throws Result getNextToken (ConstMemory          *ret_mem,
                                   StRef<StReferenced>  *ret_user_obj,
                                   void               **ret_user_ptr);

    mt_throws Result getPosition (Pargen::TokenStream::PositionMarker *ret_pmark /* non-null */);

    mt_throws Result setPosition (Pargen::TokenStream::PositionMarker const *pmark /* non-null */);

    mt_throws Result getFilePosition (Pargen::FilePosition *ret_fpos)
    {
        *ret_fpos = Pargen::FilePosition ();
        return Result::Success;
    }

  mt_iface_end

    void setNewlineReplacement (ConstMemory const &replacement)
    {
	newline_replacement = st_grab (new (std::nothrow) String (replacement));
    }

    PpItemStreamTokenStream (PpItemStream *pp_stream);
};

}


#endif /* SCRUFFY__PP_ITEM_STREAM_TOKEN_STREAM__H__ */

