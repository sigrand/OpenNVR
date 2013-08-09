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


#ifndef __LIBMARY__LIST__H__
#define __LIBMARY__LIST__H__


#include <libmary/types.h>
#include <libmary/extractor.h>
#include <libmary/iterator.h>


namespace M {

class GenericList
{
public:
    enum StealType
    {
	StealPrepend = 0,
	StealAppend
    };
};

/*c Bidirectional linked list. */
template <class T>
class List
{
private:
    List& operator = (List const &list);
    List (List const &list);

protected:
    unsigned long numElements;

public:
    /*c A node of a <t>List</t>. */
    class Element
    {
    public:
	Element *next;		/*< Pointer to the next element
				 * in the list, or NULL. */
	Element *previous;	/*< Pointer to the previous element
				 * in the list, or NULL. */

	T data;			//< User's data associated with this element.

	Element ()
	{
	}

	Element (T const &data)
	    : data (data)
	{
	}
    };

    template <class Base = EmptyBase>
    class Iterator_ : public StatefulIterator<Element&, Base>
    {
    protected:
	Element *el;

    public:
	Element& next ()
	{
	    Element *ret = el;
	    assert (el != NULL);

	    el = el->next;
	    return *ret;
	}

	bool done ()
	{
	    return el == NULL;
	}

	Iterator_ (List const &list)
	    : el (list.first)
	{
	}

	Iterator_ (Element * const el)
	    : el (el)
	{
	}
    };
    typedef Iterator_<> Iterator;

    template <class Base = EmptyBase>
    class InverseIterator_ : public StatefulIterator<Element&, Base>
    {
    protected:
	Element *el;

    public:
	// [10.08.27] I think it's wrong to return a reference.
	//            Element should be made a transparent object
	//            instead, like Map::Entry.
	Element& next ()
	{
	    Element *ret = el;
	    assert (el != NULL);

	    el = el->previous;
	    return *ret;
	}

	bool done ()
	{
	    return el == NULL;
	}

	InverseIterator_ (List const &list)
	    : el (list.last)
	{
	}

	InverseIterator_ (Element * const el)
	    : el (el)
	{
	}
    };
    typedef InverseIterator_<> InverseIterator;

    #ifdef MyCpp_List_DataIterator_base
    #error redefinition
    #endif
    #define MyCpp_List_DataIterator_base					\
	    StatefulExtractorIterator < T&,					\
					MemberExtractor < Element,		\
							  T,			\
							  &Element::data >,	\
					Iterator,				\
					Base >

    template <class Base = EmptyBase>
    class DataIterator_
	: public MyCpp_List_DataIterator_base
    {
    public:
	DataIterator_ (List const &list)
	    : MyCpp_List_DataIterator_base (Iterator (list))
	{
	}

	DataIterator_ (Element * const el)
	    : MyCpp_List_DataIterator_base (el)
	{
	}
    };
    typedef DataIterator_<> DataIterator;

    #undef MyCpp_List_DataIterator_base

    #ifdef MyCpp_List_InverseDataIterator_base
    #error redefinition
    #endif
    #define MyCpp_List_InverseDataIterator_base					\
	    StatefulExtractorIterator < T&,					\
					MemberExtractor < Element,		\
							  T,			\
							  &Element::data >,	\
					InverseIterator,			\
					Base >

    template <class Base = EmptyBase>
    class InverseDataIterator_
	: public MyCpp_List_InverseDataIterator_base
    {
    public:
	InverseDataIterator_ (List const &list)
	    : MyCpp_List_InverseDataIterator_base (InverseIterator (list))
	{
	}

	InverseDataIterator_ (Element * const el)
	    : MyCpp_List_InverseDataIterator_base (el)
	{
	}
    };
    typedef InverseDataIterator_<> InverseDataIterator;

    #undef MyCpp_List_InverseDataIterator_base

    template <class Base>
    Iterator_<Base> createIterator () const
    {
	return Iterator_<Base> (*this);
    }

    template <class Base>
    DataIterator_<Base> createDataIterator () const
    {
	return DataIterator_<Base> (*this);
    }

    // TODO These fields should be made private in favor of 'get' accessors.
    //
    /*> Pointer to the first element of the list, or NULL. */
    Element *first;
    /*> Pointer to the last element of the list, or NULL. */
    Element *last;

    T& getFirst () const
    {
	return first->data;
    }

    T& getLast () const
    {
	return last->data;
    }

    Element* getFirstElement () const
    {
	return first;
    }

    Element* getLastElement () const
    {
	return last;
    }

    Element* getIthElement (Size const index)
    {
	assert (index < numElements);

	Element *el = first;
	for (Size i = 0; i < index; i++) {
	    el = el->next;
	}

	return el;
    }

    /*m Returns the number of elements currently in the list. */
    unsigned long getNumElements () const
    {
	return numElements;
    }

    bool isEmpty () const
    {
	return first == NULL;
    }

    /*m Appends an element to the list.
     *
     * After the completion of this method a new element
     * will be inserted right after the specified element.
     *
     * A default constructor must exist for type <t>T</t>.
     *
     * @element The element after which the new element is to be inserted. */
    Element* appendEmpty (Element *element)
    {
	Element *nl = new Element;

	nl->previous = element;

	if (element == NULL) {
	    nl->next = NULL;
	    first = nl;
	    last  = nl;
	} else {
	    nl->next = element->next;
	    if (element->next != NULL)
		element->next->previous = nl;

	    element->next = nl;
	    if (element == last)
		last = nl;
	}

	numElements ++;
	return nl;
    }

    Element* appendEmpty ()
    {
	return appendEmpty (last);
    }

    /*m Appends an element to the specified element of list, initializing
     * the element's <t>data</t> field with a specified value.
     *
     * The element's value will be set using the copy constructor
     * for type <t>T</t>.
     *
     * @data The value to be assigned to the <t>data</t> field
     * of the newly created element.
     * @element The element after which the new element is to be inserted. */
    Element* append (const T &data, Element *element)
    {
	Element *nl = new Element (data);

	nl->previous = element;

	if (element == NULL) {
	    nl->next = NULL;
	    first = nl;
	    last  = nl;
	} else {
	    nl->next = element->next;
	    if (element->next != NULL)
		element->next->previous = nl;
	    element->next = nl;
	    if (element == last)
		last = nl;
	}

	numElements ++;
	return nl;
    }

    Element* append (const T &data)
    {
	return append (data, last);
    }

    Element* prependEmpty (Element *element)
    {
	Element *nl = new Element;

	nl->next = element;

	if (element == NULL) {
	    nl->previous = NULL;
	    first = nl;
	    last  = nl;
	} else {
	    nl->previous = element->previous;
	    if (element->previous != NULL)
		element->previous->next = nl;

	    element->previous = nl;
	    if (element == first)
		first = nl;
	}

	numElements ++;
	return nl;
    }

    Element* prependEmpty ()
    {
	return prependEmpty (first);
    }

    /*m Prepends an element to the specified element of the list, initializing
     * the element's <t>data</t> field with a specified value.
     *
     * The element's value will be set using the copy constructor
     * for type <t>T</t>.
     *
     * @data The value to be assigned to the <t>data</t> field
     * of the newly created element.
     * @element The element before which the new element is to be inserted. */
    Element* prepend (const T &data, Element *element)
    {
	Element *nl = new Element (data);

	nl->next = element;

	if (element == NULL) {
	    nl->previous = NULL;
	    first = nl;
	    last  = nl;
	} else {
	    nl->previous = element->previous;
	    if (element->previous != NULL)
		element->previous->next = nl;

	    element->previous = nl;
	    if (element == first)
		first = nl;
	}

	numElements ++;
	return nl;
    }

    Element* prepend (const T &data)
    {
	return prepend (data, first);
    }

    /*m Removes an element from the list.
     *
     * @element The element to be removed from the list. */
    void remove (Element *element)
    {
	if (element == first)
	    first = element->next;
	else
	    element->previous->next = element->next;

	if (element == last)
	    last = element->previous;
	else
	    element->next->previous = element->previous;

	numElements --;

	delete element;
    }

    /*m Clears the list, i.e. removes all elements from it. */
    void clear ()
    {
	Element *cur = first;
	Element *tmp;

	while (cur != NULL) {
	    tmp = cur;
	    cur = cur->next;
	    delete tmp;
	}

	first = NULL;
	last  = NULL;

	numElements = 0;
    }

    /*m*/
    void steal (List<T> *list,
		Element *from,
		Element *to,
		Element *element,
		GenericList::StealType stealType)
    {
	if (list == NULL ||
	    from == NULL ||
	    to   == NULL)
	{
	    return;
	}

	{
	  // NOTE: We have to traverse the list because of the need
	  // to maintain 'numElements'. This doesn't look smart.

	    unsigned long n = 1;
	    Element *cur = from;
	    while (cur != to) {
		n ++;
		cur = cur->next;
	    }

	    list->numElements -= n;
	    numElements += n;
	}

	assert (list != NULL);

	if (from == list->first)
	    list->first = to->next;

	if (to == list->last)
	    list->last = from->previous;

	if (from->previous != NULL)
	    from->previous->next = to->next;

	if (to->next != NULL)
	    to->next->previous = from->previous;

	if (element != NULL) {
	    if (stealType == GenericList::StealPrepend) {
		if (element->previous != NULL)
		    element->previous->next = from;

		from->previous = element->previous;

		element->previous = to;
		to->next = element;

		if (element == first)
		    first = from;
	    } else
	    if (stealType == GenericList::StealAppend) {
		if (element->next != NULL)
		    element->next->previous = to;

		to->next = element->next;

		element->next = from;
		from->previous = element;

		if (element == last)
		    last = to;
	    } else {
		abort ();
	    }
	} else {
	    assert (!(first != NULL || last != NULL));

	    from->previous = NULL;
	    to->next = NULL;
	    first = from;
	    last = to;
	}
    }

    List ()
    {
	first = NULL;
	last = NULL;

	numElements = 0;
    }

    ~List ()
    {
	clear ();
    }


  // ________________________________ iterator _________________________________

    class iterator
    {
    private:
        Element *cur;

    public:
        iterator (List &list) : cur (list.getFirstElement()) {}
        iterator () {}

        bool operator == (iterator const &iter) const { return cur == iter.cur; }
        bool operator != (iterator const &iter) const { return cur != iter.cur; }

        bool done () const { return cur == NULL; }
        Element* next () { Element * const tmp = cur; cur = cur->next; return tmp; }
    };


// _________________________________ Iterator __________________________________

    class iter
    {
	friend class List;

    private:
	Element *cur;

	iter (Element * const el) : cur (el) {}

    public:
	iter () {}
	iter (List &list) { list.iter_begin (*this); }

	bool operator == (iter const &iter) const { return cur == iter.cur; }
	bool operator != (iter const &iter) const { return cur != iter.cur; }

 	// Methods for C API binding.
	void *getAsVoidPtr () const { return static_cast <void*> (cur); }
	static iter fromVoidPtr (void *ptr) {
		return iter (static_cast <Element*> (ptr)); }
    };

    void iter_begin (iter &iter) const
    {
	iter.cur = getFirstElement();
    }

    static Element* iter_next (iter &iter)
    {
	Element * const el = iter.cur;
	iter.cur = iter.cur->next;
	return el;
    }

    static bool iter_done (iter &iter)
    {
	return iter.cur == NULL;
    }

  // ___________________________ Reverse iterator ______________________________

    class rev_iter
    {
	friend class List;

    private:
	Element *cur;

	rev_iter (Element * const el) : cur (el) {}

    public:
	rev_iter () {}
	rev_iter (List &list) { list.rev_iter_begin (*this); }

	bool operator == (rev_iter const &iter) const
	{
	    return cur == iter.cur;
	}

	bool operator != (rev_iter const &iter) const
	{
	    return cur != iter.cur;
	}
    };

    void rev_iter_begin (rev_iter &iter) const
    {
	iter.cur = getLastElement();
    }

    static Element* rev_iter_next (rev_iter &iter)
    {
	Element * const el = iter.cur;
	iter.cur = iter.cur->previous;
	return el;
    }

    static bool rev_iter_done (rev_iter &iter)
    {
	return iter.cur == NULL;
    }
};

template <class T, class Base = EmptyBase>
class List_ : public List<T>,
	      public Base
{
};

}

#endif /*__LIBMARY__LIST_H__*/

