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


#ifndef SCRUFFY__PP_ITEM_STREAM__H__
#define SCRUFFY__PP_ITEM_STREAM__H__


#include <libmary/libmary.h>

#include <pargen/file_position.h>

#include <scruffy/preprocessor_types.h>
#include <scruffy/parsing_exception.h>


namespace Scruffy {

using namespace M;

class PpItemStream : public StReferenced
{
public:
    class PositionMarker : public StReferenced
    {
    };

    enum PpItemResult {
	PpItemNormal = 0,
	PpItemEof
    };

  /* Virtual methods */

    /* If 'PpTokenNormal' is returned, then it is guaranteed that
     * '*pp_item' is non-null and contains the next item. */
    virtual PpItemResult getNextItem (StRef<PpItem> *pp_item)
			    throw (InternalException,
				   ParsingException) = 0;

    // header-name is a special kind of preprocessing token which
    // can occur only in an #include directive. Hence we need
    // a context-dependent hook for parsing it.
    //
    // If PpItemNormal is returned and ret_hn_token is non-null,
    // then the value of ret_hn_token determines if there was a match
    // after the call (if null, then no match).
    virtual PpItemResult getHeaderName (StRef<PpToken_HeaderName> *ret_hn_token)
				 throw (ParsingException,
					InternalException) = 0;

    virtual StRef<PositionMarker> getPosition ()
				      throw (InternalException) = 0;

    virtual void setPosition (PositionMarker *pmark)
		       throw (InternalException) = 0;

    virtual Pargen::FilePosition getFpos (PositionMarker *pmark) = 0;

    virtual Pargen::FilePosition getFpos () = 0;

  /* (End of virtual methods) */

    /* 'whsp' is set to null if there is any non-whitespace character
     * in the way. */
    PpItemResult getWhitespace (StRef<Whitespace> *whsp)
			 throw (InternalException,
				ParsingException);

    /* 'pp_token' is set to null if there is something in the way
     * which is not a preprocessing token. */
    PpItemResult getPpToken (StRef<PpToken> *pp_token)
		      throw (InternalException,
			     ParsingException);

    /* Skips all whitespace before the next preprocessing token.
     *
     * If 'PpTokenNormal' is returned, then it is guaranteed that
     * '*pp_token' is non-null and contains the next preprocessing token.
     * Otherwise it is guaranteed that '*pp_token' will be set to NULL
     * (as long as 'pp_token' is not NULL). */
    PpItemResult getNextPpToken (StRef<PpToken> *pp_token)
			  throw (InternalException,
				 ParsingException);
};

}


#endif /* SCRUFFY__PP_ITEM_STREAM__H__ */

