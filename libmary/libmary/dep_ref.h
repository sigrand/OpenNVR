/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2012-2013 Dmitry Shatrov

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


#ifndef LIBMARY__DEP_REF__H__
#define LIBMARY__DEP_REF__H__


#include <libmary/code_referenced.h>
#include <libmary/weak_ref.h>


namespace M {

template <class T>
class WeakDepRef
{
    template <class C> friend class CodeDepRef;

private:
    T *unsafe_obj;
    WeakRef<Object> weak_ref;

public:
    bool isValid () const { return weak_ref.isValid(); }

    T* getUnsafePtr () const { return unsafe_obj; }

    WeakDepRef& operator = (T * const obj)
    {
        Object * const container = obj ? obj->getCoderefContainer() : NULL;
        unsafe_obj = obj;
        weak_ref = container;
        return *this;
    }

    WeakDepRef (T * const obj)
        : unsafe_obj (obj),
          weak_ref (obj ? obj->getCoderefContainer() : NULL)
    {}

    WeakDepRef ()
        : unsafe_obj (NULL)
    {}
};

template <class T>
class DataDepRef
{
private:
    Object * const coderef_container;

    T *obj_ptr;
    Ref<Object> container_ref;

public:
       operator T* () const { return obj_ptr; }
    T* operator -> () const { return obj_ptr; }
    T*         ptr () const { return obj_ptr; }

    DataDepRef& operator = (T * const ptr)
    {
        obj_ptr = ptr;

        if (ptr) {
            Object * const ptr_container = ptr->getCoderefContainer();
            if (ptr_container == coderef_container)
                container_ref = NULL;
            else
                container_ref = ptr_container;
        } else {
            container_ref = NULL;
        }

        return *this;
    }

    DataDepRef (Object * const coderef_container)
        : coderef_container (coderef_container),
          obj_ptr (NULL)
    {}
};

template <class T>
class CodeDepRef
{
private:
    T *obj_ptr;
    Ref<Object> container_ref;

public:
       operator T* () const { return obj_ptr; }
    T* operator -> () const { return obj_ptr; }

    CodeDepRef& operator = (T * const ptr)
    {
        obj_ptr = ptr;
        container_ref = ptr ? ptr->getCoderefContainer() : NULL;
        return *this;
    }

    CodeDepRef (WeakDepRef<T> const &weak_ref)
    {
        if (weak_ref.weak_ref.isValid()) {
            container_ref = weak_ref.weak_ref.getRef();
            if (container_ref)
                obj_ptr = weak_ref.unsafe_obj;
            else
                obj_ptr = NULL;
        } else {
          // NULL coderef_container
            obj_ptr = weak_ref.unsafe_obj;
        }
    }

    CodeDepRef (T * const ptr)
        : obj_ptr (ptr),
          container_ref (ptr ? ptr->getCoderefContainer() : NULL)
    {}

    CodeDepRef ()
        : obj_ptr (NULL)
    {}
};

}


#endif /* LIBMARY__DEP_REF__H__ */

