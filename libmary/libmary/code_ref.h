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


#ifndef LIBMARY__CODE_REF__H__
#define LIBMARY__CODE_REF__H__


#include <libmary/types_base.h>
#include <libmary/code_referenced.h>
#include <libmary/object.h>
#include <libmary/ref.h>
#include <libmary/weak_ref.h>
#include <libmary/libmary_thread_local.h>


#ifdef DEBUG
#error DEBUG already defined
#endif
#define DEBUG(a)
#if DEBUG(1) + 0
#include <cstdio>
#endif


namespace M {

class CodeRef;

class WeakCodeRef
{
    friend class CodeRef;

private:
    // This is Ref<Object::Shadow>, actually.
    WeakRef<Object> weak_ref;

public:
    bool isValid () const
    {
	return weak_ref.isValid();
    }

    Object::Shadow* getShadowPtr () const
    {
        return weak_ref.getShadowPtr();
    }

    WeakCodeRef& operator = (CodeReferenced * const obj)
    {
	Object * const container = obj ? obj->getCoderefContainer() : NULL;
	weak_ref = container;
	return *this;
    }

    WeakCodeRef (CodeReferenced * const obj)
        : weak_ref (obj ? obj->getCoderefContainer() : NULL)
    {
    }

    WeakCodeRef ()
    {
    }
};


// TODO Ref<X> ref = weak_ref;
//      if (ref) { ... }
//      ^^^ Такая запись лучше с т з оптимизации, чем "Ref<X> ref = weak_ref.getRef();"
//          Это особенно важно потому, что операции с AtomicInt дороги.
//
class CodeRef
{
private:
    Ref<Object> ref;

public:
    operator Object* () const
    {
	return ref;
    }

    Object* operator -> () const
    {
	return ref;
    }

    CodeRef& operator = (CodeReferenced * const obj)
    {
	this->ref = obj ? obj->getCoderefContainer() : NULL;
	return *this;
    }

    CodeRef (WeakCodeRef const &weak_ref)
	: ref (weak_ref.weak_ref.getRef ())
    {
	DEBUG (
	  fprintf (stderr, "CodeRef(WeakCodeRef const &): obj 0x%lx\n", (unsigned long) (void*) ref);
	)
    }

    CodeRef (CodeReferenced * const obj)
	: ref (obj ? obj->getCoderefContainer() : NULL)
    {
	DEBUG (
	  fprintf (stderr, "CodeRef(CodeReferenced*): obj 0x%lx\n", (unsigned long) (void*) ref);
	)
    }

    CodeRef ()
    {
	DEBUG (
	  fprintf (stderr, "CodeRef()\n");
	)
    }

  DEBUG (
    ~CodeRef ()
    {
	fprintf (stderr, "~CodeRef: obj 0x%lx\n", (unsigned long) (void*) ref);
    }
  )
};

}


#ifdef DEBUG
#undef DEBUG
#endif


#endif /* LIBMARY__CODE_REF__H__ */

