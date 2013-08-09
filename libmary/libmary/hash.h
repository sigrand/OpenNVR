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


#ifndef __LIBMARY__HASH__H__
#define __LIBMARY__HASH__H__


#include <libmary/types.h>
#include <libmary/intrusive_list.h>
#include <libmary/intrusive_avl_tree.h>


namespace M {

class DefaultStringHasher
{
public:
    static Uint32 hash (ConstMemory const &mem)
    {
	Uint32 result = 5381;

	for (Size i = 0; i < mem.len(); ++i)
	    result = ((result << 5) + result) ^ mem.mem() [i];

	return result;
    }
};

class Hash_Default;

template <class HashName = Hash_Default>
class HashEntry : public IntrusiveAvlTree_Node<HashName>,
		  public IntrusiveListElement<HashName>
{
//    template <class T, class HashName> friend class Hash_common;

    template <class T,
              class KeyType,
              class Extractor,
              class Comparator,
              class Hasher,
              class HashName_>
            friend class Hash_anybase;

    template <class T,
              class KeyType,
              class Extractor,
              class Comparator,
              class Hasher,
              class HashName_,
              class Base>
            friend class Hash;

private:
    Uint32 unrolled_hash;
};

#if 0
template < class T,
	   class HashName >
class Hash_common
{
};
#endif

// TODO Default template parameter values are poorly thought.
template < class T,
	   class KeyType,
	   class Extractor = DirectExtractor<T>,
	   class Comparator = DirectComparator<T>,
	   class Hasher = DefaultStringHasher,
	   class HashName = Hash_Default>
class Hash_anybase
{
private:
#if 0
    class HashEntryComparator
    {
    public:
#if 0
	static bool greater (HashEntry<HashName> * const left,
			     HashEntry<HashName> * const right)
	{
	    if (left->unrolled_hash > right->unrolled_hash)
		return true;

//	    if (Comparator::greater (left, right))
	    // FIXME This is a hack. There's no place for pointer casts.
	    if ((UintPtr) left > (UintPtr) right)
		return true;

	    return false;
	}

	static bool equals (HashEntry<HashName> * const left,
			    HashEntry<HashName> * const right)
	{
	    if (left->unrolled_hash != right->unrolled_hash)
		return false;

	    if ((UintPtr) left == (UintPtr) right)
		return true;

	    return false;
	}
#endif

	static bool greater (KeyType left,
			     KeyType right)
	{
	}
    };
#endif

    class Cell
    {
    public:
	IntrusiveAvlTree< T, Extractor, Comparator, HashName > tree;
    };

    bool growing;

    Cell *hash_table;
    Size  hash_size;

    IntrusiveList<T, HashName> node_list;

public:
    bool isEmpty () const
    {
	return node_list.isEmpty();
    }

    void add (T * const entry)
    {
	Uint32 const unrolled_hash = Hasher::hash (Extractor::getValue (entry));
	Uint32 const hash = unrolled_hash % hash_size;

	entry->unrolled_hash = unrolled_hash;
	hash_table [hash].tree.add (entry);

	node_list.append (entry);

	// TODO if (growing) ... then grow.
    }

    void remove (T * const entry)
    {
	hash_table [entry->unrolled_hash % hash_size].tree.remove (entry);
	node_list.remove (entry);
    }

    void clear ()
    {
	node_list.clear ();

	delete[] hash_table;
	hash_table = new Cell [hash_size];
    }

    template <class C>
    T* lookup (C key)
    {
	Uint32 const unrolled_hash = Hasher::hash (key);
	Uint32 const hash = unrolled_hash % hash_size;

	return hash_table [hash].tree.lookup (key);
    }

    Hash_anybase (Size const initial_hash_size,
		  bool const growing)
	: growing (growing),
	  hash_size (initial_hash_size)
    {
	hash_table = new Cell [hash_size];
    }

#if 0
// Unnecessary
    // Default hash configuration suitable for most uses.
    Hash_anybase ()
	: growing (true),
	  hash_size (16)
    {
	hash_table = new Cell [hash_size];
    }
#endif

    ~Hash_anybase ()
    {
	delete[] hash_table;
    }


  // ________________________________ iterator _________________________________

    class iterator
    {
    private:
	typename IntrusiveList<T, HashName>::iterator node_iter;

    public:
        iterator (Hash_anybase< T, KeyType, Extractor, Comparator, Hasher, HashName > &hash)
                : node_iter (hash.node_list) {}
        iterator () {}

        bool operator == (iterator const &iter) const { return node_iter == iter.node_iter; }
        bool operator != (iterator const &iter) const { return node_iter != iter.node_iter; }

        bool done () const { return node_iter.done(); }
        T* next () { return node_iter.next(); }
    };


  // __________________________________ iter ___________________________________

    class iter
    {
	friend class Hash_anybase< T, KeyType, Extractor, Comparator, Hasher, HashName >;

    private:
	typename IntrusiveList<T, HashName>::iter node_iter;

    public:
	iter (Hash_anybase< T, KeyType, Extractor, Comparator, Hasher, HashName > &hash)
	    : node_iter (hash.node_list) {}
	iter () {}

 	// Methods for C API binding.
	void *getAsVoidPtr () const { return node_iter.getAsVoidPtr (); }
	static iter fromVoidPtr (void *ptr)
	{
	    iter it;
	    it.node_iter = IntrusiveList<T, HashName>::iter::fromVoidPtr (ptr);
	    return it;
	}
    };

    void iter_begin (iter &iter) const
    {
	node_list.iter_begin (iter.node_iter);
    }

    T* iter_next (iter &iter) const
    {
	return node_list.iter_next (iter.node_iter);
    }

    bool iter_done (iter &iter) const
    {
	return node_list.iter_done (iter.node_iter);
    }

  // ___________________________________________________________________________

};

template < class T,
	   class KeyType,
	   class Extractor = DirectExtractor<T>,
	   class Comparator = DirectComparator<T>,
	   class Hasher = DefaultStringHasher,
	   class HashName = Hash_Default,
	   class Base = EmptyBase>
class Hash : public Hash_anybase< T, KeyType, Extractor, Comparator, Hasher, HashName >,
	     public Base
{
public:
    Hash (Size const initial_hash_size = 16,
	  bool const growing = true)
	: Hash_anybase< T, KeyType, Extractor, Comparator, Hasher, HashName > (initial_hash_size, growing)
    {
    }

#if 0
// Unnecessary
    Hash ()
    {
    }
#endif

    ~Hash ()
    {
// Wrong	Hash_anybase< T, KeyType, Extractor, Comparator, Hasher, HashName >::~Hash_anybase< T, KeyType, Extractor, Comparator, Hasher, HashName > ();
    }
};

}


#endif /* __LIBMARY__HASH__H__ */

