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


#ifndef PARGEN__PARSER_ELEMENT__H__
#define PARGEN__PARSER_ELEMENT__H__


#include <libmary/libmary.h>


#define VSLAB_ACCEPTOR


namespace Pargen {

using namespace M;

class ParserElement
#ifndef VSLAB_ACCEPTOR
    : public StReferenced
#endif
{
public:
    void *user_obj;

    ParserElement ()
	: user_obj (NULL)
    {
    }
};

class ParserElement_Token : public ParserElement //,
//			    public virtual SimplyReferenced
{
public:
    ConstMemory token;

    ParserElement_Token (ConstMemory   const token,
			 void        * const user_obj)
	: token (token)
    {
	this->user_obj = user_obj;
    }
};

}


#endif /* PARGEN__PARSER_ELEMENT__H__ */

