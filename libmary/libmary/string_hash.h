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


#ifndef LIBMARY__STRING_HASH__H__
#define LIBMARY__STRING_HASH__H__


#include <libmary/hash.h>


namespace M {

template <class T> class StringHash_anybase;

class GenericStringHash
{
    template <class T> friend class StringHash_anybase;

private:
    class Entry
    {
    public:
	Ref<String> str;

        Entry (ConstMemory const mem)
            : str (grab (new (std::nothrow) String (mem)))
        {}
    };

public:
    // GenericStringHash::EntryKey is useful to create EntryKey fields
    // inside classes of type T when StringHash<T> is used.
    class EntryKey
    {
        template <class T> friend class StringHash_anybase;
    private:
	Entry *entry;
	EntryKey (Entry * const entry) : entry (entry) {}
    public:
	operator bool () const { return entry; }
	ConstMemory getKey() const { return entry->str->mem(); }
	EntryKey () : entry (NULL) {}

#if 0
	// Methods for C API binding.
	void *getAsVoidPtr () const { return static_cast <void*> (entry); }
	static EntryKey fromVoidPtr (void *ptr) { return EntryKey (static_cast <Entry*> (ptr)); }
#endif
    };
};

template <class T>
class StringHash_anybase
{
private:
    class Entry : public GenericStringHash::Entry,
                  public HashEntry<>
    {
    public:
	T data;

	Entry (ConstMemory const mem,
	       T data)
	    : GenericStringHash::Entry (mem),
	      data (data)
	{}

	Entry (ConstMemory const mem)
	    : GenericStringHash::Entry (mem)
	{}
    };

    typedef Hash< Entry,
		  ConstMemory,
		  MemberExtractor< GenericStringHash::Entry,
				   Ref<String>,
				   &Entry::str,
				   ConstMemory,
				   AccessorExtractor< String,
						      Memory,
						      &String::mem > >,
		  MemoryComparator<>,
		  DefaultStringHasher >
	    StrHash;

    StrHash hash;

public:
    class EntryKey
    {
	friend class StringHash_anybase;
    private:
	Entry *entry;
	EntryKey (Entry * const entry) : entry (entry) {}
    public:
	operator bool () const { return entry; }
	ConstMemory getKey() const { return entry->str->mem(); }
	T getData () const { return entry->data; }
	T* getDataPtr() const { return &entry->data; }
	EntryKey () : entry (NULL) {}

        operator GenericStringHash::EntryKey ()
            { return GenericStringHash::EntryKey (entry); }

        EntryKey (GenericStringHash::EntryKey const mt_nonnull entry_key)
            : entry (static_cast <Entry*> (entry_key.entry))
        {}

	// Methods for C API binding.
	void *getAsVoidPtr () const { return static_cast <void*> (entry); }
	static EntryKey fromVoidPtr (void *ptr) { return EntryKey (static_cast <Entry*> (ptr)); }
    };

    bool isEmpty () const { return hash.isEmpty (); }

    EntryKey add (ConstMemory const &mem,
		  T data)
    {
	Entry * const entry = new (std::nothrow) Entry (mem, data);
        assert (entry);
	hash.add (entry);
	return entry;
    }

    EntryKey addEmpty (ConstMemory const &mem)
    {
	Entry * const entry = new (std::nothrow) Entry (mem);
        assert (entry);
	hash.add (entry);
	return entry;
    }

    void remove (EntryKey const key)
    {
        if (key.entry) {
            hash.remove (key.entry);
            delete key.entry;
        }
    }

    // TODO Why not to use "lookup (ConstMemory mem)" ?
    template <class C>
    EntryKey lookup (C key) { return hash.lookup (key); }

    StringHash_anybase (Size const initial_hash_size,
			bool const growing)
	: hash (initial_hash_size, growing)
    {}

    ~StringHash_anybase ()
    {
	typename StrHash::iter iter (hash);
	while (!hash.iter_done (iter)) {
	    Entry * const entry = hash.iter_next (iter);
	    delete entry;
	}
    }

  // Iterators

    class iter
    {
	friend class StringHash_anybase;
    private:
	typename StrHash::iter _iter;
    public:
	iter () {}
	iter (StringHash_anybase &hash) : _iter (hash.hash) {}
    };

    void     iter_begin (iter &iter) const { hash.iter_begin (iter._iter); }
    EntryKey iter_next  (iter &iter) const { return hash.iter_next (iter._iter); }
    bool     iter_done  (iter &iter) const { return hash.iter_done (iter._iter); }


  // ________________________________ iterator _________________________________

    class iterator
    {
    private:
	typename StrHash::iterator iter;

    public:
	iterator (StringHash_anybase &hash) : iter (hash.hash) {}
        iterator () {}

        bool operator == (iterator const &iter) const { return this->iter == iter.iter; }
        bool operator != (iterator const &iter) const { return this->iter != iter.iter; }

        bool done () const { return iter.done(); }
        T* next () { return &iter.next()->data; }
    };

  // ___________________________________________________________________________

};

template < class T, class Base = EmptyBase >
class StringHash : public StringHash_anybase<T>,
		   public Base
{
public:
    StringHash (Size const initial_hash_size = 16,
		bool const growing = true)
	: StringHash_anybase<T> (initial_hash_size, growing)
    {}
};

}


#endif /* LIBMARY__STRING_HASH__H__ */

