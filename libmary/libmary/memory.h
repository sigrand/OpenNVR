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


#ifndef LIBMARY__MEMORY__H__
#define LIBMARY__MEMORY__H__


// No libmary #include directives here.
// This file is meant to be included via libmary/types.h
#if !(defined (LIBMARY__TYPES__H__)  || \
      defined (LIBMARY__STRING__H__) || \
      defined (LIBMARY__UTIL_BASE__H__))
#error libmary/memory.h should not be included directly
#endif

namespace M {

class Memory
{
private:
    Byte *mem_;
    Size len_;

public:
    Byte* mem () const { return mem_; }
    Size  len () const { return len_; }

    // TODO rename to isEmpty(). isNull() is for "mem_ == NULL" test.
    bool isNull () const { return len_ == 0; }

    bool isEmpty () const { return len_ == 0; }

    Memory region (Size const region_start,
		   Size const region_len) const
    {
	assert (region_start <= len_);
	assert (len_ - region_start >= region_len);
	return Memory (mem_ + region_start, region_len);
    }

    Memory safeRegion (Size const region_start,
		       Size const region_len) const
    {
	if (region_start >= len_)
	    return Memory ();

	if (region_start + region_len > len_)
	    return Memory (mem_ + region_start, len_ - region_start);

	return Memory (mem_ + region_start, region_len);
    }

    Memory region (Size const region_start) const
    {
	assert (region_start <= len_);
	return Memory (mem_ + region_start, len_ - region_start);
    }

    Memory safeRegion (Size const region_start) const
    {
	if (region_start >= len_)
	    return Memory ();

	return Memory (mem_ + region_start, len_ - region_start);
    }

    Memory (Byte * const mem,
	    Size const len)
	: mem_ (mem),
	  len_ (len)
    {
    }

    Memory (char * const mem,
	    Size const len)
	: mem_ (reinterpret_cast <Byte*> (mem)),
	  len_ (len)
    {
    }

#if 0
    template <class T>
    Memory (T &obj)
	: mem_ (static_cast <Byte*> (&obj)),
	  len_ (sizeof (obj))
    {
    }
#endif

    template <class T>
    static Memory forObject (T &obj)
    {
	return Memory (reinterpret_cast <Byte*> (&obj), sizeof (obj));
    }

    Memory ()
	: mem_ (NULL),
	  len_ (0)
    {
    }
};

class ConstMemory
{
private:
    Byte const *mem_;
    Size len_;

public:
    Byte const * mem () const { return mem_; }
    Size len () const { return len_; }

    // TODO rename to isEmpty(). isNull() is for "mem_ == 0" test.
    bool isNull () const { return len_ == 0; }

    bool isEmpty () const { return len_ == 0; }

    ConstMemory region (Size const region_start,
			Size const region_len) const
    {
	assert (region_start <= len_);
	assert (len_ - region_start >= region_len);
	return ConstMemory (mem_ + region_start, region_len);
    }

    ConstMemory safeRegion (Size const region_start,
			    Size const region_len) const
    {
	if (region_start >= len_)
	    return ConstMemory ();

	if (region_start + region_len > len_)
	    return ConstMemory (mem_ + region_start, len_ - region_start);

	return ConstMemory (mem_ + region_start, region_len);
    }

    ConstMemory region (Size const region_start) const
    {
	assert (region_start <= len_);
	return ConstMemory (mem_ + region_start, len_ - region_start);
    }

    ConstMemory safeRegion (Size const region_start) const
    {
	if (region_start >= len_)
	    return ConstMemory ();

	return ConstMemory (mem_ + region_start, len_ - region_start);
    }

    ConstMemory& operator = (Memory const &memory)
    {
	mem_ = memory.mem ();
	len_ = memory.len ();
	return *this;
    }

    ConstMemory (Memory const &memory)
	: mem_ (memory.mem ()),
	  len_ (memory.len ())
    {
    }

    ConstMemory (Byte const * const mem,
		 Size const len)
	: mem_ (mem),
	  len_ (len)
    {
    }

    ConstMemory (char const * const mem,
		 Size const len)
	: mem_ (reinterpret_cast <Byte const *> (mem)),
	  len_ (len)
    {
    }

#if 0
    template <class T>
    ConstMemory (T &obj)
	: mem_ (static_cast <Byte const *> (&obj)),
	  len_ (sizeof (obj))
    {
    }
#endif

    template <class T>
    static ConstMemory forObject (T &obj)
    {
	return ConstMemory (reinterpret_cast <Byte const *> (&obj), sizeof (obj));
    }

    // TODO Questionable.
    template <Size N>
    ConstMemory (char const (&str) [N])
	: mem_ (reinterpret_cast <Byte const *> (str)),
	  len_ (sizeof (str) - 1)
    {
    }

    ConstMemory ()
	: mem_ (NULL),
	  len_ (0)
    {
    }
};

}


#endif /* LIBMARY__MEMORY__H__ */

