/*  Scruffy - C/C++ parser and source code analyzer
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


#ifndef SCRUFFY__CHECKPOINT_TRACKER__H__
#define SCRUFFY__CHECKPOINT_TRACKER__H__


#include <libmary/libmary.h>

#include <pargen/lookup_data.h>


namespace Scruffy {

using namespace M;

class CheckpointTracker : public Pargen::LookupData
{
protected:
    class CancellableEntry;

public:
    class Cancellable : public virtual StReferenced
    {
    public:
	virtual void cancel () = 0;
    };

    class CancellableKey
    {
	friend class CheckpointTracker;

    private:
	StRef<CancellableEntry> cancellable_entry;

	CancellableKey (StRef<CancellableEntry> const &cancellable_entry)
	    : cancellable_entry (cancellable_entry)
	{
	}

	CancellableKey (CancellableEntry *cancellable_entry)
	    : cancellable_entry (cancellable_entry)
	{
	}

    public:
	CancellableKey ()
	{
	}
    };

protected:
    class Checkpoint;

    class CancellableEntry : public StReferenced
    {
    public:
	enum Type {
	    Generic,
	    Unconditional,
	    Sticky
	};

	Bool valid;

	Type cancellable_entry_type;

	StRef<Cancellable> cancellable;

	// The checkpoint that currently holds this cancellable.
	Checkpoint *checkpoint;
	// An element of Checkpoint::cancellables or
	// Checkpoint::sticky_cancellables (for sticky cancellables)
	// that refers to this cancellable.
	List< StRef<CancellableEntry> >::Element *cancellables_el;

	// The checkpoint that this cancellable is bound to.
	Checkpoint *bound_checkpoint;
	// An alement of Checkpoint::bound_cancellables.
	List<CancellableEntry*>::Element *bound_el;

	Bool isValid ()
	{
	    return valid;
	}

	// Turns this CancellableEntry into an invalid structure
	// that does not hold anything.
	void release ()
	{
	    valid = false;
	    cancellable = NULL;
	}

	CancellableEntry ()
	    : valid (true),
	      cancellable_entry_type (Generic),
	      checkpoint (NULL),
	      cancellables_el (NULL),
	      bound_checkpoint (NULL),
	      bound_el (NULL)
	{
	}
    };

    class Checkpoint : public IntrusiveListElement<>
    {
    public:
	// Bound cancellables are sticky cancellables
	// which we forget about after rerurning to this checkpoint.
	List<CancellableEntry*> bound_cancellables;

	List< StRef<CancellableEntry> > cancellables;
	List< StRef<CancellableEntry> > sticky_cancellables;

	Bool is_barrier;

	VStack::Level vstack_level;
    };

//    List<Checkpoint> checkpoints;
    IntrusiveList<Checkpoint> checkpoints;
    VStack checkpoint_vstack;

    void cleanupBoundCancellables ();

public:
    class CheckpointKey
    {
	friend class CheckpointTracker;

    private:
	Checkpoint *checkpoint;

	CheckpointKey (Checkpoint * const checkpoint)
	    : checkpoint (checkpoint)
	{
	}

     public:
	CheckpointKey ()
	    : checkpoint (NULL)
	{
	}
    };

  // Pargen::LookupData interface

    void newCheckpoint ();

    void cancelCheckpoint ();

    void commitCheckpoint ();

  // (End pargen::LookupData interface)

    CheckpointKey getCurrentCheckpoint ()
    {
	assert (!checkpoints.isEmpty ());
	return CheckpointKey (checkpoints.getLast ());
    }

    // Simple cancellables are cancelled only if the checkpoint is cancelled.
    // They don't get cancelled when the checkpoint is committed.
    CancellableKey addCancellable (Cancellable *cancellable);

    CancellableKey addUnconditionalCancellable (Cancellable *cancellable);

    // Sticky cancellables are cancelled when any parent checkpoint is cancelled.
    CancellableKey addStickyCancellable (Cancellable *cancellable);

    CancellableKey addBoundCancellable (Cancellable *cancellable,
					CheckpointKey const &checkpoint_key);

    void removeCancellable (CancellableKey const &cancellable_key);

    // TODO: It looks like barriers were a bad idea.
    void setBarrier ();

    CheckpointTracker ();

    ~CheckpointTracker ();
};

template <class T, class V = T>
class Cancellable_Value : public CheckpointTracker::Cancellable
{
private:
    T &obj;
    V value;

public:
    void cancel ()
    {
	obj = value;
    }

    Cancellable_Value (T &obj, V const &value)
	: obj (obj),
	  value (value)
    {
    }
};

template <class T, class V>
StRef<CheckpointTracker::Cancellable> createValueCancellable (T &obj, V const &value)
{
    return grab (new Cancellable_Value<T, V> (obj, value));
}

template <class T>
class Cancellable_Ref : public CheckpointTracker::Cancellable
{
private:
    StRef<T> &ref;

public:
    void cancel ()
    {
#if 0
	FUNC_NAME (
	    static char const * const _func_name = "Cancellable_Ref.cancel";
	)

	DEBUG (
	    errf->print (_func_name).print (": 0x").printHex ((UintPtr) this).pendl ();
	)
#endif

	ref = NULL;
    }

    Cancellable_Ref (StRef<T> &ref)
	: ref (ref)
    {
    }
};

template <class T>
class Cancellable_ListElement : public CheckpointTracker::Cancellable
{
private:
    List<T> &list;
    typename List<T>::Element * const el;

public:
    void cancel ()
    {
	list.remove (el);
    }

    Cancellable_ListElement (List<T> &list)
	: list (list),
	  el (list.last)
    {
	assert (!list.isEmpty ());
    }

    Cancellable_ListElement (List<T> &list,
			     typename List<T>::Element * const el)
	: list (list),
	  el (el)
    {
	assert (el);
    }
};

template <class T>
StRef<CheckpointTracker::Cancellable> createLastElementCancellable (List<T> &list)
{
    StRef< Cancellable_ListElement<T> > cancellable =
	    st_grab (new  (std::nothrow) Cancellable_ListElement<T> (list, list.last));
    return cancellable;
}

template <class T>
class Cancellable_MapEntry : public CheckpointTracker::Cancellable
{
private:
    T &map;
    typename T::Entry map_entry;

public:
    void cancel ()
    {
	map.remove (map_entry);
    }

    Cancellable_MapEntry (T &map,
			  typename T::Entry map_entry)
	: map (map),
	  map_entry (map_entry)
    {
    }
};

}


#endif /* SCRUFFY__CHECKPOINT_TRACKER__H__ */

