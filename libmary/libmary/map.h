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


#ifndef __LIBMARY__MAP__H__
#define __LIBMARY__MAP__H__


#include <libmary/avl_tree.h>


namespace M {

template < class T,
	   class Extractor,
	   class Comparator,
	   class Base >
	class Map;

template <class T>
class MapBase
{
public:
    class Entry
    {
	friend class MapBase;

	template < class A,
		   class Extractor,
		   class Comparator,
		   class Base>
		friend class Map;

    protected:
	typename AvlTreeBase<T>::Node *avl_tree_node;

	Entry (typename AvlTreeBase<T>::Node *avl_tree_node)
	{
	    this->avl_tree_node = avl_tree_node;
	}

    public:
	T& getData () const
	{
	    assert (avl_tree_node);
	    return avl_tree_node->value;
	}

	bool isNull () const
	{
	    return avl_tree_node == NULL;
	}

	bool equals (Entry const &entry) const
	{
	    return avl_tree_node == entry.avl_tree_node;
	}

	Entry ()
	{
	    avl_tree_node = NULL;
	}
    };


  // ________________________________ iterator _________________________________

    class iterator
    {
    private:
        typename AvlTreeBase<T>::iterator iter;

    public:
	template < class Extractor,
		   class Comparator,
		   class MapBase >
        iterator (Map< T, Extractor, Comparator, MapBase > const &map) : iter (map.avl_tree) {}

        bool operator == (iterator const &iter) const { return this->iter == iter.iter; }
        bool operator != (iterator const &iter) const { return this->iter != iter.iter; }

        bool done () /* const */ { return iter.done (); }
        Entry next () { return Entry (iter.next ()); }
    };


  // ______________________________ data_iterator ______________________________

    class data_iterator
    {
    private:
        iterator iter;

    public:
	template < class Extractor,
		   class Comparator,
		   class MapBase >
        data_iterator (Map< T, Extractor, Comparator, MapBase > const &map) : iter (map) {}

        bool operator == (data_iterator const &iter) const { return this->iter == iter.iter; }
        bool operator != (data_iterator const &iter) const { return this->iter != iter.iter; }

        bool done () /* const */ { return iter.done (); }
        T& next () { return iter.next ().getData(); }
    };

  // ___________________________________________________________________________


    template <class Base = EmptyBase>
    class Iterator_ : public StatefulIterator<Entry, Base>
    {
    protected:
	typename AvlTreeBase<T>::Iterator iter;

    public:
	Entry next ()
	{
	    return Entry (&iter.next ());
	}

	bool done ()
	{
	    return iter.done ();
	}

	template < class Extractor,
		   class Comparator,
		   class MapBase >
	Iterator_ (Map< T, Extractor, Comparator, MapBase > const &map)
	    : iter (map.avl_tree)
	{
	}

	template < class Extractor,
		   class Comparator,
		   class MapBase >
	Iterator_ (typename Map< T, Extractor, Comparator, MapBase >::Entry const &entry)
	    : iter (entry.avl_tree_node)
	{
	}
    };
    typedef Iterator_<> Iterator;

    template <class Base = EmptyBase>
    class InverseIterator_ : public StatefulIterator<Entry, Base>
    {
    protected:
	typename AvlTreeBase<T>::InverseIterator iter;

    public:
	Entry next ()
	{
	    return Entry (&iter.next ());
	}

	bool done ()
	{
	    return iter.done ();
	}

	template < class Extractor,
		   class Comparator,
		   class MapBase >
	InverseIterator_ (Map< T, Extractor, Comparator, MapBase > const &map)
	    : iter (map.avl_tree)
	{
	}

#if 0
	template < class Extractor,
		   class Comparator,
		   class MapBase >
	InverseIterator_ (typename Map< T, Extractor, Comparator, MapBase >::Entry const &entry)
#endif
	InverseIterator_ (typename MapBase<T>::Entry const &entry)
	    : iter (entry.avl_tree_node)
	{
	}
    };
    typedef InverseIterator_<> InverseIterator;

    template <class Base = EmptyBase>
    class DataIterator_ :
	public StatefulExtractorIterator < T&,
					   MemberExtractor< typename AvlTreeBase<T>::Node,
							    T,
							    &AvlTreeBase<T>::Node::value >,
					   typename AvlTreeBase<T>::Iterator,
					   Base >
    {
    public:
	template < class Extractor,
		   class Comparator,
		   class MapBase >
	DataIterator_ (Map< T, Extractor, Comparator, MapBase > const &map)
	    : StatefulExtractorIterator < T&,
					  MemberExtractor< typename AvlTreeBase<T>::Node,
							   T,
							   &AvlTreeBase<T>::Node::value >,
					  typename AvlTreeBase<T>::Iterator,
					  Base > (
		      map.avl_tree.createIterator ())
	{
	}

	template < class Extractor,
		   class Comparator,
		   class MapBase >
	DataIterator_ (typename Map< T, Extractor, Comparator, MapBase >::Entry const &entry)
	    : StatefulExtractorIterator < T&,
					  MemberExtractor< typename AvlTreeBase<T>::Node,
							   T,
							   &AvlTreeBase<T>::Node::value >,
					  typename AvlTreeBase<T>::Iterator,
					  Base > (
		      typename AvlTreeBase<T>::Iterator (entry.avl_tree_node))
	{
	}
    };
    typedef DataIterator_<> DataIterator;

    template <class Base = EmptyBase>
    class InverseDataIterator_ :
	public StatefulExtractorIterator < T&,
					   MemberExtractor< typename AvlTreeBase<T>::Node,
							    T,
							    &AvlTreeBase<T>::Node::value >,
					   typename AvlTreeBase<T>::InverseIterator,
					   Base >
    {
    public:
	template < class Extractor,
		   class Comparator,
		   class MapBase >
	InverseDataIterator_ (Map< T, Extractor, Comparator, MapBase > const &map)
	    : StatefulExtractorIterator < T&,
					  MemberExtractor< typename AvlTreeBase<T>::Node,
							   T,
							   &AvlTreeBase<T>::Node::value >,
					  typename AvlTreeBase<T>::InverseIterator,
					  Base > (
		      map.avl_tree.createIterator ())
	{
	}

	template < class Extractor,
		   class Comparator,
		   class MapBase >
	InverseDataIterator_ (typename Map< T, Extractor, Comparator, MapBase >::Entry const &entry)
	    : StatefulExtractorIterator < T&,
					  MemberExtractor< typename AvlTreeBase<T>::Node,
							   T,
							   &AvlTreeBase<T>::Node::value >,
					  typename AvlTreeBase<T>::InverseIterator,
					  Base > (
		      typename AvlTreeBase<T>::Iterator (entry.avl_tree_node))
	{
	}
    };
    typedef InverseDataIterator_<> InverseDataIterator;
};

template < class T,
	   class Extractor = DirectExtractor<T>,
	   class Comparator = DirectComparator<T>,
	   class Base = EmptyBase >
class Map : public MapBase<T>,
	    public Base
{
    friend class MapBase<T>;

protected:
    AvlTree< T, Extractor, Comparator > avl_tree;

public:
    typename MapBase<T>::Entry add (T const &value)
    {
 	typename AvlTreeBase<T>::Node *node = avl_tree.addForValue (value);
	assert (node);
	node->value = value;
	return typename MapBase<T>::Entry (node);
    }

    template <class C>
    typename MapBase<T>::Entry addFor (C const &value)
    {
 	typename AvlTreeBase<T>::Node *node = avl_tree.addFor (value);
	assert (node);
	return typename MapBase<T>::Entry (node);
    }

    template <class C>
    typename MapBase<T>::Entry lookupValue (C const &value)
    {
	return typename MapBase<T>::Entry (avl_tree.lookupValue (value));
    }

    template <class C>
    typename MapBase<T>::Entry lookup (C const &value)
    {
	typename AvlTreeBase<T>::Node *node = avl_tree.lookup (value);
	return typename MapBase<T>::Entry (node);
    }

    typename MapBase<T>::Entry getLeftmost ()
    {
	return typename MapBase<T>::Entry (avl_tree.getLeftmost ());
    }

    typename MapBase<T>::Entry getRightmost ()
    {
	return typename MapBase<T>::Entry (avl_tree.getRightmost ());
    }

    void remove (typename MapBase<T>::Entry const &el)
    {
	avl_tree.remove (el.avl_tree_node);
    }

    void clear ()
    {
	avl_tree.clear ();
    }

    typename MapBase<T>::Iterator createIterator () const
    {
	return typename MapBase<T>::Iterator (*this);
    }

    typename MapBase<T>::DataIterator createDataIterator () const
    {
	return typename MapBase<T>::DataIterator (*this);
    }

    bool isEmpty ()
    {
	return avl_tree.top == NULL;
    }
};

}

#endif /* __LIBMARY__MAP__H__ */

