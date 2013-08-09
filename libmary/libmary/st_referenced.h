/*  LibMary - C++ library for high-performance network servers
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


#ifndef LIBMARY__ST_REFERENCED__H__
#define LIBMARY__ST_REFERENCED__H__


#include <libmary/types_base.h>


namespace M {

class StReferenced
{
private:
    unsigned refcount;

public:
    void libMary_st_ref () { ++refcount; }

    void libMary_st_unref ()
    {
        --refcount;
        if (refcount == 0)
            delete this;
    }

    void ref   () { libMary_st_ref   (); }
    void unref () { libMary_st_unref (); }

    // For debugging purposes only.
    unsigned getRefCount () const { return refcount; }

    StReferenced& operator = (StReferenced const &) { /* No-op */ return *this; }

    StReferenced (StReferenced const &) : refcount (1) {}
    StReferenced () : refcount (1) {}

    virtual ~StReferenced () {}
};

template <class T>
class StReferenced_UnrefAction
{
public:
    static void act (T * const obj)
        { static_cast <StReferenced*> (obj)->libMary_st_unref (); }
};

}


#endif /* LIBMARY__ST_REFERENCED__H__ */

