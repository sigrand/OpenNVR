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


#ifndef LIBMARY__OBJECT__H__
#define LIBMARY__OBJECT__H__


#include <libmary/intrusive_list.h>
#include <libmary/atomic.h>
#include <libmary/referenced.h>
#include <libmary/ref.h>
#include <libmary/virt_ref.h>
#include <libmary/code_referenced.h>
#include <libmary/mutex.h>
#include <libmary/fast_mutex.h>


#ifdef DEBUG
#error DEBUG already defined
#endif
#define DEBUG(a)
#if DEBUG(1) + 0
#include <cstdio>
#endif


namespace M {

class Object;

void deletionQueue_append (Object * const obj);
void deletionQueue_process ();

template <class T> class Callback;

// Object inherits from Referenced for compatibility with MyCpp. Virtual
// inheritance is here for the same reason. It shouldn't be too much of
// overhead, though.
class Object : public virtual Referenced,
	       public virtual CodeReferenced
{
    template <class T> friend class WeakRef;

    friend void deletionQueue_append (Object * const obj);
    friend void deletionQueue_process ();

public:
    typedef void DeletionCallback (void *data);

    class DeletionSubscription;

    // This wrapper has been introduced to ease migrating MyCpp to LibMary
    // refcounting code. Originally, DeletionSubscriptionKey was a typedef for
    // DeletionSubscription*.
    class DeletionSubscriptionKey
    {
	friend class Object;
    private:
	DeletionSubscription *del_sbn;
	DeletionSubscription* operator -> () const { return del_sbn; }
    public:
	bool isNull () const { return del_sbn == NULL; } // Eases transition to M::Object for MyCpp.
        operator bool () const { return del_sbn; }
	// TODO Only class Object should be able to create bound deletion keys.
	DeletionSubscriptionKey (DeletionSubscription * const del_sbn) : del_sbn (del_sbn) {}
	DeletionSubscriptionKey () : del_sbn (NULL) {}
    };

private:
  // Class WeakRef may access the following private members as a friend.

public:
    class Shadow : public Referenced
    {
	friend class Object;
	template <class T> friend class WeakRef;

    private:
	FastMutex shadow_mutex;
	Object *weak_ptr;

	// This counter ensures that the object will be deleted sanely when
	// series of _getRef()/unref() calls sneak in while last_unref() is
	// in progress. In this case, we'll have multiple invocations of
	// last_unref(), and we should be able to determine which of those
	// invocations is the last one.
	Count lastref_cnt;

	DEBUG (
	    Shadow ()
	    {
		static char const * const _func_name = "LibMary.Object.Shadow()";
		printf ("0x%lx %s\n", (unsigned long) this, _func_name);
	    }

	    ~Shadow ()
	    {
		static char const * const _func_name = "LibMary.Object.~Shadow()";
		printf ("0x%lx %s\n", (unsigned long) this, _func_name);
	    }
	)
    };

private:
    // There's no need to return Ref<Shadow> here:
    //   * We're supposed to have a valid reference to the object for duration
    //     of getShadow() call;
    //   * Once bound to the object, the shadow's life time is not shorter than
    //     object's lifetime.
    Shadow* getShadow ()
    {
        // TODO It's better to create shadow immediately to avoid the need for atomic ops here.
        //      That would provide cheap getShadow() for runtime synchronization shortcuts.

	Shadow *shadow = static_cast <Shadow*> (atomic_shadow.get ());
	if (shadow)
	    return shadow;

	// TODO Slab cache for Object::Shadow objects: describe the idea.

	// Shadow stays referenced until it is unrefed in ~Object().
	shadow = new (std::nothrow) Shadow ();
        assert (shadow);
	shadow->weak_ptr = this;
	shadow->lastref_cnt = 1;

	if (atomic_shadow.compareAndExchange (NULL, static_cast <void*> (shadow)))
	    return shadow;

	// We assume that high contention on getShadow() is unlikely, hence
	// occasional deletes do not bring much overhead.
	delete shadow;

	return static_cast <Shadow*> (atomic_shadow.get ());
    }

    // _getRef() is specific to WeakRef::getRef(). It is a more complex subcase
    // of ref().
    static Object* _getRef (Shadow * const mt_nonnull shadow)
    {
        shadow->shadow_mutex.lock ();

	Object * const obj = shadow->weak_ptr;

	DEBUG (
	  fprintf (stderr, "Object::_getRef: shadow 0x%lx, obj 0x%lx\n", (unsigned long) shadow, (unsigned long) obj);
	)

        if (!obj) {
            shadow->shadow_mutex.unlock ();
	    return NULL;
	}

	if (obj->refcount.fetchAdd (1) == 0)
//	if (obj->refcount.fetch_add (1, std::memory_order_relaxed) == 0)
	    ++shadow->lastref_cnt;

#ifdef LIBMARY_REF_TRACING
	if (obj->traced)
	    obj->traceRef ();
#endif

        shadow->shadow_mutex.unlock ();
	return obj;
    }

  // (End of what class WeakRef may access.)

    AtomicPointer atomic_shadow;

    IntrusiveCircularList<DeletionSubscription> deletion_subscription_list;

    // Beware that size of pthread_mutex_t is 24 bytes on 32-bit platforms and
    // 40 bytes on 64-bit ones. We definitely do not want more than one-two mutexes
    // per object, which is already too much overhead.
    Mutex deletion_mutex;

    static void mutualDeletionCallback (void * mt_nonnull _sbn);

    virtual void last_unref ();

    // May be called directly by deletionQueue_process().
    void do_delete ();

    // We forbid copying until it is shown that it might be convenient in some cases.
    LIBMARY_NO_COPY (Object)

public:
  mt_iface (CodeReferenced)

      Object* getCoderefContainer ()
      {
	  return this;
      }

  mt_iface_end

    mt_locked DeletionSubscriptionKey addDeletionCallback (CbDesc<DeletionCallback> const &cb);

    // TODO Is it really necessary to create a new DeletionCallback object for
    // mutual deletion callbacks? Perhaps the existing DeletionCallback object
    // could be reused.
    mt_locked DeletionSubscriptionKey addDeletionCallbackNonmutual (CbDesc<DeletionCallback> const &cb);

    void removeDeletionCallback (DeletionSubscriptionKey mt_nonnull sbn);

private:
    static void unrefOnDeletionCallback (void *_self);

public:
    void unrefOnDeletion (Object * mt_nonnull master_obj);

    Object ()
    {
    }

    virtual ~Object ();
};

template <class T>
class ObjectWrap : public Object, public T
{
};

}


#include <libmary/weak_ref.h>
#include <libmary/dep_ref.h>


namespace M {

// TODO DeletionSubscription and mutual deletion subscription can be stored
//      in a single area of memory, which would spare us one malloc() per
//      deletion subscription.
class Object::DeletionSubscription : public IntrusiveListElement<>
{
    friend class Object;

private:
    DeletionCallback    * const cb;
    void                * const cb_data;
    WeakRef<Object>       const weak_peer_obj;
    VirtRef               const ref_data;

    // Subscription for deletion of the peer.
    DeletionSubscriptionKey mutual_sbn;

    // Pointer to self (for mutual deletion callback).
    Object *obj;

    DeletionSubscription (CbDesc<DeletionCallback> const &cb)
	: cb            (cb.cb),
	  cb_data       (cb.cb_data),
	  weak_peer_obj (cb.coderef_container),
	  ref_data      (cb.ref_data)
    {
    }
};

inline Object::~Object ()
{
    DEBUG (
	static char const * const _func_name = "LibMary.Object.~Object()";
    )

    DEBUG (
	printf ("0x%lx %s\n", (unsigned long) this, _func_name);
    )

    {
      // Note: Here we count on that we can read atomic_shadow as an atomic
      // variable correctly even if we've been using it in non-atomic fashion
      // as deletion queue link pointer. In practice, this should work, but we'll
      // be able to count on this 100% only whith C++0x.

	Shadow * const shadow = static_cast <Shadow*> (atomic_shadow.get ());
	if (shadow)
	    shadow->unref ();
    }

  // deletion_subscription_list must be empty at this moment. We have released
  // all subscriptions in do_delete. We can only check that it holds here with
  // state mutex held, so we don't do that.

    DEBUG (
	printf ("0x%lx %s: done\n", (unsigned long) this, _func_name);
    )
}

}


#ifdef DEBUG
#undef DEBUG
#endif


#endif /* LIBMARY__OBJECT__H__ */

