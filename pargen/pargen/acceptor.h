/*  Pargen - Flexible parser generator
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


#ifndef PARGEN__ACCEPTOR__H__
#define PARGEN__ACCEPTOR__H__


#include <libmary/libmary.h>

#include <pargen/parser_element.h>


namespace Pargen {

using namespace M;

class Acceptor : public StReferenced
{
public:
    virtual void setParserElement (ParserElement *parser_element) = 0;
};

template <class T>
class ListAcceptor : public Acceptor
{
protected:
    List<T*> *target_list;

public:
    void setParserElement (ParserElement * const parser_element)
    {
	if (!parser_element)
	    return;

//	assert (dynamic_cast <T*> (parser_element));

	if (target_list)
	    target_list->append (static_cast <T*> (parser_element));
    }

    void init (List<T*> * const target_list)
    {
	this->target_list = target_list;
    }

    ListAcceptor (List<T*> * const target_list)
        : target_list (target_list)
    {
    }

    ListAcceptor ()
    {
    }
};

template <class T>
class PtrAcceptor : public Acceptor
{
protected:
    T **target_ptr;

public:
    void setParserElement (ParserElement * const parser_element)
    {
	if (!parser_element) {
	    if (target_ptr)
		*target_ptr = NULL;

	    return;
	}

	assert (!target_ptr || !*target_ptr);

	if (target_ptr)
	    *target_ptr = parser_element;
    }

    void init (T ** const target_ptr)
    {
	this->target_ptr = target_ptr;

	if (target_ptr)
	    *target_ptr = NULL;
    }

    PtrAcceptor (T ** const target_ptr)
        : target_ptr (target_ptr)
    {
	if (target_ptr)
	    *target_ptr = NULL;
    }

    PtrAcceptor ()
    {
    }
};

template <class T>
class RefAcceptor : public Acceptor
{
protected:
    StRef<T> *target_ref;

public:
    void setParserElement (ParserElement * const parser_element)
    {
	if (!parser_element) {
	    if (target_ref)
		*target_ref = NULL;

	    return;
	}

        assert (!target_ref || !*target_ref);

	if (target_ref)
	    *target_ref = parser_element;
    }

    void init (StRef<T> * const target_ref)
    {
	this->target_ref = target_ref;

	if (target_ref)
	    *target_ref = NULL;
    }

    RefAcceptor (StRef<T> * const target_ref)
        : target_ref (target_ref)
    {
	if (target_ref)
	    *target_ref = NULL;
    }

    RefAcceptor ()
    {
    }
};

}


#endif /* PARGEN__ACCEPTOR__H__ */

