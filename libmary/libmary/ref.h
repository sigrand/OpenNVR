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


#ifndef LIBMARY__REF__H__
#define LIBMARY__REF__H__


#include <libmary/types_base.h>
#include <libmary/util_base.h>


namespace M {

// Note: don't do static_cast <T*> (ref). Implicit casts should do the necessary
// conversions.
//
// Type T should have two methods to be usable with Ref<>:
//     * libMary_ref();
//     * libMary_unref.
// We want Ref<> to be usable with both BasicReferenced and Referenced classes.
// To get that, we require specific method to be present. The methods are name
// so that it is unlikely that they'll be overriden inadvertently.
//
template <class T>
class Ref
{
    template <class C> friend class Ref;

private:
    // 'obj' is (used to be) mutable for RetRef, which derives from class Ref.
    // We should be able to do the following:
    //
    //     template <class C>
    //     Ref (RetRef<C> const &ref)
    //         : obj (ref.obj)
    //     {
    //         ref.obj = NULL;
    //     }
    //
    // Note that C++0x move semantics references (&&) should allow to avoid
    // using 'mutable' here.
    //
    T *obj;

    void do_ref (T* const ref)
    {
        if (obj == ref)
            return;

        T * const old_obj = obj;

	obj = ref;
	if (ref)
	    /*static_cast <Referenced*>*/ (ref)->libMary_ref ();

        // Note that unref() may lead to a dtor call, the code for each
        // may require this Ref to be valid. That's why we change Ref's state
        // first, and call unref() last. Ths same applies to all other
        // calls to unref().
        if (old_obj)
            /*static_cast <Referenced*>*/ (old_obj)->libMary_unref ();
    }

public:
    template <class C>
    operator C* () const
    {
	// TODO Is this check necessary?
	//      It is here because I was afraid of incorrect casts.
	//      NULL pointer casts are treated specially by the standard, so this
	//      must be unnecessary.
	if (obj == NULL)
	    return NULL;

	return obj;
    }

    // This is necessary for implicit conversions (like conversions to bool).
    // "template <class C> operator C* ()" is not sufficient.
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
        // FIXME This is likely wrong: we should unref the object (-1 +0).
        if (obj == ref)
            return;

        T * const old_obj = obj;
	obj = ref;

	if (old_obj)
	    /*static_cast <Referenced*>*/ (old_obj)->libMary_unref ();
    }

    template <class C>
    void setNoUnref (C * const ref)
    {
        if (obj == ref)
            return;

	obj = ref;
	if (ref)
	    /*static_cast <Referenced*>*/ (obj)->libMary_ref ();
    }

    template <class C>
    void setNoRefUnref (C * const ref)
    {
        obj = ref;
    }

    template <class C>
    Ref& operator = (Ref<C> const &ref)
    {
        do_ref (ref.obj);
	return *this;
    }

    template <class C>
    Ref& operator = (Ref<C> &&ref)
    {
        if (obj != ref.obj) {
            T * const old_obj = obj;

            obj = ref.obj;
            ref.obj = NULL;

            if (old_obj)
                /* static_cast <Referenced*> */ (old_obj)->libMary_unref ();
        }

        return *this;
    }

    // Note that template <class C> Ref& opreator = (Ref<C> const &ref) does not
    // cover default assignment operator.
    Ref& operator = (Ref const &ref)
    {
	if (this == &ref)
	    return *this;

	do_ref (ref.obj);
	return *this;
    }

    Ref& operator = (Ref &&ref)
    {
        if (obj != ref.obj) {
            T * const old_obj = obj;

            obj = ref.obj;
            ref.obj = NULL;

            if (old_obj)
                /* static_cast <Referenced*> */ (old_obj)->libMary_unref ();
        }

        return *this;
    }

    template <class C>
    Ref& operator = (C* const ref)
    {
	do_ref (ref);
	return *this;
    }

    // This is necessary for the following to work:
    //     Ref<X> ref;
    //     ref = NULL;
    Ref& operator = (T* const ref)
    {
	do_ref (ref);
	return *this;
    }

    static Ref<T> createNoRef (T* const ref)
    {
	Ref<T> tmp_ref;
	tmp_ref.obj = ref;
	return tmp_ref;
    }

    template <class C>
    Ref (Ref<C> const &ref)
	: obj (ref.obj)
    {
	if (ref.obj != NULL)
	    /*static_cast <Referenced*>*/ (ref.obj)->libMary_ref ();
    }

    template <class C>
    Ref (Ref<C> &&ref)
        : obj (ref.obj)
    {
        ref.obj = NULL;
    }

    // Note that template <class C> Ref (Ref<C> const &ref) does not cover
    // default copy constructor.
    //
    // We presume that it is impossible to pass a reference to self to a copy
    // constructor.
    Ref (Ref const &ref)
	: obj (ref.obj)
    {
	if (ref.obj != NULL)
	    /*static_cast <Referenced*>*/ (ref.obj)->libMary_ref ();
    }

    Ref (Ref &&ref)
        : obj (ref.obj)
    {
        ref.obj = NULL;
    }

    template <class C>
    Ref (C* const ref)
	: obj (ref)
    {
	if (ref != NULL)
	    /*static_cast <Referenced*>*/ (ref)->libMary_ref ();
    }

    // This is necessary for the following to work:
    //     Ref<X> ref (NULL);
    Ref (T * const ref)
	: obj (ref)
    {
	if (ref != NULL)
	    /*static_cast <Referenced*>*/ (ref)->libMary_ref ();
    }

    Ref ()
	: obj (NULL)
    {
    }

    ~Ref ()
    {
	if (obj)
	    /*static_cast <Referenced*>*/ (obj)->libMary_unref ();
    }

  // MyCpp compatibility methods.

    T* ptr () const
    {
	return obj;
    }

    T& der () const
    {
	assert (obj);
	return *obj;
    }

    bool isNull () const
    {
	return obj == NULL;
    }
};

// "Grabbing" is needed because object's reference count is initiallized to '1'
// on object creation, which allows to use references freely in constructors.
template <class T>
Ref<T> grab (T * const obj)
{
    assert_hard (obj);
    return Ref<T>::createNoRef (obj);
}

}


#endif /* LIBMARY__REF__H__ */

