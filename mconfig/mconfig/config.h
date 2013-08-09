/*  MConfig - C++ library for working with configuration files
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


#ifndef MCONFIG__CONFIG__H__
#define MCONFIG__CONFIG__H__


#include <libmary/libmary.h>


namespace MConfig {

using namespace M;

enum BooleanValue {
    Boolean_Invalid,
    Boolean_Default,
    Boolean_True,
    Boolean_False
};

class Attribute : public HashEntry<>
{
    friend class Section;
    friend class Config;

private:
    Ref<String> name_str;
    Ref<String> value_str;

public:
    bool hasValue () const
    {
        return value_str != NULL;
    }

    ConstMemory getName () const
    {
        if (!name_str)
            return ConstMemory();

        return name_str->mem();
    }

    ConstMemory getValue () const
    {
        if (!value_str)
            return ConstMemory();

        return value_str->mem();
    }

    void setValue (bool        const has_value,
                   ConstMemory const value)
    {
        if (has_value)
            value_str = grab (new String (value));
        else
            value_str = NULL;
    }

    Attribute (ConstMemory const name,
               bool        const has_value,
               ConstMemory const value)
        : name_str  (grab (new String (name)))
    {
        if (has_value)
            value_str = grab (new String (value));
    }
};

class SectionEntry : public HashEntry<>
{
    friend class Section;
    friend class Config;

public:
    enum Type
    {
	Type_Option,
	Type_Section,
	Type_Invalid
    };

private:
    Ref<String> name_str;
    Type const type;

public:
    Type        getType () const { return type; }
    ConstMemory getName () const { return name_str->mem(); }

    SectionEntry (Type        const type,
		  ConstMemory const entry_name)
	: type (type)
    {
	name_str = grab (new String (entry_name));
    }

    virtual ~SectionEntry () {}
};

class Value : public IntrusiveListElement<>
{
private:
    Ref<String> value_str;

    enum CachedType {
	CachedType_None,
	CachedType_Double,
	CachedType_Int64,
	CachedType_Uint64
    };

    CachedType cached_type;
    bool bad_value;

    union {
	double cached_double;
	Int64  cached_int64;
	Uint64 cached_uint64;
    } cached_value;

public:
    void setValue (ConstMemory const mem)
    {
	value_str = grab (new String (mem));
    }

    Result getAsDouble (double *ret_val);

    Result getAsInt64 (Int64 *ret_val);

    Result getAsUint64 (Uint64 *ret_val);

    Ref<String> getAsString ()
    {
	return value_str;
    }

    ConstMemory mem ()
    {
	return value_str->mem();
    }

    Value ()
	: cached_type (CachedType_None),
	  bad_value (false)
    {
    }
};

class Option : public SectionEntry
{
private:
    typedef IntrusiveList<Value> ValueList;

    ValueList value_list;

public:
    void addValue (ConstMemory const mem)
    {
	Value * const value = new Value;
	value->setValue (mem);
	value_list.append (value);
    }

    void removeValues ()
    {
	{
	    ValueList::iter iter (value_list);
	    while (!value_list.iter_done (iter)) {
		Value * const value = value_list.iter_next (iter);
		delete value;
	    }
	}

	value_list.clear ();
    }

    Value* getValue ()
    {
	return value_list.getFirst();
    }

    BooleanValue getBoolean ();

    void dump (OutputStream *outs,
	       unsigned      nest_level);

    Option (ConstMemory const option_name)
	: SectionEntry (SectionEntry::Type_Option, option_name)
    {
    }

    ~Option ()
    {
	removeValues ();
    }

  // Iterators.

    class iter
    {
	friend class Option;

    private:
	ValueList::iter iter_;

    public:
	iter ()
	{
	}

	iter (Option &option)
	    : iter_ (option.value_list)
	{
	}
    };

    void iter_begin (iter &iter)
    {
	value_list.iter_begin (iter.iter_);
    }

    Value* iter_next (iter &iter)
    {
	return value_list.iter_next (iter.iter_);
    }

    bool iter_done (iter &iter)
    {
	return value_list.iter_done (iter.iter_);
    }
};

class Section : public SectionEntry
{
private:
    typedef Hash< Attribute,
                  Memory,
                  MemberExtractor< Attribute,
                                   Ref<String>,
                                   &Attribute::name_str,
                                   Memory,
                                   AccessorExtractor< String,
                                                      Memory,
                                                      &String::mem > >,
                  MemoryComparator<> >
            AttributeHash;

    typedef Hash< SectionEntry,
		  Memory,
		  MemberExtractor< SectionEntry,
				   Ref<String>,
				   &SectionEntry::name_str,
				   Memory,
				   AccessorExtractor< String,
						      Memory,
						      &String::mem > >,
		  MemoryComparator<> >
	    SectionEntryHash;

    AttributeHash    attribute_hash;
    SectionEntryHash section_entry_hash;

public:
    Attribute* getAttribute (ConstMemory attr_name);

    SectionEntry* getSectionEntry (ConstMemory path,
				   bool create = false,
				   SectionEntry::Type section_entry_type = SectionEntry::Type_Invalid);

    Option* getOption (ConstMemory path,
		       bool create = false);

    Section* getSection (ConstMemory path,
			 bool create = false);

    SectionEntry* getSectionEntry_nopath (ConstMemory section_entry_name);

    Option* getOption_nopath (ConstMemory option_name,
			      bool create = false);

    Section* getSection_nopath (ConstMemory section_name,
				bool create = false);

    // Takes ownership of @attribute.
    void addAttribute (Attribute *attr);

    // Takes ownership of @option.
    void addOption (Option *option);

    // Takes ownership of @section.
    void addSection (Section *section);

    void removeSectionEntry (SectionEntry *section_entry);

    void dump (OutputStream *outs,
	       unsigned      nest_level = 0);

    void dumpBody (OutputStream *outs,
		   unsigned      nest_level = 0);

    Section (ConstMemory const section_name)
	: SectionEntry (SectionEntry::Type_Section, section_name)
    {
    }

    ~Section ();


  // __________________________________ iter ___________________________________

    class iter
    {
	friend class Section;

    private:
	SectionEntryHash::iter iter_;

    public:
	iter (Section &section) : iter_ (section.section_entry_hash) {}
	iter () {}

 	// Methods for C API binding.
	void *getAsVoidPtr () const { return iter_.getAsVoidPtr(); }
	static iter fromVoidPtr (void *ptr)
	{
	    iter it;
	    it.iter_ = SectionEntryHash::iter::fromVoidPtr (ptr);
	    return it;
	}
    };

    void iter_begin (iter &iter)
        { section_entry_hash.iter_begin (iter.iter_); }

    SectionEntry* iter_next (iter &iter)
        { return section_entry_hash.iter_next (iter.iter_); }

    bool iter_done (iter &iter)
        { return section_entry_hash.iter_done (iter.iter_); }


  // ________________________________ iterator _________________________________

    class iterator
    {
    private:
        SectionEntryHash::iterator sect_iter;

    public:
        iterator (Section &section) : sect_iter (section.section_entry_hash) {}
        iterator () {}

        bool operator == (iterator const &iter) const { return sect_iter == iter.sect_iter; }
        bool operator != (iterator const &iter) const { return sect_iter != iter.sect_iter; }

        bool done () const { return sect_iter.done(); }
        SectionEntry* next () { return sect_iter.next(); }
    };


  // ___________________________ attribute_iterator ____________________________

    class attribute_iterator
    {
    private:
        AttributeHash::iterator hash_iter;

    public:
        attribute_iterator (Section &section) : hash_iter (section.attribute_hash) {}
        attribute_iterator () {}

        bool operator == (attribute_iterator const &iter) const { return hash_iter == iter.hash_iter; }
        bool operator != (attribute_iterator const &iter) const { return hash_iter != iter.hash_iter; }

        bool done () const { return hash_iter.done(); }
        Attribute* next () { return hash_iter.next(); }
    };

  // ___________________________________________________________________________

};

class GetResult
{
public:
    enum Value {
	Invalid = 0,
	Default,
	Success
    };
    operator Value () const { return value; }
    GetResult (Value const value) : value (value) {}
    GetResult () {}
private:
    Value value;
};

class Config : public Object
{
#if 0
private:
    StateMutex mutex;


  // _____________________________ event_informer ______________________________

public:
    typedef Uint32 update_key;

    struct Events
    {
        void (*configReload) (Config    *config,
                              UpdateKey  update_key,
                              void      *cb_data);
    };

private:
    Informer_<Events> event_informer;

    struct InformConfigReload_Data
    {
        Config *config;
        UpdateKey update_key;
    };

    static void informConfigReload (Events * const events,
                                    void   * const cb_data,
                                    void   * const _config)
    {
        Config * const config = static_cast <Config*> (_config);
        if (events->configReload)
            events->configReload (config, cb_data);
    }

    void fireConfigReload () { event_informer.informAll (informConfigReload, /* data */); }

public:
    Informer_<Events>* getEventInformer () { return &event_informer; }


  // ____________________________ config reloading _____________________________

private:
    Uint32 next_update_key;

public:
    // All nodes are marked as "not changed" (update key  increment).
    // Original values are preserved for later comparison (copy-on-write).
    mt_locks (update_mutex) void beginUpdate ();

    // Update notification.
    mt_unlocks (update_mutex) void endUpdate ();

    mt_locks (update_mutex) void updateLock ();

    mt_unlocks (update_mutex) void updateUnlock ();

  // ___________________________________________________________________________
#endif


    Section root_section;


public:
    // Helper method to avoid excessive explicit calls to getRootSection().
    Option* getOption (ConstMemory const path,
		       bool        const create = false)
    {
	return root_section.getOption (path, create);
    }

    // Helper method to avoid excessive explicit calls to getRootSection().
    Section* getSection (ConstMemory const path,
			 bool        const create = false)
    {
	return root_section.getSection (path, create);
    }

    Option* setOption (ConstMemory path,
		       ConstMemory value);

    ConstMemory getString (ConstMemory  path,
			   bool        *ret_is_set = NULL);

    ConstMemory getString_default (ConstMemory path,
				   ConstMemory default_value);

    GetResult getUint64 (ConstMemory  path,
			 Uint64      *ret_value);

    GetResult getUint64_default (ConstMemory   const path,
				 Uint64      * const ret_value,
				 Uint64        const default_value)
    {
	GetResult const res = getUint64 (path, ret_value);
	if (res == GetResult::Default) {
	    if (ret_value) {
		*ret_value = default_value;
	    }
	    return GetResult::Default;
	}
	return res;
    }

    BooleanValue getBoolean (ConstMemory path);

    Section* getRootSection ()
    {
	return &root_section;
    }

    void dump (OutputStream *outs,
	       unsigned      nest_level = 0);

    Config ()
        : // event_informer (this /* coderef_container */, &mutex),
	  root_section ("root") 
    {
    }
};

}


#endif /* MCONFIG__CONFIG__H__ */

