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


#include <libmary/libmary.h>

#include <scruffy/checkpoint_tracker.h>


#define DEBUG(a) ;

#define FUNC_NAME(a) ;


#define CLASS_NAME "Scruffy.CheckpointTracker"


using namespace M;

namespace Scruffy {

void
CheckpointTracker::newCheckpoint ()
{
  FUNC_NAME (
    static char const * const _func_name = CLASS_NAME ".newCheckpoint";
  )

    VStack::Level const vstack_level = checkpoint_vstack.getLevel ();
    Checkpoint * const checkpoint = new (checkpoint_vstack.push_malign (sizeof (Checkpoint), alignof (Checkpoint))) Checkpoint;
    checkpoint->vstack_level = vstack_level;
    checkpoints.append (checkpoint);

    // TEST
    assert (!checkpoints.isEmpty ());

    DEBUG (
	errf->print (_func_name).print (": checkpoint 0x").printHex ((Uint64) checkpoints.getLast ()).pendl ();
    )

//    errf->print ("--- NEW LEVEL ").print (checkpoint_vstack.getLevel ()).pendl ();
}

void
CheckpointTracker::cleanupBoundCancellables ()
{
  FUNC_NAME (
    static char const * const _func_name = CLASS_NAME ".cleanupBoundCancellables";
  )

    assert (!checkpoints.isEmpty ());
    Checkpoint * const checkpoint = checkpoints.getLast ();

    List<CancellableEntry*>::DataIterator iter (checkpoint->bound_cancellables);
    while (!iter.done ()) {
	CancellableEntry * &cancellable_entry = iter.next ();

	assert (cancellable_entry->cancellable_entry_type == CancellableEntry::Sticky);
	assert (cancellable_entry->bound_checkpoint == checkpoint);

	DEBUG (
	    errf->print (_func_name).print (": releasing bound cancellable: cancellable 0x"
			 "").printHex ((Uint64) cancellable_entry->cancellable.ptr ()).pendl ();
	)

	List< StRef<CancellableEntry> >::Element *tmp_el = cancellable_entry->cancellables_el;

	cancellable_entry->release ();

	checkpoint->sticky_cancellables.remove (tmp_el);
      // 'cancellable_entry' is not valid anymore.
    }

    checkpoint->bound_cancellables.clear ();
}

void
CheckpointTracker::cancelCheckpoint ()
{
  FUNC_NAME (
    static char const * const _func_name = CLASS_NAME ".cancelCheckpoint";
  )

    assert (!(checkpoints.isEmpty () ||
	      checkpoints.getLast () == checkpoints.getFirst ()));

    DEBUG (
	errf->print (_func_name).print (": checkpoint 0x").printHex ((Uint64) checkpoints.getLast ()).pendl ();
    )

    Checkpoint * const checkpoint = checkpoints.getLast ();

    cleanupBoundCancellables ();

  // Note: It is important to cancel old sticky cancellables
  // before cancelling the current ones.

    // Note: 'checkpoint->sticky_cancellables' list may be altered by calls to Cancellable::cancel().
    while (checkpoint->sticky_cancellables.first != NULL) {
	List< StRef<CancellableEntry> >::Element * const cur_el = checkpoint->sticky_cancellables.first;

	StRef<CancellableEntry> &cancellable_entry = cur_el->data;
	assert (cancellable_entry->cancellable_entry_type == CancellableEntry::Sticky);

	DEBUG (
	    errf->print (_func_name).print (": (sticky) calling Cancellable::cancel(), cancellable 0x"
			 "").printHex ((Uint64) cancellable_entry->cancellable.ptr ()).pendl ();
	)

	cancellable_entry->valid = false;
	cancellable_entry->cancellable->cancel ();

	if (cancellable_entry->bound_checkpoint != NULL) {
	    cancellable_entry->bound_checkpoint->bound_cancellables.remove (
		    cancellable_entry->bound_el);
	}

	cancellable_entry->release ();

	checkpoint->sticky_cancellables.remove (cur_el);
    }
    assert (checkpoint->sticky_cancellables.isEmpty ());

    // Note: 'checkpoint->cancellables' list may be altered by calls to Cancellable::cancel().
    while (checkpoint->cancellables.first != NULL) {
	List< StRef<CancellableEntry> >::Element * const cur_el = checkpoint->cancellables.first;

	StRef<CancellableEntry> &cancellable_entry = cur_el->data;

	DEBUG (
	    errf->print (_func_name).print (": calling Cancellable::cancel(), cancellable 0x"
			 "").printHex ((Uint64) cancellable_entry->cancellable.ptr ()).pendl ();
	)

	cancellable_entry->valid = false;
	cancellable_entry->cancellable->cancel ();

	cancellable_entry->release ();

	checkpoint->cancellables.remove (cur_el);
    }
    assert (checkpoint->cancellables.isEmpty ());

    {
	Checkpoint * const tmp_checkpoint = checkpoints.getLast ();
	checkpoints.remove (tmp_checkpoint);
	VStack::Level const tmp_level = tmp_checkpoint->vstack_level;
	tmp_checkpoint->~Checkpoint ();
	checkpoint_vstack.setLevel (tmp_level);
    }
  // 'checkpoint' is not valid anymore.

    cleanupBoundCancellables ();
}

void
CheckpointTracker::commitCheckpoint ()
{
  FUNC_NAME (
    static char const * const _func_name = CLASS_NAME ".commitCheckpoint";
  )

    assert (!(checkpoints.isEmpty () ||
	      checkpoints.getLast () == checkpoints.getFirst ()));

    Checkpoint * const checkpoint = checkpoints.getLast ();

    cleanupBoundCancellables ();

    DEBUG (
	errf->print (_func_name).print (": checkpoint 0x").printHex ((Uint64) checkpoint).pendl ();
    )

    if (checkpoint->is_barrier) {
	DEBUG (
	    errf->print (_func_name).print (": barrier").pendl ();
	)

	cancelCheckpoint ();
	return;
    }

    // Note: 'checkpoint->cancellables' list may be altered by calls to Cancellable::cancel().
    while (checkpoint->cancellables.first != NULL) {
	List< StRef<CancellableEntry> >::Element * const cur_el = checkpoint->cancellables.first;

	StRef<CancellableEntry> &cancellable_entry = cur_el->data;
	if (cancellable_entry->cancellable_entry_type == CancellableEntry::Unconditional) {
	    DEBUG (
		errf->print (_func_name).print (": (unconditional) calling Cancellable::cancel(), cancellable 0x"
			     "").printHex ((Uint64) cancellable_entry->cancellable.ptr ()).pendl ();
	    )

	    cancellable_entry->valid = false;
	    cancellable_entry->cancellable->cancel ();
	}

	cancellable_entry->release ();

	checkpoint->cancellables.remove (cur_el);
    }
    assert (checkpoint->cancellables.isEmpty ());

    if (!checkpoint->sticky_cancellables.isEmpty ()) {
	Checkpoint * const prv_checkpoint = checkpoints.getPrevious (checkpoints.getLast ());
	// Note: it is important to _prepend_ old sticky cancellables to the list,
	// so that they'll be cancelled first later on.
	prv_checkpoint->sticky_cancellables.steal (&checkpoint->sticky_cancellables,
						   checkpoint->sticky_cancellables.first,
						   checkpoint->sticky_cancellables.last,
						   prv_checkpoint->sticky_cancellables.first,
						   GenericList::StealPrepend);

	{
	    List< StRef<CancellableEntry> >::Iterator iter (prv_checkpoint->sticky_cancellables);
	    while (!iter.done ()) {
		List< StRef<CancellableEntry> >::Element &cur_el = iter.next ();

		StRef<CancellableEntry> &cancellable_entry = cur_el.data;
		cancellable_entry->checkpoint = prv_checkpoint;
		cancellable_entry->cancellables_el = &cur_el;
	    }
	}
    }

    {
	Checkpoint * const tmp_checkpoint = checkpoints.getLast ();
	checkpoints.remove (tmp_checkpoint);
	VStack::Level const tmp_level = tmp_checkpoint->vstack_level;
	tmp_checkpoint->~Checkpoint ();
	checkpoint_vstack.setLevel (tmp_level);
    }
  // 'checkpoint' is not valid anymore.

    cleanupBoundCancellables ();
}

CheckpointTracker::CancellableKey
CheckpointTracker::addCancellable (Cancellable * const cancellable)
{
  FUNC_NAME (
    static char const * const _func_name = CLASS_NAME ".addCancellable";
  )

    assert (cancellable);

    assert (!checkpoints.isEmpty ());
    Checkpoint * const checkpoint = checkpoints.getLast ();

    DEBUG (
	errf->print (_func_name).print (": adding generic cancellable 0x"
		     "").printHex ((Uint64) cancellable).print (" to checkpoint 0x"
		     "").printHex ((Uint64) checkpoint).pendl ();
    )

    StRef<CancellableEntry> cancellable_entry = st_grab (new (std::nothrow) CancellableEntry);
    checkpoint->cancellables.prepend (cancellable_entry);
    cancellable_entry->checkpoint = checkpoint;
    cancellable_entry->cancellables_el = checkpoint->cancellables.first;

    cancellable_entry->cancellable_entry_type = CancellableEntry::Generic;
    cancellable_entry->cancellable = cancellable;

    return cancellable_entry;
}

CheckpointTracker::CancellableKey
CheckpointTracker::addUnconditionalCancellable (Cancellable * const cancellable)
{
  FUNC_NAME (
    static char const * const _func_name = CLASS_NAME ".addUnconditionalCancellable";
  )

    assert (cancellable);

    assert (!checkpoints.isEmpty ());
    Checkpoint * const checkpoint = checkpoints.getLast ();

    DEBUG (
	errf->print (_func_name).print (": adding unconditional cancellable 0x"
		     "").printHex ((Uint64) cancellable).print (" to checkpoint 0x"
		     "").printHex ((Uint64) checkpoint).pendl ();
    )

    StRef<CancellableEntry> cancellable_entry = st_grab (new (std::nothrow) CancellableEntry);
    checkpoint->cancellables.prepend (cancellable_entry);
    cancellable_entry->checkpoint = checkpoint;
    cancellable_entry->cancellables_el = checkpoint->cancellables.first;

    cancellable_entry->cancellable_entry_type = CancellableEntry::Unconditional;
    cancellable_entry->cancellable = cancellable;

    return cancellable_entry;
}

CheckpointTracker::CancellableKey
CheckpointTracker::addStickyCancellable (Cancellable * const cancellable)
{
  FUNC_NAME (
    static char const * const _func_name = CLASS_NAME ".addStickyCancellable";
  )

    assert (cancellable);

    assert (!checkpoints.isEmpty ());
    Checkpoint * const checkpoint = checkpoints.getLast ();

    DEBUG (
	errf->print (_func_name).print (": adding sticky cancellable 0x"
		     "").printHex ((Uint64) cancellable).print (" to checkpoint 0x"
		     "").printHex ((Uint64) checkpoint).pendl ();
    )

    StRef<CancellableEntry> cancellable_entry = st_grab (new (std::nothrow) CancellableEntry);
    checkpoint->sticky_cancellables.prepend (cancellable_entry);
    cancellable_entry->checkpoint = checkpoint;
    cancellable_entry->cancellables_el = checkpoint->sticky_cancellables.first;

    cancellable_entry->cancellable_entry_type = CancellableEntry::Sticky;
    cancellable_entry->cancellable = cancellable;

    return cancellable_entry;
}

CheckpointTracker::CancellableKey
CheckpointTracker::addBoundCancellable (Cancellable * const cancellable,
					CheckpointKey const &checkpoint_key)
{
  FUNC_NAME (
    static char const * const _func_name = CLASS_NAME ".addBoundCancellable";
  )

    assert (!(cancellable == NULL ||
	      checkpoint_key.checkpoint == NULL));

    assert (!checkpoints.isEmpty ());
    Checkpoint * const checkpoint = checkpoints.getLast ();

    DEBUG (
	errf->print (_func_name).print (": adding bound cancellable 0x"
		     "").printHex ((Uint64) cancellable).print (" to checkpoint 0x"
		     "").printHex ((Uint64) checkpoint).print (", bound to checkpoint 0x"
		     "").printHex ((Uint64) checkpoint_key.checkpoint).pendl ();
    )

    StRef<CancellableEntry> cancellable_entry = st_grab (new (std::nothrow) CancellableEntry);
    checkpoint->sticky_cancellables.prepend (cancellable_entry);
    cancellable_entry->checkpoint = checkpoint;
    cancellable_entry->cancellables_el = checkpoint->sticky_cancellables.first;

    cancellable_entry->cancellable_entry_type = CancellableEntry::Sticky;
    cancellable_entry->cancellable = cancellable;

    Checkpoint * const bound_checkpoint = checkpoint_key.checkpoint;
    assert (bound_checkpoint);

    cancellable_entry->bound_checkpoint = bound_checkpoint;
    cancellable_entry->bound_el =
	    bound_checkpoint->bound_cancellables.prepend (cancellable_entry);

    return cancellable_entry;
}

void
CheckpointTracker::removeCancellable (CancellableKey const &cancellable_key)
{
  FUNC_NAME (
    static char const * const _func_name = CLASS_NAME ".removeCancellable";
  )

    assert (cancellable_key.cancellable_entry);
    StRef<CancellableEntry> const &cancellable_entry = cancellable_key.cancellable_entry;
    if (!cancellable_entry->isValid ()) {
	DEBUG (
	    errf->print (_func_name).print (": invalid cancellable").pendl ();
	)
	return;
    }

    DEBUG (
	errf->print (_func_name).print (": removing cancellable "
		     "0x").printHex ((Uint64) cancellable_entry->cancellable.ptr ()).pendl ();
    )

    if (cancellable_entry->bound_checkpoint != NULL) {
	DEBUG (
	    errf->print (_func_name).print (": bound to checkpoint "
			 "0x").print ((Uint64) cancellable_entry->bound_checkpoint).pendl ();
	)

	cancellable_entry->bound_checkpoint->bound_cancellables.remove (cancellable_entry->bound_el);
    }

    switch (cancellable_entry->cancellable_entry_type) {
	case CancellableEntry::Generic: {
	    DEBUG (
		errf->print (_func_name).print (": generic cancellable").pendl ();
	    )

	    cancellable_entry->checkpoint->cancellables.remove (cancellable_entry->cancellables_el);
	} break;
	case CancellableEntry::Unconditional: {
	    DEBUG (
		errf->print (_func_name).print (": unconditional cancellable").pendl ();
	    )

	    cancellable_entry->checkpoint->cancellables.remove (cancellable_entry->cancellables_el);
	} break;
	case CancellableEntry::Sticky: {
	    DEBUG (
		errf->print (_func_name).print (": sticky cancellable").pendl ();
	    )

	    cancellable_entry->checkpoint->sticky_cancellables.remove (cancellable_entry->cancellables_el);
	} break;
	default:
            unreachable ();
    }

    cancellable_entry->release ();
}

void
CheckpointTracker::setBarrier ()
{
  FUNC_NAME (
    static char const * const _func_name = CLASS_NAME ".setBarrier";
  )

    DEBUG (
	errf->print (_func_name).print (": checkpoint 0x").printHex ((Uint64) checkpoints.getLast ()).pendl ();
    )

    checkpoints.getLast ()->is_barrier = true;
}

CheckpointTracker::CheckpointTracker ()
    : checkpoint_vstack (1 << 16 /* block_size */)
{
    VStack::Level const vstack_level = checkpoint_vstack.getLevel ();
    Checkpoint * const checkpoint = new (checkpoint_vstack.push_malign (sizeof (Checkpoint), alignof (Checkpoint))) Checkpoint;
    checkpoint->vstack_level = vstack_level;
    checkpoints.append (checkpoint);

//    errf->print ("--- FIRST LEVEL ").print (checkpoint_vstack.getLevel ()).pendl ();
}

CheckpointTracker::~CheckpointTracker ()
{
    Checkpoint *cur_checkpoint = checkpoints.getFirst ();
    while (cur_checkpoint != NULL) {
	Checkpoint * const next_checkpoint = checkpoints.getNext (cur_checkpoint);
	cur_checkpoint->~Checkpoint ();
	cur_checkpoint = next_checkpoint;
    }

//    errf->print ("--- LEVEL ").print (checkpoint_vstack.getLevel ()).pendl ();
}

}

