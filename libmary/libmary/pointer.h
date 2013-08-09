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


// Generic representation for reference types from MyCpp.


#ifndef __LIBMARY__POINTER__H__
#define __LIBMARY__POINTER__H__


#include <libmary/util_base.h>


namespace M {

template <class T> class Ref;

// Pointers provide a generic way to reference objects in MyCpp.
// T&, T* and Ref<T> can all be represented by the same Pointer<T>.
// This is convinient when writing templates: one can declare one
// method operating with a Pointer<T> instead of three separate methods
// for T&, T* and Ref<T> (plus necessary cv-variations).
//
// A pointer can be created using compact syntax "pointer(obj)".
// ConstPointer<T> and const_pointer(obj) should be used for pointers
// to const objects.

template <class T>
class Pointer
{
protected:
    T *ptr;

public:
    bool isNull () const
    {
	return ptr == NULL;
    }

    operator T* () const
    {
	return ptr;
    }

    T& operator * () const
    {
	assert (ptr);
	return *ptr;
    }

    T* operator -> () const
    {
	return ptr;
    }

    Pointer& operator = (T &obj)
    {
	this->ptr = &obj;
	return *this;
    }

    // Both "T*" and "T const *" land here. The latter fails because
    // 'this->ptr' if of type "T*".
    Pointer& operator = (T *ptr)
    {
	this->ptr = ptr;
	return *this;
    }

  // We must declare both "Ref<T> &" and "Ref<T> const &" versions to avoid
  // overlapping with "T &obj", where "T" is "Ref<T>" or "Ref<T> const".
  // Same applies to constructors.

    Pointer& operator = (Ref<T> &ref)
    {
	this->ptr = ref;
	return *this;
    }

    Pointer& operator = (Ref<T> const &ref)
    {
	this->ptr = ref;
	return *this;
    }

    Pointer (T &obj)
    {
	this->ptr = &obj;
    }

    // Both "T*" and "T const *" land here. The latter fails because
    // 'this->ptr' is of type "T*".
    Pointer (T *ptr)
    {
	this->ptr = ptr;
    }

    Pointer (Ref<T> &ref)
    {
	this->ptr = ref;
    }

    Pointer (Ref<T> const &ref)
    {
	this->ptr = ref;
    }

    Pointer ()
    {
	this->ptr = NULL;
    }
};

template <class T>
class ConstPointer
{
protected:
    T const *ptr;

public:
    bool isNull () const
    {
	return ptr == NULL;
    }

    operator T const * () const
    {
	return ptr;
    }

    T const & operator * () const
    {
	assert (ptr);
	return *ptr;
    }

    T const * operator -> () const
    {
	return ptr;
    }

    ConstPointer& operator = (T &obj)
    {
	this->ptr = &obj;
	return *this;
    }

    // Both "T*" and "T const *" land here.
    ConstPointer& operator = (T *ptr)
    {
	this->ptr = ptr;
	return *this;
    }

  // We must declare both "Ref<T> &" and "Ref<T> const &" versions to avoid
  // overlapping with "T &obj", where "T" is "Ref<T>" or "Ref<T> const".
  // Same applies to constructors.

    ConstPointer& operator = (Ref<T> &ref)
    {
	this->ptr = ref;
    }

    ConstPointer& operator = (Ref<T> const &ref)
    {
	this->ptr = ref;
    }

    ConstPointer& operator = (Pointer<T> &p)
    {
	this->ptr = p;
	return *this;
    }

    ConstPointer& operator = (Pointer<T> const &p)
    {
	this->ptr = p;
	return *this;
    }

    ConstPointer (T &obj)
    {
	this->ptr = &obj;
    }

    // Both "T*" and "T const *" land here.
    ConstPointer (T *ptr)
    {
	this->ptr = ptr;
    }

    ConstPointer (Ref<T> &ref)
    {
	this->ptr = ref;
    }

    ConstPointer (Ref<T> const &ref)
    {
	this->ptr = ref;
    }

    ConstPointer (Pointer<T> &p)
    {
	this->ptr = p;
    }

    ConstPointer (Pointer<T> const &p)
    {
	this->ptr = p;
    }

    ConstPointer ()
    {
	this->ptr = NULL;
    }
};

template <class T>
Pointer<T> pointer (T &obj)
{
    return Pointer<T> (&obj);
}

template <class T>
Pointer<T> pointer (T *ptr)
{
    return Pointer<T> (ptr);
}

template <class T>
Pointer<T> pointer (Ref<T> &ref)
{
    return Pointer<T> (ref);
}

template <class T>
Pointer<T> pointer (Ref<T> const &ref)
{
    return Pointer<T> (ref);
}

template <class T>
ConstPointer<T> const_pointer (T &ptr)
{
    return ConstPointer<T> (ptr);
}

template <class T>
ConstPointer<T> const_pointer (T *ptr)
{
    return ConstPointer<T> (ptr);
}

template <class T>
ConstPointer<T> const_pointer (Ref<T> &ref)
{
    return ConstPointer<T> (ref.ptr ());
}

template <class T>
ConstPointer<T> const_pointer (Ref<T> const &ref)
{
    return ConstPointer<T> (ref.ptr ());
}

}


#endif /* __LIBMARY__POINTER__H__ */

