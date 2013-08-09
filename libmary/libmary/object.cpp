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


#include <libmary/libmary_thread_local.h>

#include <libmary/object.h>


#define DEBUG(a)

#if DEBUG(1) + 0
#include <cstdio>
#endif


namespace M {

void
Object::last_unref ()
{
    DEBUG (
	static char const * const _func_name = "LibMary.Object.last_unref";
    )

    DEBUG (
	printf ("0x%lx %s\n", (unsigned long) this, _func_name);
    )

    Shadow * const shadow = static_cast <Shadow*> (atomic_shadow.get ());
    if (!shadow) {
	DEBUG (
	    printf ("0x%lx %s: no shadow\n", (unsigned long) this, _func_name);
	)

	do_delete ();
	return;
    }

    {
        shadow->shadow_mutex.lock ();

#if 0
// This check overlaps with lastref_cnt.
	if (refcount.get () > 0) {
	  // We've been re-referenced via a weak reference before we have
	  // locked shadow->shadow_mutex.
            shadow->shadow_mutex.unlock ();
	    return;
	}
#endif

	assert (shadow->lastref_cnt > 0);
	--shadow->lastref_cnt;
	if (shadow->lastref_cnt) {
	  // There'll be more calls to last_unref() for this object due to
	  // getRef()/unref() pairs which sneaked before we've locked
	  // shadow->shadow_mutex. This call to last_unref() is not the last one.
            shadow->shadow_mutex.unlock ();
	    return;
	}

	shadow->weak_ptr = NULL;

        // Note that we *must* unref 'shadow_mutex' before calling getRefPtr()
        // for objects from 'deletion_subscription_list'. Otherwise there'd
        // be a deadlock condition on shadow mutexes when two objects subscribed
        // for deletion of each other are deleted simultaneously.
        //
        // There's no need to worry about dangling pointers due to the other
        // object being deleted after we nullified 'shadow->weak_ptr', because
        // we still have a reference to that object's shadow. WeakRefs act
        // as a proper synchronization mechanism here.
        shadow->shadow_mutex.unlock ();

	{
            deletion_mutex.lock ();

	    DeletionSubscription *sbn = deletion_subscription_list.getFirst ();
	    if (sbn) {
		do {
		    if (sbn->weak_peer_obj.isValid()) {
			if (// Objects are allowed to subscribe for deletion of
			    // themselves. All this takes is some extra caution to avoid
			    // deadlocks with mutexes.
			    sbn->weak_peer_obj.getTypedWeakPtr() != this)
			{
			    // Note that we're abusing the meaning of sbn->obj here.
			    // It points to the peer object now. We may do that because
			    // we have just nullified shadow->weak_ptr, which means that
			    // there'll be no external method calls for the object
			    // anymore.
			    sbn->obj = sbn->weak_peer_obj.getRefPtr ();
                            // If we couldn't obtain a reference (got NULL), then the peer
                            // object is being deleted right now. We just silently delete
                            // 'sbn' in that case.
			} else {
			    sbn->obj = this;
			}
		    }
		    sbn = deletion_subscription_list.getNext (sbn);
		} while (sbn != deletion_subscription_list.getFirst ());
	    }

            deletion_mutex.unlock ();
	}
    }

    // Releasing 'shadow' early so that 'atomic_shadow' field can be used
    // for other purposes (as deletion queue linked list pointer).
    shadow->unref ();
    // Shadow is also unrefed in ~Object().
    // This is necessary to prevent statically allocated Objects from leaking
    // their Shadows. Nullifying 'atomic_shadow' to avoid duplicate unref().
    atomic_shadow.set (NULL);

    do_delete ();
}

void
Object::do_delete ()
{
    DEBUG (
	static char const * const _func_name = "LibMary.Object.do_delete";
    )

    {
	LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal ();
	if (tlocal->state_mutex_counter > 0) {
	    DEBUG (
		printf ("0x%lx %s: state_mutex_counter > 0\n", (unsigned long) this, _func_name);
	    )

	    deletionQueue_append (this);

#if 0
// Creating references to an object after entering its destructor or after
// putting it on deletion queue is explicitly forbidden.
// At this point, the object has no valid shadow and most valid actions
// with references (like keeping them elsewhere) are invalid.
// Provided this, allowing to create limited (temporal) references makes
// no sense: it'd be an unnecessary compilcation for no good reason.
	    // We do a ref() to allow the destructor to create temporal
	    // references to this object.
	    ref ();
#endif

	    return;
	}
    }

    DEBUG (
	printf ("0x%lx %s: invoking deletion subscriptions\n", (unsigned long) this, _func_name);
    )

  // mutualDeletionCallback() will _not_be called at this point,
  // thanks to WeakRef's on this object.
  // removeDeletionCallback() will not be called for the same reason.

  // Invoking deletion subscriptions. do_delete() must synchronize with
  // itself, because deletion_subscription_list may contain mutual
  // subscriptions which may be deleted at any moment by do_delete() of
  // another object.
  //
  // ^^^ And if they do get deleted, then we'll never call unref() for the peer object.
  // That would be a bug (leak). But the comment above is likely false, because
  // to remove any deletion subscription (including mutual subscriptions),
  // one has to get a reference to the object, which is not possible at this point.

    for (;;) {
	deletion_mutex.lock ();
	DeletionSubscription * const sbn = deletion_subscription_list.getFirst ();
	if (!sbn) {
	    deletion_mutex.unlock ();
	    break;
	}

	deletion_subscription_list.remove (sbn);
	deletion_mutex.unlock ();

	if (sbn->weak_peer_obj.isValid()) {
	    // We did getRef() after we nullified shadow->weak_ptr in
	    // last_unref().  sbn->obj was set to point to the peer object.
	    Object * const peer_obj = sbn->obj;
	    DEBUG (
		printf ("0x%lx %s: deletion callback with peer obj: "
                        "sbn: 0x%lx, peer_obj: 0x%lx, sbn->mutual_sbn: 0x%lx\n",
			(unsigned long) this,
                        _func_name,
                        (unsigned long) sbn,
                        (unsigned long) peer_obj,
                        (unsigned long) sbn->mutual_sbn.del_sbn);
	    )
	    if (peer_obj) {
		if (sbn->mutual_sbn)
		    peer_obj->removeDeletionCallback (sbn->mutual_sbn);

		sbn->cb (sbn->cb_data);

		if (peer_obj != this)
		    peer_obj->unref ();
	    }
	} else {
	    assert (!sbn->mutual_sbn);
	    sbn->cb (sbn->cb_data);
	}

	delete sbn;
    }

    DEBUG (
	printf ("0x%lx %s: deleting self\n", (unsigned long) this, _func_name);
    )

    delete this;
}

void
Object::mutualDeletionCallback (void * const mt_nonnull _sbn)
{
  // Note: It is guaranteed that this method will not be called when any
  // StateMutex is locked. Deletion queue takes care of this.

    DeletionSubscription * const sbn = static_cast <DeletionSubscription*> (_sbn);
    Object * const self = sbn->obj;

    self->deletion_mutex.lock ();
    self->deletion_subscription_list.remove (sbn);
    self->deletion_mutex.unlock ();

    delete sbn;
}

mt_locked Object::DeletionSubscriptionKey
Object::addDeletionCallback (CbDesc<DeletionCallback> const &cb)
{
    DEBUG (
	static char const * const _func_name = "LibMary.Object.addDeletionCallback";
    )

    DeletionSubscription * const sbn = new (std::nothrow) DeletionSubscription (cb);
    assert (sbn);
    DEBUG (
	printf ("0x%lx %s: sbn: 0x%lx, guard_obj: 0x%lx\n",
		(unsigned long) this, _func_name, (unsigned long) sbn, (unsigned long) cb.coderef_container);
    )
    sbn->obj = this;

    if (cb.coderef_container && cb.coderef_container != getCoderefContainer() /* equivalent to 'this' */) {
        sbn->mutual_sbn = cb.coderef_container->addDeletionCallbackNonmutual (
                CbDesc<DeletionCallback> (
                        mutualDeletionCallback,
                        sbn,
                        getCoderefContainer() /* equivalent to 'this' */));
    }

    deletion_mutex.lock ();
    deletion_subscription_list.append (sbn);
    deletion_mutex.unlock ();

    return sbn;
}

mt_locked Object::DeletionSubscriptionKey
Object::addDeletionCallbackNonmutual (CbDesc<DeletionCallback> const &cb)
{
    DEBUG (
	static char const * const _func_name = "LibMary.Object.addDeletionCallbackNonmutual";
    )

    DeletionSubscription * const sbn = new (std::nothrow) DeletionSubscription (cb);
    assert (sbn);
    DEBUG (
	printf ("0x%lx %s: sbn: 0x%lx, guard_obj: 0x%lx\n",
		(unsigned long) this, _func_name, (unsigned long) sbn, (unsigned long) cb.coderef_container);
    )
    sbn->obj = this;

    deletion_mutex.lock ();
    deletion_subscription_list.append (sbn);
    deletion_mutex.unlock ();

    return sbn;
}

void
Object::removeDeletionCallback (DeletionSubscriptionKey const mt_nonnull sbn)
{
    if (sbn->mutual_sbn) {
	Ref<Object> const peer_obj = sbn->weak_peer_obj.getRef ();
        if (!peer_obj) {
          // 'sbn' will be released by mutualDeletionCallback(), which is likely
          // just about to be called.
            return;
        }

        peer_obj->removeDeletionCallback (sbn->mutual_sbn);
    }

    deletion_mutex.lock ();
    deletion_subscription_list.remove (sbn.del_sbn);
    deletion_mutex.unlock ();

    delete sbn.del_sbn;
}

void Object::unrefOnDeletionCallback (void * const _self)
{
    Object * const self = static_cast <Object*> (_self);
    self->unref ();
}

void Object::unrefOnDeletion (Object * const mt_nonnull master_obj)
{
    if (master_obj == this) {
        fprintf (stderr, "libMary WARNING: Object::unrefOnDeletion: binding to self\n");
        return;
    }

    master_obj->addDeletionCallback (CbDesc<DeletionCallback> (unrefOnDeletionCallback,
                                                               this,
                                                               this));
}

}

