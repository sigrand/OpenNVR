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


#ifndef __LIBMARY__ST_REF__H__
#define __LIBMARY__ST_REF__H__


#include <libmary/st_referenced.h>


namespace M {

template <class T>
class StRef
{
    template <class C> friend class StRef;

private:
    T *obj;

    void do_ref (T * const ref)
    {
        if (obj == ref)
            return;

        T * const old_obj = obj;

        obj = ref;
        if (ref)
            static_cast <StReferenced*> (ref)->libMary_st_ref ();

        if (old_obj)
            static_cast <StReferenced*> (old_obj)->libMary_st_unref ();
    }

public:
    T* ptr () const
    {
        return obj;
    }

    template <class C>
    operator C* () const
    {
        return obj;
    }

    operator T* () const
    {
        return obj;
    }

    T* operator -> () const
    {
        return obj;
    }

    T& operator * () const
    {
        return *obj;
    }

    template <class C>
    void setNoRef (C * const ref)
    {
        if (obj == ref)
            return;

        T * const old_obj = obj;

        obj = ref;

        if (old_obj)
            static_cast <StReferenced*> (old_obj)->libMary_st_unref ();
    }

    template <class C>
    void setNoUnref (C * const ref)
    {
        if (obj == ref)
            return;

        obj = ref;
        if (ref)
            static_cast <StReferenced*> (ref)->libMary_st_ref ();
    }

    template <class C>
    void setNoRefUnref (C * const ref)
    {
        obj = ref;
    }

    template <class C>
    StRef& operator = (StRef<C> const &ref)
    {
        do_ref (ref.obj);
        return *this;
    }

    template <class C>
    StRef& operator = (StRef<C> &&ref)
    {
        if (obj != ref.obj) {
            T * const old_obj = obj;

            obj = ref.obj;
            ref.obj = NULL;

            if (old_obj)
                static_cast <StReferenced*> (old_obj)->libMary_st_unref ();
        }

        return *this;
    }

    StRef& operator = (StRef const &ref)
    {
        if (this != &ref)
            do_ref (ref.obj);

        return *this;
    }

    StRef& operator = (StRef &&ref)
    {
        if (obj != ref.obj) {
            T * const old_obj = obj;

            obj = ref.obj;
            ref.obj = NULL;

            if (old_obj)
                static_cast <StReferenced*> (old_obj)->libMary_st_unref ();
        }

        return *this;
    }

    template <class C>
    StRef& operator = (C * const ref)
    {
        do_ref (ref);
        return *this;
    }

    StRef& operator = (T * const ref)
    {
        do_ref (ref);
        return *this;
    };

    static StRef<T> createNoRef (T * const ref)
    {
        StRef<T> tmp_ref;
        tmp_ref.obj = ref;
        return tmp_ref;
    }

    template <class C>
    StRef (StRef<C> const &ref)
        : obj (ref.obj)
    {
        if (ref.obj)
            static_cast <StReferenced*> (ref.obj)->libMary_st_ref ();
    }

    template <class C>
    StRef (StRef<C> &&ref)
        : obj (ref.obj)
    {
        ref.obj = NULL;
    }

    StRef (StRef const &ref)
        : obj (ref.obj)
    {
        if (ref.obj)
            static_cast <StReferenced*> (ref.obj)->libMary_st_ref ();
    }

    StRef (StRef &&ref)
        : obj (ref.obj)
    {
        ref.obj = NULL;
    }

    template <class C>
    StRef (C * const ref)
        : obj (ref)
    {
        if (ref)
            static_cast <StReferenced*> (ref)->libMary_st_ref ();
    }

    // This is necessary for the following to work:
    //     Ref<X> ref (NULL);
    StRef (T * const ref)
        : obj (ref)
    {
        if (ref)
            static_cast <StReferenced*> (ref)->libMary_st_ref ();
    }

    StRef ()
        : obj (NULL)
    {
    }

    ~StRef ()
    {
        if (obj)
            static_cast <StReferenced*> (obj)->libMary_st_unref ();
    }
};

template <class T>
StRef<T> st_grab (T * const obj)
{
    assert_hard (obj);
    return StRef<T>::createNoRef (obj);
}

}


#endif /* __LIBMARY__ST_REF__H__ */

