/*  LibMary - C++ library for high-performance network servers
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


#ifndef __LIBMARY__BASIC_REFERENCED__H__
#define __LIBMARY__BASIC_REFERENCED__H__


#include <libmary/types_base.h>


namespace M {

// TODO Get rid of BasicReferenced altogether (in favor of StReferenced+StRef).
class BasicReferenced
{
private:
    Uint32 ref_count;

    BasicReferenced& operator = (BasicReferenced const &);
    BasicReferenced (BasicReferenced const &);

public:
    void libMary_ref ()
    {
	++ref_count;
    }

    void libMary_unref ()
    {
	--ref_count;
	if (ref_count == 0)
	    delete this;
    }

    void ref ()
    {
	libMary_ref ();
    }

    void unref ()
    {
	libMary_unref ();
    }

    // For debugging purposes only.
    Count getRefCount () const
    {
	return ref_count;
    }

    BasicReferenced ()
	: ref_count (1)
    {
    }

    virtual ~BasicReferenced () {}
};

}


#endif /* __LIBMARY__BASIC_REFERENCED__H__ */

