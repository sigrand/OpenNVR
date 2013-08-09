/*  LibMary - C++ library for high-performance network servers
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


#ifndef LIBMARY__ATOMIC__H__
#define LIBMARY__ATOMIC__H__


#include <libmary/types.h>
#ifdef LIBMARY_MT_SAFE
//#include <glib/gatomic.h>
#include <glib.h>
#endif


// Note that g_atomic_*_get/set() macros use some static assertion technique
// which doesn't play well with C++'s "this" pointer. This is reproducible
// with clang.


namespace M {

class AtomicInt
{
private:
#ifdef LIBMARY_MT_SAFE
    gint volatile value;
#else
    int value;
#endif

public:
    void set (int const value)
    {
#ifdef LIBMARY_MT_SAFE
        gint volatile * const ptr = &this->value;
	g_atomic_int_set (ptr, (gint) value);
#else
	this->value = value;
#endif
    }

    int get () const
    {
#ifdef LIBMARY_MT_SAFE
	return (int) g_atomic_int_get (&value);
#else
	return value;
#endif
    }

    void inc ()
    {
#ifdef LIBMARY_MT_SAFE
	g_atomic_int_inc (&value);
#else
	++value;
#endif
    }

    void dec ()
    {
        add (-1);
    }

    void add (int const a)
    {
#ifdef LIBMARY_MT_SAFE
	(void) g_atomic_int_add (&value, (gint) a);
#else
	value += a;
#endif
    }

    int fetchAdd (int const a)
    {
#ifdef LIBMARY_MT_SAFE
  #ifdef LIBMARY__OLD_GTHREAD_API
	return (int) g_atomic_int_exchange_and_add (&value, a);
  #else
        return (int) g_atomic_int_add (&value, a);
  #endif
#else
	int old_value = value;
	value += a;
	return old_value;
#endif

#if 0
// Alternate version.
	for (;;) {
	    gint const old = g_atomic_int_get (&value);
	    if (g_atomic_int_compare_and_exchange (&value, old, old + a))
		return old;
	}
	// unreachable
#endif
    }

    bool compareAndExchange (int const old_value,
			     int const new_value)
    {
#ifdef LIBMARY_MT_SAFE
	return (bool) g_atomic_int_compare_and_exchange (
			      &value, (gint) old_value, (gint) new_value);
#else
	if (value == old_value) {
	    value = new_value;
	    return true;
	}

	return false;
#endif
    }

    bool decAndTest ()
    {
#ifdef LIBMARY_MT_SAFE
	return (bool) g_atomic_int_dec_and_test (&value);
#else
	--value;
	if (value != 0)
	    return false;

	return true;
#endif
    }

    AtomicInt (int const value = 0)
    {
#ifdef LIBMARY_MT_SAFE
        gint volatile * const ptr = &this->value;
	g_atomic_int_set (ptr, (gint) value);
#else
	this->value = value;
#endif
    }
};

class AtomicPointer
{
private:
    // Mutable, because g_atomic_pointer_get() takes non-const
    // parameter in mingw.
    mutable gpointer volatile value;

public:
    void set (void * const value)
    {
#ifdef LIBMARY_MT_SAFE
        gpointer volatile * const ptr = &this->value;
	g_atomic_pointer_set (ptr, (gpointer) value);
#else
	this->value = value;
#endif
    }

    void* get () const
    {
#ifdef LIBMARY_MT_SAFE
        gpointer volatile * const ptr = &this->value;
	return (void*) g_atomic_pointer_get (ptr);
#else
	return value;
#endif
    }

    // _nonatomic methods have been added to reuse Object::shadow for
    // deletion queue list link pointer.
    void set_nonatomic (void * const value)
    {
	this->value = (gpointer) value;
    }

    void* get_nonatomic () const
    {
	return (void*) value;
    }

    bool compareAndExchange (void * const old_value,
			     void * const new_value)
    {
#ifdef LIBMARY_MT_SAFE
	return (bool) g_atomic_pointer_compare_and_exchange (&value,
							     (gpointer) old_value,
							     (gpointer) new_value);
#else
	if (value == old_value) {
	    value = new_value;
	    return true;
	}

	return false;
#endif
    }

    AtomicPointer (void * const value = NULL)
    {
#ifdef LIBMARY_MT_SAFE
        gpointer volatile * const ptr = &this->value;
	g_atomic_pointer_set (ptr, value);
#else
	this->value = value;
#endif
    }
};

#ifdef LIBMARY_MT_SAFE
extern volatile gint _libMary_dummy_mb_int;

static inline void full_memory_barrier ()
{
    // g_atomic_int_get() acts as a full compiler and hardware memory barrier.
    g_atomic_int_get (&_libMary_dummy_mb_int);
}
#else
static inline void full_memory_barrier () {}
#endif

}


#endif /* LIBMARY__ATOMIC__H__ */

