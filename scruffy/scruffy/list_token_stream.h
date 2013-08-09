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


#ifndef SCRUFFY__LIST_TOKEN_STREAM__H__
#define SCRUFFY__LIST_TOKEN_STREAM__H__


#include <libmary/libmary.h>

#include <pargen/token_stream.h>


namespace Scruffy {

using namespace M;

class ListTokenStream : public Pargen::TokenStream,
                        public StReferenced
{
private:
    List< StRef<Token> > * const token_list;
    List< StRef<Token> >::Element *cur_el;

public:
  mt_iface (Pargen::TokenStream)

    mt_throws Result getNextToken (ConstMemory * const ret_token)
    {
	return getNextToken (ret_token, NULL, NULL);
    }

    Result getNextToken (ConstMemory          * const ret_token,
                         StRef<StReferenced>  * const ret_user_obj,
                         void                ** const ret_user_ptr)
    {
	if (cur_el == NULL) {
	    if (ret_user_obj != NULL)
		*ret_user_obj = NULL;

	    if (ret_user_ptr != NULL)
		*ret_user_ptr = NULL;

            if (ret_token)
                *ret_token = ConstMemory();
            return Result::Success;
	}

	Token * const token = cur_el->data;

	if (ret_user_obj != NULL)
	    *ret_user_obj = token;

	if (ret_user_ptr != NULL)
	    *ret_user_ptr = token;

	cur_el = cur_el->next;

        if (ret_token)
            *ret_token = token->str->mem();
        return Result::Success;
    }

    mt_throws Result getPosition (Pargen::TokenStream::PositionMarker * const mt_nonnull ret_pmark)
    {
	ret_pmark->body.ptr = cur_el;
        return Result::Success;
    }

    mt_throws Result setPosition (Pargen::TokenStream::PositionMarker const * const mt_nonnull pmark)
    {
	cur_el = static_cast < List< StRef<Token> >::Element* > (pmark->body.ptr);
        return Result::Success;
    }

    mt_throws Result getFilePosition (Pargen::FilePosition * const ret_fpos)
    {
        if (ret_fpos)
            *ret_fpos = Pargen::FilePosition ();
        return Result::Success;
    }

  mt_iface_end

    ListTokenStream (List< StRef<Token> > * const mt_nonnull token_list)
	: token_list (token_list),
	  cur_el (token_list->first)
    {
    }
};

}


#endif /* SCRUFFY__LIST_TOKEN_STREAM__H__ */

