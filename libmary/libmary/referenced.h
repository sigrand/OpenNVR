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


#ifndef LIBMARY__REFERENCED__H__
#define LIBMARY__REFERENCED__H__


#include <libmary/types.h>
#include <libmary/atomic.h>
#include <libmary/virt_referenced.h>
#include <libmary/avl_tree.h>
#include <libmary/util_base.h>
#include <libmary/debug.h>

//#include <atomic>


namespace M {

class Object;

class Referenced : public virtual VirtReferenced
{
    friend class Object;

private:
    AtomicInt refcount;
//    std::atomic<Size> refcount;

protected:
#ifdef LIBMARY_REF_TRACING
    mt_const bool traced;

    void traceRef ();
    void traceUnref ();
#endif

    virtual void last_unref ()
    {
	delete this;
    }

public:
#ifdef LIBMARY_REF_TRACING
    mt_const void traceReferences ()
    {
	traced = true;
    }
#endif

    void libMary_ref ()
    {
//	refcount.fetch_add (1, std::memory_order_relaxed);
	refcount.inc ();

#ifdef LIBMARY_REF_TRACING
	if (traced)
	    traceRef ();
#endif
    }

    void libMary_unref ()
    {
#ifdef LIBMARY_REF_TRACING
	if (traced)
	    traceUnref ();
#endif
#if 0
	if (refcount.fetch_sub (1, std::memory_order_release) != 1)
	    return;

	std::atomic_thread_fence (std::memory_order_acquire);
#endif

	if (refcount.decAndTest ())
	    last_unref ();
    }

    void ref ()
    {
	libMary_ref ();
    }

    void unref ()
    {
	libMary_unref ();
    }

    virtual void virt_ref ()
    {
	libMary_ref ();
    }

    virtual void virt_unref ()
    {
	libMary_unref ();
    }

    // For debugging purposes only.
    Count getRefCount () const
    {
	return refcount.get ();
//	return refcount.load ();
    }

    // Copying is allowed for MyCpp::Exception cloning mechanism to work.
    // There's no real reason to forbid copying of Referenced objects,
    // because it can be done transparently and with zero overhead.
    Referenced & operator = (Referenced const &)
    {
      // No-op
	return *this;
    }

    Referenced (Referenced const &)
	: VirtReferenced (),
	  refcount (1)
#ifdef LIBMARY_REF_TRACING
	  , traced (false)
#endif
    {
    }

    Referenced ()
	: refcount (1)
#ifdef LIBMARY_REF_TRACING
	  , traced (false)
#endif
    {
    }

    virtual ~Referenced () {}
};

template <class T>
class Referenced_UnrefAction
{
public:
    static void act (T * const obj)
    {
        static_cast <Referenced*> (obj)->libMary_unref ();
    }
};

}


#endif /* LIBMARY__REFERENCED__H__ */

