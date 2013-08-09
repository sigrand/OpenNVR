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


#ifndef LIBMARY__INFORMER__H__
#define LIBMARY__INFORMER__H__


#include <libmary/types.h>
#include <libmary/state_mutex.h>
#include <libmary/object.h>
#include <libmary/virt_ref.h>
#include <libmary/code_ref.h>
#include <libmary/cb.h>
#include <libmary/intrusive_list.h>


namespace M {

// This class should not be used from outside.
class GenericInformer : public DependentCodeReferenced
{
public:
    enum InformFlags {
	// TODO Support oneshot subscriptions.
	InformOneshot = 1
    };

protected:
    union CallbackPtr
    {
	void *obj;
	VoidFunction func;

	CallbackPtr (void * const obj) : obj (obj) {}
	CallbackPtr (VoidFunction const func) : func (func) {}
    };

    typedef void (*ProxyInformCallback) (CallbackPtr   cb_ptr,
					 void         *cb_data,
					 VoidFunction  inform_cb,
					 void         *inform_data);

    class SubscriptionList_name;
    class SubscriptionInvalidationList_name;

    class Subscription : public IntrusiveListElement<SubscriptionList_name>,
			 public IntrusiveListElement<SubscriptionInvalidationList_name>
    {
    public:
	bool valid;

	GenericInformer *informer;
	bool oneshot;
	Object::DeletionSubscriptionKey del_sbn;

	CallbackPtr cb_ptr;
	void *cb_data;
	WeakCodeRef weak_code_ref;

	VirtRef ref_data;

	Subscription (CallbackPtr      const cb_ptr,
		      void           * const cb_data,
		      VirtReferenced * const ref_data,
		      Object         * const coderef_container)
	    : cb_ptr (cb_ptr),
	      cb_data (cb_data),
	      weak_code_ref (coderef_container),
	      ref_data (ref_data)
	{
	}
    };

    typedef IntrusiveList<Subscription, SubscriptionList_name> SubscriptionList;
    typedef IntrusiveList<Subscription, SubscriptionInvalidationList_name> SubscriptionInvalidationList;

public:
    class SubscriptionKey
    {
	friend class GenericInformer;
    private:
	Subscription *sbn;
    public:
	operator bool () const { return sbn; }
	SubscriptionKey () : sbn (NULL) {}
	SubscriptionKey (Subscription * const sbn) : sbn (sbn) {}

 	// Methods for C API binding.
	void *getAsVoidPtr () const { return static_cast <void*> (sbn); }
	static SubscriptionKey fromVoidPtr (void *ptr) {
		return SubscriptionKey (static_cast <Subscription*> (ptr)); }
   };

protected:
    StateMutex * const mutex;

    mt_mutex (mutex) SubscriptionList sbn_list;
    mt_mutex (mutex) SubscriptionInvalidationList sbn_invalidation_list;
    mt_mutex (mutex) Count traversing;

    mt_mutex (mutex) void releaseSubscription (Subscription *sbn);
    mt_mutex (mutex) void releaseSubscriptionFromDestructor (Subscription *sbn);

    static void subscriberDeletionCallback (void *_sbn);

    void informAll (ProxyInformCallback  mt_nonnull proxy_inform_cb,
		    VoidFunction         inform_cb,
		    void                *inform_cb_data);

    mt_unlocks_locks (mutex) void informAll_unlocked (ProxyInformCallback  mt_nonnull proxy_inform_cb,
                                                      VoidFunction         inform_cb,
                                                      void                *inform_cb_data);

    SubscriptionKey subscribeVoid (CallbackPtr     cb_ptr,
				   void           *cb_data,
				   VirtReferenced *ref_data,
				   Object         *coderef_container);

    mt_mutex (mutex) SubscriptionKey subscribeVoid_unlocked (CallbackPtr     cb_ptr,
                                                             void           *cb_data,
                                                             VirtReferenced *ref_data,
                                                             Object         *coderef_container);

public:
    mt_mutex (mutex) bool gotSubscriptions_unlocked ()
    {
	return !sbn_list.isEmpty();
    }

    void unsubscribe (SubscriptionKey sbn_key);

    mt_mutex (mutex) void unsubscribe_unlocked (SubscriptionKey sbn_key);

    // In general, if @coderef_container is not null, then @mutex should be
    // the state mutex of @coderef_container. There may be concious deviations
    // from this rule.
    GenericInformer (Object     * const coderef_container,
		     StateMutex * const mutex)
	: DependentCodeReferenced (coderef_container),
	  mutex (mutex),
	  traversing (0)
    {
    }

    ~GenericInformer ();
};

// Informer for structs with callbacks.
template <class T>
class Informer_ : public GenericInformer
{
public:
    typedef void (*InformCallback) (T /* TODO const */ *ev_struct,
				    void *cb_data,
				    void *inform_data);

protected:
    static void proxyInformCallback (CallbackPtr      const cb_ptr,
				     void           * const cb_data,
				     VoidFunction     const inform_cb,
				     void           * const inform_data)
    {
	((InformCallback) inform_cb) ((T*) cb_ptr.obj, cb_data, inform_data);
    }

public:
    void informAll (InformCallback   const inform_cb,
		    void           * const inform_cb_data)
    {
	GenericInformer::informAll (proxyInformCallback, (VoidFunction) inform_cb, inform_cb_data);
    }

    // May unlock and lock 'mutex' in the process.
    mt_unlocks_locks (mutex) void informAll_unlocked (InformCallback    const inform_cb,
                                                      void            * const inform_cb_data)
    {
	mt_unlocks_locks (mutex) GenericInformer::informAll_unlocked (proxyInformCallback, (VoidFunction) inform_cb, inform_cb_data);
    }

    SubscriptionKey subscribe (T const        * const ev_struct,
			       void           * const cb_data,
			       VirtReferenced * const ref_data,
			       Object         * const coderef_container)
    {
	return subscribeVoid ((void*) ev_struct, cb_data, ref_data, coderef_container);
    }

    SubscriptionKey subscribe (CbDesc<T> const &cb)
    {
	return subscribeVoid ((void*) cb.cb, cb.cb_data, cb.ref_data, cb.coderef_container);
    }

    SubscriptionKey subscribe_unlocked (T const        * const ev_struct,
					void           * const cb_data,
					VirtReferenced * const ref_data,
					Object         * const coderef_container)
    {
	return subscribeVoid_unlocked ((void*) ev_struct, cb_data, ref_data, coderef_container);
    }

    SubscriptionKey subscribe_unlocked (CbDesc<T> const &cb)
    {
	return subscribeVoid_unlocked ((void*) cb.cb, cb.cb_data, cb.ref_data, cb.coderef_container);
    }

    Informer_ (Object     * const coderef_container,
	       StateMutex * const mutex)
	: GenericInformer (coderef_container, mutex)
    {
    }
};

// Informer for direct callbacks.
template <class T>
class Informer : public GenericInformer
{
public:
    typedef void (*InformCallback) (T     cb,
				    void *cb_data,
				    void *inform_data);

protected:
    static void proxyInformCallback (CallbackPtr      const cb_ptr,
				     void           * const cb_data,
				     VoidFunction     const inform_cb,
				     void           * const inform_data)
    {
	((InformCallback) inform_cb) ((T) cb_ptr.func, cb_data, inform_data);
    }

public:
    void informAll (InformCallback   const inform_cb,
		    void           * const inform_cb_data)
    {
	GenericInformer::informAll (proxyInformCallback, (VoidFunction) inform_cb, inform_cb_data);
    }

    // May unlock and lock 'mutex' in the process.
    mt_unlocks_locks (mutex) void informAll_unlocked (InformCallback    const inform_cb,
			                              void            * const inform_cb_data)
    {
	mt_unlocks_locks (mutex) GenericInformer::informAll_unlocked (
                proxyInformCallback, (VoidFunction) inform_cb, inform_cb_data);
    }

    SubscriptionKey subscribe (T                const cb,
			       void           * const cb_data,
			       VirtReferenced * const ref_data,
			       Object         * const coderef_container)
    {
	return subscribeVoid ((VoidFunction) cb, cb_data, ref_data, coderef_container);
    }

    SubscriptionKey subscribe (CbDesc<T> const &cb)
    {
	return subscribeVoid ((VoidFunction) cb.cb, cb.cb_data, cb.ref_data, cb.coderef_container);
    }

    mt_mutex (mutex) SubscriptionKey subscribe_unlocked (CbDesc<T> const &cb)
    {
	return subscribeVoid_unlocked ((VoidFunction) cb.cb, cb.cb_data, cb.ref_data, cb.coderef_container);
    }

    Informer (Object     * const coderef_container,
	      StateMutex * const mutex)
	: GenericInformer (coderef_container, mutex)
    {
    }
};

}


#endif /* LIBMARY__INFORMER__H__ */

