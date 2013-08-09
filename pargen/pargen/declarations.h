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


#ifndef PARGEN__DECLARATIONS__H__
#define PARGEN__DECLARATIONS__H__


#include <libmary/libmary.h>


namespace Pargen {

using namespace M;

// Declaration

class Declaration : public StReferenced
{
public:
    enum Type {
	t_Phrases,
	t_Callbacks
    };

    const Type declaration_type;

    StRef<String> declaration_name;
    StRef<String> lowercase_declaration_name;

    Declaration (Type type)
	: declaration_type (type)
    {
    }
};

// Declaration
//     Declaration_Callbacks
//         CallbackDecl

class CallbackDecl : public StReferenced
{
public:
    StRef<String> callback_name;
};

// Declaration
//      Declaration_Phrases

class Phrase;

class Declaration_Phrases : public Declaration
{
public:
    class PhraseRecord : public StReferenced
    {
    public:
	StRef<Phrase> phrase;
	List< StRef<String> > variant_names;
    };

    Bool is_alias;
    StRef<String> aliased_name;
    StRef<String> deep_aliased_name;
    Declaration_Phrases *aliased_decl;

    List< StRef<PhraseRecord> > phrases;

    // Used for detecting infinite grammar loops when linking upwards anchors.
    Size loop_id;
    Size decl_loop_id;

    Map< StRef<CallbackDecl>,
	 MemberExtractor< CallbackDecl,
			  StRef<String>,
			  &CallbackDecl::callback_name,
			  Memory,
			  AccessorExtractor< String,
					     Memory,
					     &String::mem > >,
	 MemoryComparator<> >
	    callbacks;

    Declaration_Phrases ()
	: Declaration (Declaration::t_Phrases),
	  aliased_decl (NULL),
	  loop_id (0),
	  decl_loop_id (0)
    {
    }
};

// Declaration
//     Declaration_Phrases
//         Phrase

class PhrasePart;

class Phrase : public StReferenced
{
public:
    StRef<String> phrase_name;
    List< StRef<PhrasePart> > phrase_parts;
};

// Declaration
//     Declaration_Phrases
//         Phrase
//             PhrasePart

class PhrasePart : public StReferenced
{
public:
    enum Type {
	t_Phrase,
	t_Token,
	t_AcceptCb,
	t_UniversalAcceptCb,
	t_UpwardsAnchor,
	t_Label
    };

    const Type phrase_part_type;

    Bool seq;
    Bool opt;

    StRef<String> name;
    Bool name_is_explicit;

    StRef<String> toString ();

    PhrasePart (Type type)
	: phrase_part_type (type)
    {
    }
};

// Declaration
//     Declaration_Phrases
//         Phrase
//             PhrasePart
//                 PhrasePart_Phrase

class PhrasePart_Phrase : public PhrasePart
{
public:
    StRef<String> phrase_name;
    StRef<Declaration_Phrases> decl_phrases;

    PhrasePart_Phrase ()
	: PhrasePart (PhrasePart::t_Phrase)
    {
    }
};

// Declaration
//     Declaration_Phrases
//         Phrase
//             PhrasePart
//                 PhrasePart_Token

class PhrasePart_Token : public PhrasePart
{
public:
    // If null, then any token matches.
    StRef<String> token;
    StRef<String> token_match_cb;

    PhrasePart_Token ()
	: PhrasePart (PhrasePart::t_Token)
    {
    }
};

// Declaration
//     Declaration_Phrases
//         Phrase
//             PhrasePart
//                 PhrasePart_AcceptCb

class PhrasePart_AcceptCb : public PhrasePart
{
public:
    StRef<String> cb_name;
    Bool repetition;

    PhrasePart_AcceptCb ()
	: PhrasePart (PhrasePart::t_AcceptCb)
    {
    }
};

// Declaration
//     Declaration_Phrases
//         Phrase
//             PhrasePart
//                 PhrasePart_UniversalAcceptCb

class PhrasePart_UniversalAcceptCb : public PhrasePart
{
public:
    StRef<String> cb_name;
    Bool repetition;

    PhrasePart_UniversalAcceptCb ()
	: PhrasePart (PhrasePart::t_UniversalAcceptCb)
    {
    }
};

// Declaration
//     Declaration_Phrases
//         Phrase
//             PhrasePart
//                 PhrasePart_UpwardsAnchor

class PhrasePart_UpwardsAnchor : public PhrasePart
{
public:
    StRef<String> declaration_name;
    StRef<String> phrase_name;
    StRef<String> label_name;
    StRef<String> jump_cb_name;

    Size switch_grammar_index;
    Size compound_grammar_index;

    PhrasePart_UpwardsAnchor ()
	: PhrasePart (PhrasePart::t_UpwardsAnchor),
	  switch_grammar_index (0),
	  compound_grammar_index (0)
    {
    }
};

// Declaration
//     Declaration_Phrases
//         Phrase
//             PhrasePart
//                 PhrasePart_UpwardsAnchor

class PhrasePart_Label : public PhrasePart
{
public:
    StRef<String> label_name;

    PhrasePart_Label ()
	: PhrasePart (PhrasePart::t_Label)
    {
    }
};

// Declaration
//     Declaration_Callbacks

class Declaration_Callbacks : public Declaration
{
public:
    List< StRef<CallbackDecl> > callbacks;

    Declaration_Callbacks ()
	: Declaration (Declaration::t_Callbacks)
    {
    }
};

}


#endif /* PARGEN__DECLARATIONS__H__ */

