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


#ifndef __LIBMARY__REFERENCED__H__
#define __LIBMARY__REFERENCED__H__


#include <atomic>

#include <libmary/types.h>


namespace M {

class Referenced
{
private:
    std::atomic<Size> refcount;

    Referenced& operator = (Referenced const &);
    Referenced (Referenced const &);

public:
    void ref ()
    {
	refcount.fetch_add (1, std::memory_order_relaxed);
    }

    void unref ()
    {
	if (refcount.fetch_sub (1, std::memory_order_release) != 1)
	    return;

	std::atomic_thread_fence (std::memory_order_acquire);
	delete this;
    }

    // For debugging purposes only.
    Count getRefCount () const
    {
	return refcount.load ();
    }

    Referenced ()
	: refcount (1)
    {
    }

    virtual ~Referenced () {}
};

}


#endif /* __LIBMARY__REFERENCED__H__ */

