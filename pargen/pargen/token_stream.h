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


#ifndef PARGEN__TOKEN_STREAM__H__
#define PARGEN__TOKEN_STREAM__H__


#include <libmary/libmary.h>

#include <pargen/file_position.h>


namespace Pargen {

using namespace M;

class TokenStream
{
public:
    // Simplicity is sacrificed for efficiency here.
    // The point is to be able to use PositionMarkers as stack-allocated objects
    // and pass them around (almost) by value. But, in some cases, position
    // markers need to be more complex than the generalized form, hence we
    // provide a mechanism for implementing such complex markers (ptr,
    // copy_func, release_func).
    class PositionMarker
    {
    public:
	// Called for copying (copy constructor and assignment operator).
	// 'from->body.copy_func' is called for this purpose.
	typedef void (*CopyFunc) (PositionMarker       *to   /* non-null */,
				  PositionMarker const *from /* non-null */);

	// Destructor callback.
	typedef void (*ReleaseFunc) (PositionMarker *pmark /* non-null */);

	// POD type for memcpy()
	struct Body {
	    CopyFunc copy_func;
	    ReleaseFunc release_func;

	    FileSize offset;
	    unsigned long cur_line;
	    Uint64 cur_line_start;

	    void *ptr;
	};

	Body body;

	PositionMarker& operator = (PositionMarker const &pmark)
	{
	    if (this == &pmark)
		return *this;

	    if (body.release_func != NULL)
		body.release_func (this);

	    if (pmark.body.copy_func != NULL)
		pmark.body.copy_func (this, &pmark);
	    else
		memcpy (&this->body, &pmark.body, sizeof (body));

	    return *this;
	}

	PositionMarker (PositionMarker const &pmark)
	{
	    if (pmark.body.copy_func != NULL) {
		pmark.body.copy_func (this, &pmark);
	    } else
		memcpy (&this->body, &pmark.body, sizeof (body));
	}

	PositionMarker ()
	{
	    body.copy_func = NULL;
	    body.release_func = NULL;
	}

	~PositionMarker ()
	{
	    if (body.release_func != NULL)
		body.release_func (this);
	}
    };

    // If newlines are reported, then "\n" string will be returned for any whitespace
    // which contains one or more newlines (see newline(), isNewline() methods).
    //
    // The memory returned stays valid till the next call to getNextToken().
    virtual mt_throws Result getNextToken (ConstMemory *ret_mem) = 0;

    // User-defined objects may be associated with tokens by lower-level tokenizers.
    virtual mt_throws Result getNextToken (ConstMemory          * const ret_mem,
                                           StRef<StReferenced>  * const ret_user_obj,
                                           void                ** const ret_user_ptr)
    {
        if (ret_user_obj)
            *ret_user_obj = NULL;

        if (ret_user_ptr)
            *ret_user_ptr = NULL;

        return getNextToken (ret_mem);
    }

    virtual mt_throws Result getPosition (PositionMarker * mt_nonnull ret_pmark) = 0;

    virtual mt_throws Result setPosition (PositionMarker const * mt_nonnull pmark) = 0;

    virtual mt_throws Result getFilePosition (FilePosition *ret_fpos) = 0;

#if 0
    // TODO ?
    virtual unsigned long getLine ()
    {
	return 0;
    }

    // TODO ?
    virtual Ref<String> getLineStr ()
			     throw (InternalException)
    {
	return String::nullString ();
    }
#endif
};

}


#endif /* PARGEN__TOKEN_STREAM__H__ */

