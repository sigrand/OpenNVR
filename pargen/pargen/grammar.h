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


#ifndef PARGEN__GRAMMAR__H__
#define PARGEN__GRAMMAR__H__


#include <libmary/libmary.h>

#include <pargen/parser_element.h>
#include <pargen/acceptor.h>


namespace Pargen {

using namespace M;

class Parser;
class ParserControl;

/*c
 * Internal representation for grammars.
 *
 * <c>Grammar</c> objects are opaque for pargen users.
 */
class Grammar : public StReferenced
{
public:
    typedef void (*BeginFunc) (void *data);

    // TODO AcceptFunc and MatchFunc should be merged into
    // callbacks identical to InlineMatchFunc.
    // There'll be only one type of callback - "MatchFunc".
    typedef void (*AcceptFunc) (ParserElement *parser_element,
				ParserControl *parser_control,
				void          *data);

    typedef bool (*MatchFunc) (ParserElement *parser_element,
			       ParserControl *parser_control,
			       void          *data);

    typedef bool (*InlineMatchFunc) (ParserElement *parser_element,
				     ParserControl *parser_control,
				     void          *data);

    typedef bool (*JumpFunc) (ParserElement *parser_element,
			      void          *data);

    enum Type {
	t_Immediate,
	t_Compound,
	t_Switch,
	t_Alias
    };

    const Type grammar_type;

    BeginFunc begin_func;

    // Called after the grammar was found to be matching
    // according to the syntax. Returns true if the element is
    // found to be a mathcing one according to lookup data etc.
    MatchFunc match_func;

    // Called after the grammar was found to be the matching one,
    // and chosen as the preferred one among the alternatives.
    AcceptFunc accept_func;

    // Used for detecting infinite grammar loops when optimizing.
    Size loop_id;

    Bool optimized;

    // Returns string representation of the grammar for debugging output.
    virtual StRef<String> toString () = 0;

    Grammar (Type type)
	: grammar_type (type)
    {
	begin_func = NULL;
	match_func = NULL;
	accept_func = NULL;

	loop_id = 0;
    }
};

// FIXME Grammar_Immediate_SingleToken should become Grammar_Immediate.
//       Currently, this is assumed when we do optimizations.
class Grammar_Immediate : public Grammar
{
public:
    // Do not confuse this with match_func().
    // match_func() is supposed to be provided by the user of the grammar.
    // match() is a description of the grammar type, it's an internal
    // pargen mechanism.
    virtual bool match (ConstMemory  token,
			void        *token_user_ptr,
			void        *user_data) = 0;

    Grammar_Immediate ()
	: Grammar (Grammar::t_Immediate)
    {
    }
};

class Grammar_Immediate_SingleToken : public Grammar_Immediate
{
public:
    typedef bool (*TokenMatchCallback) (ConstMemory const &token,
					void        *token_user_ptr,
					void        *user_data);

    TokenMatchCallback token_match_cb;
    // For debug dumps.
    StRef<String> token_match_cb_name;

protected:
    // If null, then any token matches.
    StRef<String> token;

public:
    StRef<String> getToken ()
    {
	return token;
    }

    StRef<String> toString ();

    bool match (ConstMemory   const t,
		void        * const token_user_ptr,
		void        * const user_data)
    {
	if (token_match_cb)
	    return token_match_cb (t, token_user_ptr, user_data);

	if (!token || token->len() == 0)
	    return true;

	return equal (token->mem(), t);
    }

    // If token is NULL, then any token matches.
    Grammar_Immediate_SingleToken (char const * const token)
	: token_match_cb (NULL),
          token (st_grab (new String (token)))
    {
    }
};

class TranzitionMatchEntry : public StReferenced
{
public:
    Grammar_Immediate_SingleToken::TokenMatchCallback token_match_cb;

    TranzitionMatchEntry ()
	: token_match_cb (NULL)
    {
    }
};

class SwitchGrammarEntry : public StReferenced
{
public:
    enum Flags {
	Dominating = 0x1 // Currently unused
    };

    StRef<Grammar> grammar;
    Uint32 flags;

    List< StRef<String> > variants;

    class TranzitionEntry : public StReferenced,
			    public M::HashEntry<>
    {
    public:
	StRef<String> grammar_name;
    };

    typedef M::Hash< TranzitionEntry,
		     Memory,
		     MemberExtractor< TranzitionEntry,
				      StRef<String>,
				      &TranzitionEntry::grammar_name,
				      Memory,
				      AccessorExtractor< String,
							 Memory,
							 &String::mem > >,
		     MemoryComparator<> >
	    TranzitionEntryHash;

    TranzitionEntryHash tranzition_entries;

    // TODO Use Map<>
    List< StRef<TranzitionMatchEntry> > tranzition_match_entries;
    bool any_tranzition;

    SwitchGrammarEntry ()
	: flags (0),
	  any_tranzition (false)
    {
    }

    ~SwitchGrammarEntry ();
};

class CompoundGrammarEntry : public StReferenced
{
public:
    typedef void (*AssignmentFunc) (ParserElement *compound_element,
				    ParserElement *subel);

    class Acceptor : public Pargen::Acceptor
    {
    protected:
	AssignmentFunc assignment_func;
	ParserElement *compound_element;

    public:
	void setParserElement (ParserElement * const parser_element)
	{
	    if (assignment_func != NULL)
		assignment_func (compound_element, parser_element);
	}

	void init (AssignmentFunc   const assignment_func,
		   ParserElement  * const compound_element /* non-null */)
	{
	    this->assignment_func = assignment_func;
	    this->compound_element = compound_element;
	}

	Acceptor (AssignmentFunc   const assignment_func,
		  ParserElement  * const mt_nonnull compound_element)
            : assignment_func  (assignment_func),
              compound_element (compound_element)
	{
	    assert (compound_element);
	}

	Acceptor ()
	{
	}
    };

    enum Flags {
	Optional = 0x1,
	Sequence = 0x2
    };

  // TODO CompoundGrammarEntry_Jump,
  //      CompoundGrammarEntry_InlineMatch,
  //      CompoundGrammarEntry_Grammar

    // If 'is_jump' is non-null, then all non-jump fields should be considered
    // invalid.
    Bool is_jump;
    // Switch or compound
    Grammar *jump_grammar;
    Grammar::JumpFunc jump_cb;

    Size jump_switch_grammar_index;
    List< StRef<SwitchGrammarEntry> >::Element *jump_switch_grammar_entry;

    Size jump_compound_grammar_index;
    List< StRef<CompoundGrammarEntry> >::Element *jump_compound_grammar_entry;

    // If inline_match_func is non-null, then all other fields
    // should be considered invalid.
    Grammar::InlineMatchFunc inline_match_func;

    StRef<Grammar> grammar;
    Uint32 flags;

    AssignmentFunc assignment_func;

    static VSlab<Acceptor> acceptor_slab;

    VSlabRef<Acceptor> createAcceptorFor (ParserElement *compound_element)
    {
	// TODO If assignment_func is NULL, then there's probably no need
	// in allocating the acceptor at all.
	VSlabRef<Acceptor> acceptor = VSlabRef<Acceptor>::forRef <Acceptor> (acceptor_slab.alloc ());
	acceptor->init (assignment_func, compound_element);
	return acceptor;
    }

    CompoundGrammarEntry ()
	: jump_grammar (NULL),
	  jump_cb (NULL),
	  jump_switch_grammar_index (0),
	  jump_switch_grammar_entry (NULL),
	  jump_compound_grammar_index (0),
	  jump_compound_grammar_entry (NULL),
	  inline_match_func (NULL),
	  flags (0),
	  assignment_func (NULL)
    {
    }
};

class Grammar_Compound : public Grammar
{
public:
    typedef ParserElement* (*ElementCreationFunc) (VStack * const vstack /* non-null */);

    StRef<String> name;

    ElementCreationFunc elem_creation_func;
    List< StRef<CompoundGrammarEntry> > grammar_entries;

    StRef<String> toString ();

    List< StRef<CompoundGrammarEntry> >::Element* getSecondSubgrammarElement ()
    {
	Size i = 0;
	List< StRef<CompoundGrammarEntry> >::Iterator ge_iter (grammar_entries);
	while (!ge_iter.done ()) {
	    List< StRef<CompoundGrammarEntry> >::Element &ge_el = ge_iter.next ();
	    if (ge_el.data->inline_match_func == NULL) {
		i ++;
		if (i > 1)
		    return &ge_el;
	    }
	}

	return NULL;
    }

    CompoundGrammarEntry* getFirstSubgrammarEntry ()
    {
	List< StRef<CompoundGrammarEntry> >::DataIterator ge_iter (grammar_entries);
	while (!ge_iter.done ()) {
	    StRef<CompoundGrammarEntry> &ge = ge_iter.next ();
	    if (ge->inline_match_func == NULL)
		return ge;
	}

	return NULL;
    }

    Grammar* getFirstSubgrammar ()
    {
	CompoundGrammarEntry * const ge = getFirstSubgrammarEntry ();
	if (ge == NULL)
	    return NULL;

	return ge->grammar;
    }

    ParserElement* createParserElement (VStack * const vstack /* non-null */)
    {
	return elem_creation_func (vstack);
    }

    Grammar_Compound (ElementCreationFunc elem_creation_func)
	: Grammar (Grammar::t_Compound)
    {
	this->elem_creation_func = elem_creation_func;
    }
};

class Grammar_Switch : public Grammar
{
public:
    StRef<String> name;

    List< StRef<SwitchGrammarEntry> > grammar_entries;

    StRef<String> toString ();

    Grammar_Switch ()
	: Grammar (Grammar::t_Switch)
    {
    }
};

class Grammar_Alias : public Grammar
{
public:
    StRef<String> name;

    StRef<Grammar> aliased_grammar;

    StRef<String> toString ()
    {
	// TODO
	return st_grab (new String ("--- Alias ---"));
    }

    Grammar_Alias ()
	: Grammar (Grammar::t_Alias)
    {
    }
};

}


#include <pargen/parser.h>


#endif /* PARGEN__GRAMMAR__H__ */

