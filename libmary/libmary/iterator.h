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


// MyCpp-style iterators.


#ifndef __LIBMARY__ITERATOR__H__
#define __LIBMARY__ITERATOR__H__


#include <libmary/types.h>
#include <libmary/pointer.h>
#include <libmary/extractor.h>
#include <libmary/util_base.h>


namespace M {

// Stateful iterators must support direct copying for saving
// iterator's state.
template <class T>
class StatefulIterator_anybase
{
public:
    virtual T next () = 0;
    virtual bool done () = 0;

    virtual ~StatefulIterator_anybase () {}
};

template < class T,
	   class Base = EmptyBase >
class StatefulIterator : public StatefulIterator_anybase<T>,
			 public Base
{
};

// Resettable iterator
template <class T>
class IteratorBase
{
private:
    IteratorBase& operator = (IteratorBase &);
    IteratorBase (IteratorBase &);

public:
    virtual T next () = 0;
    virtual bool done () = 0;
    virtual void reset () = 0;

    IteratorBase () {}

    virtual ~IteratorBase () {}
};

// Generic iterators shouldn't support direct copying.
template < class T,
	   class Base = EmptyBase >
class Iterator : public IteratorBase<T>,
		 public Base
{
};

// This wrapper allows for representing stateful iterators
// as resettable iterators.
template < class T,
	   class StatefulIterator,
	   class Base = EmptyBase >
class IteratorWrapper : public Iterator<T, Base>
{
protected:
    StatefulIterator initial_state;
    StatefulIterator current_state;

public:
    T next ()
    {
	return current_state.next();
    }

    bool done ()
    {
	return current_state.done ();
    }

    void reset ()
    {
	current_state = initial_state;
    }

    IteratorWrapper (StatefulIterator const &iter)
	: initial_state (iter),
	  current_state (iter)
    {
    }
};

template < class T,                 // Iterator's return type
	   class Extractor,         // The extractor used for values returned by StatefulIterator
	   class StatefulIterator,  // The underlying iterator
	   class Base = EmptyBase >
class StatefulExtractorIterator : public M::StatefulIterator<T, Base>
{
protected:
    StatefulIterator iter;

public:
    T next ()
    {
	return Extractor::getValue (iter.next ());
    }

    bool done ()
    {
	return iter.done ();
    }

    StatefulExtractorIterator (StatefulIterator const &iter)
	: iter (iter)
    {
    }
};

template < class T,
	   class Extractor,
	   class Iterator,
	   class Base = EmptyBase >
class ExtractorIterator : public M::Iterator<T, Base>
{
protected:
    Iterator &iter;

public:
    T next ()
    {
	return Extractor::getValue (iter.next ());
    }

    bool done ()
    {
	return iter.done ();
    }

    void reset ()
    {
	iter.reset ();
    }

    ExtractorIterator (Iterator &iter)
	: iter (iter)
    {
    }
};

template < class T,
	   class Extractor,
	   class Iterator,
	   class Base = EmptyBase >
class RefExtractorIterator : public M::Iterator<T, Base>
{
protected:
    Ref<Iterator> iter;

public:
    T next ()
    {
	return Extractor::getValue (iter.next ());
    }

    bool done ()
    {
	return iter.done ();
    }

    void reset ()
    {
	iter.reset ();
    }

    RefExtractorIterator (Iterator *iter)
	: iter (iter)
    {
	assert (iter);
    }
};

template < class T,
	   class Base = EmptyBase >
class StatefulSingleItemIterator : public StatefulIterator<T&, Base>
{
protected:
    Pointer<T> item;
    bool iterated;

public:
    T& next ()
    {
	if (iterated)
	    unreachable ();

	iterated = true;
	return *item;
    }

    bool done ()
    {
	return iterated;
    }

    StatefulSingleItemIterator (Pointer<T> const &item)
	: item (item)
    {
	iterated = false;
    }
};

template < class T,
	   class Base = EmptyBase >
class SingleItemIterator : public IteratorWrapper< T&,
						   StatefulSingleItemIterator<T>,
						   Base >
{
public:
    SingleItemIterator (Pointer<T> const &item)
	: IteratorWrapper< T&,
			   StatefulSingleItemIterator<T>,
			   Base > (
		  StatefulSingleItemIterator<T> (item))
    {
    }
};

template < class T,
	   class Base = EmptyBase >
class IterArrayIterator : public Iterator<T, Base>
{
protected:
    IteratorBase<T> **iter_arr;
    unsigned long num_iters;

    unsigned long cur_iter;

public:
    T next ()
    {
	if (cur_iter >= num_iters)
	    unreachable ();

	while (iter_arr [cur_iter] == NULL ||
	       iter_arr [cur_iter]->done ())
	{
	    cur_iter ++;
	    if (cur_iter >= num_iters)
		unreachable ();
	}

	return iter_arr [cur_iter]->next ();
    }

    bool done ()
    {
	if (cur_iter >= num_iters)
	    return true;

	while (iter_arr [cur_iter] == NULL ||
	       iter_arr [cur_iter]->done ())
	{
	    cur_iter ++;
	    if (cur_iter >= num_iters)
		return true;
	}

	return false;
    }

    void reset ()
    {
	for (unsigned long i = 0; i < num_iters; i++) {
	    if (iter_arr [i] != NULL)
		iter_arr [i]->reset ();
	}

	cur_iter = 0;
    }

    IterArrayIterator (IteratorBase<T> **iter_arr,
		       unsigned long num_iters)
	: iter_arr (iter_arr),
	  num_iters (num_iters)
    {
	cur_iter = 0;
    }
};

template < class T,
	   class X = T&,
	   class Extractor = DirectExtractor<T&>,
	   class Base = EmptyBase >
class ArrayIterator : public Iterator<X, Base>
{
protected:
    T * const array;
    const unsigned long num_elements;

    unsigned long cur_element;

public:
    X next ()
    {
	if (cur_element >= num_elements)
	    unreachable ();

	T& ret = array [cur_element];
	cur_element ++;

	return Extractor::getValue (ret);
    }

    bool done ()
    {
	if (cur_element >= num_elements)
	    return true;

	return false;
    }

    void reset ()
    {
	cur_element = 0;
    }

    ArrayIterator (T *array,
		   unsigned long num_elements)
	: array (array),
	  num_elements (num_elements)
    {
	if (array == NULL && num_elements > 0)
	    unreachable ();

	cur_element = 0;
    }
};

}


#endif /* __LIBMARY__ITERATOR__H__ */

