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


#include <mycpp/io.h>

#include <scruffy/name_tracker.h>

#define DEBUG(a) a
// Checkpoints
#define DEBUG_CHECKPOINT(a) ;

using namespace MyCpp;
//using namespace MyLang;
using namespace Pargen;

namespace Scruffy {

namespace NameTracker_Priv {}
using namespace NameTracker_Priv;

namespace NameTracker_Priv {

    class Cancellable_NamespaceEntry : public CheckpointTracker::Cancellable
    {
    public:
	Ref<Cpp::NamespaceEntry> namespace_entry;

	void cancel ()
	{
	    errf->print ("Scruffy.NameTracker.Cancellable_NamespaceEntry.cancel").pendl ();
	    // TODO
	    //    namespace_entry->parent_namespace->namespace_entries.remove (parent_namespace_link);
	}
    };

    class Cancellable_Name : public CheckpointTracker::Cancellable
    {
    public:
	Ref<Cpp::Name> name;

	void cancel () {
	    errf->print ("Scruffy.NameTracker.Cancellable_Name.cancel: \"").print (name->name_str).print ("\"").pendl ();

	    name->parent_namespace->names.remove (name->parent_namespace_link);
	}
    };

    class Cancellable_ObjectEntry : public CheckpointTracker::Cancellable
    {
    public:
	Ref<Cpp::Namespace> namespace_;
	Cpp::Namespace::ObjectEntryMap::Entry object_map_entry;

	void cancel ()
	{
	    errf->print ("Scruffy.NameTracker.Cancellable_ObjectEntry.cancel").pendl ();

	    namespace_->objects.remove (object_map_entry);
	}
    };

    class Cancellable_TypeEntry : public CheckpointTracker::Cancellable
    {
    public:
	Ref<Cpp::Namespace> namespace_;
	Cpp::Namespace::TypeEntryMap::Entry type_map_entry;

	void cancel ()
	{
	    errf->print ("Scruffy.NameTracker.Cancellable_TypeEntry.cancel").pendl ();

	    namespace_->types.remove (type_map_entry);
	}
    };

    class Cancellable_TemporalNamespace : public CheckpointTracker::Cancellable
    {
    public:
	NameTracker *name_tracker;
	List< Ref<Cpp::Namespace> >::Element *temporal_namespace_el;

	void do_cancel ()
	{
	    errf->print ("Scruffy.NameTracker.Cancellable_TemporalNamespace.do_cancel").pendl ();

	    name_tracker->temporal_namespaces.remove (temporal_namespace_el);
	}

	void commit ()
	{
	    errf->print ("Scruffy.NameTracker.Cancellable_TemporalNamespace.commit").pendl ();

	    do_cancel ();
	}

	void cancel ()
	{
	    errf->print ("Scruffy.NameTracker.Cancellable_TemporalNamespace.cancel").pendl ();

	    do_cancel ();
	}
    };

}

void
NameTracker::newCheckpoint ()
{
    DEBUG_CHECKPOINT (
	errf->print ("Scruffy.NameTracker.newCheckpoint").pendl ();
    )

    checkpoint_tracker.newCheckpoint ();
}

void
NameTracker::commitCheckpoint ()
{
    DEBUG_CHECKPOINT (
	errf->print ("Scruffy.NameTracker.commitCheckpoint").pendl ();
    )

    checkpoint_tracker.commitCheckpoint ();
}

void
NameTracker::cancelCheckpoint ()
{
    DEBUG_CHECKPOINT (
	errf->print ("Scruffy.NameTracker.cancelCheckpoint").pendl ();
    )

    checkpoint_tracker.cancelCheckpoint ();
}

void
NameTracker::beginNamespace (ConstMemoryDesc const &namespace_name)
{
    errf->print ("Scruffy.NameTracker.beginNamespace").pendl ();
}

void
NameTracker::endNamespace ()
{
    errf->print ("Scruffy.NameTracker.endNamespace").pendl ();
}

void
NameTracker::beginTemporalNamespace ()
{
    errf->print ("Scruffy.NameTracker.beginTemporalNamespace").pendl ();

    Ref<Cpp::Namespace> temporal_namespace = grab (new Cpp::Namespace);
    List< Ref<Cpp::Namespace> >::Element *temporal_namespace_el =
	    temporal_namespaces.append (temporal_namespace);

    Ref<Cancellable_TemporalNamespace> cancellable_temporal_namespace =
	    grab (new Cancellable_TemporalNamespace);
    cancellable_temporal_namespace->name_tracker = this;
    cancellable_temporal_namespace->temporal_namespace_el = temporal_namespace_el;

    checkpoint_tracker.addStickyCancellable (cancellable_temporal_namespace);
    checkpoint_tracker.setBarrier ();
}

void
NameTracker::endTemporalNamespace ()
{
    errf->print ("Scruffy.NameTracker.endTemporalNamespace").pendl ();

  // Unnecessary (!)
}

#if 0
void
NameTracker::usingNamespace (ConstMemoryDesc const &namespace_name)
{
}
#endif

void
NameTracker::addName (Cpp::Name const * const name)
{
    static char const * const _func_name = "Scruffy.NameTracker.addName";

    abortIf (name == NULL);

    errf->print (_func_name).print (": ").print (name->toString ()).pendl ();

    // TODO
    //
    // 1. Lookup the name;
    // 2. If the name already exists, then check if it is a redeclaration, etc.
    // 3. Add the name to the current namespace.

    abortIf (cur_namespace_entry == NULL ||
	     cur_namespace_entry->namespace_.isNull ());

    Cpp::Namespace *namespace_;
    if (!temporal_namespaces.isEmpty ())
	namespace_ = temporal_namespaces.last->data;
    else
	namespace_ = cur_namespace_entry->namespace_;

    Cpp::Name::NameMap::Entry name_entry = namespace_->names.lookupValue (name);
    if (name_entry.isNull ()) {
	errf->print (_func_name).print (": no such name yet").pendl ();
    } else {
	errf->print (_func_name).print ("--- !!! There's such name already").pendl ();
    }

    Ref<Cpp::Name> new_name = grab (new Cpp::Name);
    new_name->setName (name);

    new_name->parent_namespace = namespace_;
    new_name->parent_namespace_link = namespace_->names.add (new_name);

  // Adding a cancellable

    Ref<Cancellable_Name> cancellable_name = grab (new Cancellable_Name);
    cancellable_name->name = new_name;

    checkpoint_tracker.addStickyCancellable (cancellable_name);
}

void
NameTracker::addObject (Cpp::Object *object,
			Cpp::Name const *name)
{
    static char const * const _func_name = "Scruffy.NameTracker.addObject";

    abortIf (object == NULL ||
	     name == NULL);

    errf->print (_func_name).pendl ();

    abortIf (cur_namespace_entry == NULL ||
	     cur_namespace_entry->namespace_.isNull ());

    Cpp::Namespace *namespace_;
    if (!temporal_namespaces.isEmpty ())
	namespace_ = temporal_namespaces.last->data;
    else
	namespace_ = cur_namespace_entry->namespace_;

    {
	Cpp::Namespace::ObjectEntryMap::Entry object_map_entry = namespace_->objects.lookup (*name);
	if (object_map_entry.isNull ()) {
	    errf->print (_func_name).print (": no such object yet").pendl ();
	} else {
	    errf->print (_func_name).print ("--- !!! There's such object already").pendl ();
	}
    }

    Ref<Cpp::Namespace::ObjectEntry> object_entry = grab (new Cpp::Namespace::ObjectEntry);
    object_entry->object = object;
    object_entry->name = grab (new Cpp::Name);
    object_entry->name->setName (name);

    Cpp::Namespace::ObjectEntryMap::Entry object_map_entry = namespace_->objects.add (object_entry);

    {
      // Adding a cancellable

	Ref<Cancellable_ObjectEntry> cancellable_object_entry = grab (new Cancellable_ObjectEntry);
	cancellable_object_entry->namespace_ = namespace_;
	cancellable_object_entry->object_map_entry = object_map_entry;

	checkpoint_tracker.addStickyCancellable (cancellable_object_entry);
    }
}

void
NameTracker::addType (Cpp::TypeDesc *type_desc,
		      String *identifier_str)
{
    static char const * const _func_name = "Scruffy.NameTracker.addType";

    abortIf (type_desc == NULL ||
	     identifier_str == NULL);

    errf->print (_func_name).pendl ();

    abortIf (cur_namespace_entry == NULL ||
	     cur_namespace_entry->namespace_.isNull ());

    Cpp::Namespace *namespace_;
    if (!temporal_namespaces.isEmpty ())
	namespace_ = temporal_namespaces.last->data;
    else
	namespace_ = cur_namespace_entry->namespace_;

    {
	Cpp::Namespace::TypeEntryMap::Entry type_map_entry =
		namespace_->types.lookup (identifier_str->getMemoryDesc ());
	if (type_map_entry.isNull ()) {
	    errf->print (_func_name).print (": no such type yet").pendl ();
	} else {
	    errf->print (_func_name).print ("--- !!! There's such type already").pendl ();
	}
    }

    Ref<Cpp::Namespace::TypeEntry> type_entry = grab (new Cpp::Namespace::TypeEntry);
    type_entry->type_desc = type_desc;
    type_entry->identifier_str = identifier_str;

    Cpp::Namespace::TypeEntryMap::Entry type_map_entry = namespace_->types.add (type_entry);

    {
      // Adding a cancellable

	Ref<Cancellable_TypeEntry> cancellable_type_entry = grab (new Cancellable_TypeEntry);
	cancellable_type_entry->namespace_ = namespace_;
	cancellable_type_entry->type_map_entry = type_map_entry;

	checkpoint_tracker.addStickyCancellable (cancellable_type_entry);
    }
}

Ref<Cpp::Namespace::TypeEntry>
NameTracker::lookupType (ConstMemoryDesc const &identifier_str)
{
  // TODO Lookup according to real lookup rules.

    abortIf (cur_namespace_entry == NULL ||
	     cur_namespace_entry->namespace_.isNull ());

    Cpp::Namespace *namespace_;
    if (!temporal_namespaces.isEmpty ())
	namespace_ = temporal_namespaces.last->data;
    else
	namespace_ = cur_namespace_entry->namespace_;

    Cpp::Namespace::TypeEntryMap::Entry type_map_entry = namespace_->types.lookup (identifier_str);
    if (type_map_entry.isNull ())
	return NULL;

    return type_map_entry.getData ();
}

Ref<Cpp::Name>
NameTracker::lookupName (Cpp::Name const * const name)
{
    errf->print ("Scruffy.NameTracker.lookupName").pendl ();

    abortIf (name == NULL);

    abortIf (cur_namespace_entry == NULL ||
	     cur_namespace_entry->namespace_.isNull ());

    Cpp::Namespace *namespace_;
    if (!temporal_namespaces.isEmpty ())
	namespace_ = temporal_namespaces.last->data;
    else
	namespace_ = cur_namespace_entry->namespace_;

    Cpp::Name::NameMap::Entry name_entry = namespace_->names.lookupValue (name);
    if (name_entry.isNull ())
	return NULL;

    return name_entry.getData ();
}

NameTracker::NameTracker ()
{
    root_namespace_entry = grab (new Cpp::NamespaceEntry);
    root_namespace_entry->namespace_ = grab (new Cpp::Namespace);
    cur_namespace_entry = root_namespace_entry;
}

}

