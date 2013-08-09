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


#include <pargen/parser.h>


/* [DMS] This is probably among the worst pieces of code I have ever written.
 * But since this is all about managed non-stack recursion, it is also
 * one of the most unusual pieces of code. Take it as it is :)
 */

// Parsing trace, should be always enabled.
// Enabled/disabled by parsing_state->debug_dump flag.
#define DEBUG_PAR(a) a

#define DEBUG(a) ;
// Flow
#define DEBUG_FLO(a) ;
// Internal
#define DEBUG_INT(a) ;
// Callbacks
#define DEBUG_CB(a) ;
// Optimization
#define DEBUG_OPT(a) ;
#define DEBUG_OPT2(a) ;
// Negative cache
#define DEBUG_NEGC(a) ;
#define DEBUG_NEGC2(a) ;
// VStack memory allocation
#define DEBUG_VSTACK(a) ;

#define FUNC_NAME(a) ;


// Enables forward optimization (single-token lookahead)
#define PARGEN_FORWARD_OPTIMIZATION

// Enables negative cache lookups
#define PARGEN_NEGATIVE_CACHE

// Enables upwards jumps
#define PARGEN_UPWARDS_JUMPS


using namespace M;

namespace Pargen {

StRef<ParserConfig>
createParserConfig (bool const upwards_jumps)
{
    StRef<ParserConfig> const parser_config = st_grab (new (std::nothrow) ParserConfig);
    parser_config->upwards_jumps = upwards_jumps;
    return parser_config;
}

StRef<ParserConfig>
createDefaultParserConfig ()
{
    return createParserConfig (true /* upwards_jumps */);
}

namespace {
class ParsingState;
}

static mt_throws Result pop_step (ParsingState * mt_nonnull parsing_state,
                                  bool          match,
                                  bool          empty_match,
                                  bool          negative_cache_update = true);

namespace {
class ParsingStep : public StReferenced,
                    public IntrusiveListElement<>
{
public:
    typedef void (*AssignmentFunc) (ParserElement *parser_element,
                                    void          *user_data);

    enum Type {
        t_Sequence,
        t_Compound,
        t_Switch,
        t_Alias
    };

    const Type parsing_step_type;

    Grammar *grammar;

#ifndef VSLAB_ACCEPTOR
    StRef<Acceptor> acceptor;
#else
    // TODO Why not use steps vstack to hold the acceptor?
    VSlabRef<Acceptor> acceptor;
#endif
    Bool optional;

    // Initialized in push_step()
    TokenStream::PositionMarker token_stream_pos;

    Size go_right_count;

    VStack::Level vstack_level;
    VStack::Level el_level;

    ParsingStep (Type type)
        : parsing_step_type (type),
          grammar (NULL),
          go_right_count (0)
    {
    }

    ~ParsingStep ()
    {
      DEBUG_INT (
        errs->println (_func_);
      )
    }
};

typedef IntrusiveList<ParsingStep> ParsingStepList;

class ParsingStep_Sequence : public ParsingStep
{
public:
    // FIXME Memory leak + inefficient: use intrusive list.
    List<ParserElement*> parser_elements;

    ParsingStep_Sequence ()
        : ParsingStep (ParsingStep::t_Sequence)
    {
    }
};

class ParsingStep_Compound : public ParsingStep
{
public:
    List< StRef<CompoundGrammarEntry> >::Element *cur_subg_el;

    Bool got_jump;
    Grammar *jump_grammar;
    Grammar::JumpFunc jump_cb;
    List< StRef<SwitchGrammarEntry> >::Element *jump_switch_grammar_entry;
    List< StRef<CompoundGrammarEntry> >::Element *jump_compound_grammar_entry;

    Bool jump_performed;

    // We should be able to detect left-recursive grammars for "a: b_opt a c"
    // cases when b_opt doesn't match, hence this hint.
    Grammar *lr_parent;

    ParserElement *parser_element;

    Bool got_nonoptional_match;

    ParsingStep_Compound ()
        : ParsingStep (ParsingStep::t_Compound),
          jump_grammar (NULL),
          jump_cb (NULL),
          jump_switch_grammar_entry (NULL),
          jump_compound_grammar_entry (NULL),
          lr_parent (NULL),
          parser_element (NULL)
    {
    }
};

class ParsingStep_Switch : public ParsingStep
{
public:
    enum State {
        // Parsing non-left-recursive grammars
        State_NLR,
        // Parsing left-recursive grammars
        State_LR
    };

    State state;

    Bool got_empty_nlr_match;
    Bool got_nonempty_nlr_match;
    Bool got_lr_match;

    List< StRef<SwitchGrammarEntry> >::Element *cur_nlr_el;
    List< StRef<SwitchGrammarEntry> >::Element *cur_lr_el;

#ifdef VSLAB_ACCEPTOR
    ParserElement *nlr_parser_element;
    ParserElement *parser_element;
#else
    StRef<ParserElement> nlr_parser_element;
    StRef<ParserElement> parser_element;
#endif

    ParsingStep_Switch ()
        : ParsingStep (ParsingStep::t_Switch),
          nlr_parser_element (NULL),
          parser_element (NULL)
    {
    }
};

class ParsingStep_Alias : public ParsingStep
{
public:
    ParserElement *parser_element;

    ParsingStep_Alias ()
        : ParsingStep (ParsingStep::t_Alias),
          parser_element (NULL)
    {
    }
};

// Positive cache is an n-ary tree with chains of matching phrases.
// Nodes of the tree refer to grammars.
//
// Consider the following grammar:
//
//     a: b c
//     b: [b]
//     c: d e
//     d: [d]
//     e: [e]
//     f: [f]
//
// And input: "b d e f".
//
// Here's the state of the cache after parsing this input:
//
//        +--->a------------------------->f
//        |                              /^
//        |      +----->c---------------/ |
//        |      |                        |
// (root)-+--->b-+----->d------->e--------+
//
// Tokens:     o--------o--------o--------o
//             b        d        e        f
//
// Positive cache is cleaned at cache cleanup points. Such points are
// specified explicitly in the grammar. They are usually points of
// no return, after wich match failures mean syntax errors in input.
//
class PositiveCacheEntry : public StReferenced
{
public:
    Bool match;
    Grammar *grammar;

    PositiveCacheEntry *parent_entry;

    typedef Map< StRef<PositiveCacheEntry>,
                 MemberExtractor< PositiveCacheEntry const,
                                  Grammar* const,
                                  &PositiveCacheEntry::grammar,
                                  UintPtr,
                                  CastExtractor< Grammar*,
                                                 UintPtr > >,
                 DirectComparator<UintPtr> >
#if 0
                                  UidType,
                                  AccessorExtractor< UidProvider const,
                                                     UidType,
                                                     &Grammar::getUid > >,
                 DirectComparator<UidType> >
#endif
            PositiveMap;

    PositiveMap positive_map;

    PositiveCacheEntry ()
        : grammar (NULL),
          parent_entry (NULL)
    {
    }
};

// TODO Having a similar superclass for positive cache would be nice.
class NegativeCache
{
private:
    class GrammarEntry : public IntrusiveAvlTree_Node<>
    {
    public:
        Grammar *grammar;
    };

    class NegEntry : //public SimplyReferenced
                     public IntrusiveListElement<>
    {
    public:
        typedef IntrusiveAvlTree< GrammarEntry,
                                  MemberExtractor< GrammarEntry,
                                                   Grammar*,
                                                   &GrammarEntry::grammar,
                                                   UintPtr,
                                                   CastExtractor< Grammar*,
                                                                  UintPtr > >,
                                  DirectComparator<UintPtr> >
                GrammarEntryTree;

        GrammarEntryTree grammar_entries;
    };

    // Note: There's no crucial reason to do this.
    typedef IntrusiveList<NegEntry> NegEntryList;

    VStack neg_vstack;
    VSlab<GrammarEntry> grammar_slab;

    NegEntryList neg_cache;
    NegEntry *cur_neg_entry;

    // For debugging
    size_t pos_index;

public:
    void goRight ()
    {
      FUNC_NAME (
        static char const * const _func_name = "Pargen.NegativeCache.goRight";
      )

        if (cur_neg_entry == NULL ||
            cur_neg_entry == neg_cache.getLast())
        {
            NegEntry * const neg_entry =
                    new (neg_vstack.push_malign (sizeof (NegEntry), alignof (NegEntry))) NegEntry;
            neg_cache.append (neg_entry);
            cur_neg_entry = neg_cache.getLast();
        } else {
            cur_neg_entry = neg_cache.getNext (cur_neg_entry);
        }

        ++ pos_index;
        DEBUG_NEGC2 (
            errs->println (_func, "pos_index ", pos_index);
        )
    }

    void goLeft ()
    {
        if (cur_neg_entry == NULL ||
            cur_neg_entry == neg_cache.getFirst())
        {
            NegEntry * const neg_entry =
                    new (neg_vstack.push_malign (sizeof (NegEntry), alignof (NegEntry))) NegEntry;
            neg_cache.append (neg_entry, cur_neg_entry /* to_el */);
            cur_neg_entry = neg_entry;
        } else {
            cur_neg_entry = neg_cache.getPrevious (cur_neg_entry);
        }

        -- pos_index;
        DEBUG_NEGC2 (
            errs->println (_func, "pos_index ", pos_index);
        )
    }

    void addNegative (Grammar * const mt_nonnull grammar)
    {
        assert (cur_neg_entry);

        DEBUG_NEGC2 (
            errs->println (_func, "pos_index ", pos_index);
        )

        VSlab<GrammarEntry>::AllocKey slab_key;
        GrammarEntry * const grammar_entry = grammar_slab.alloc (&slab_key);
        grammar_entry->grammar = grammar;
        if (!cur_neg_entry->grammar_entries.addUnique (grammar_entry)) {
          // New element inserted.

//		grammar_entry->slab_key = slab_key;
        } else {
          // Element with the same value already exists.

            DEBUG_NEGC (
                errs->println (_func, "duplicate entry");
            )

            grammar_slab.free (slab_key);
        }
    }

    bool isNegative (Grammar * const grammar)
    {
        assert (cur_neg_entry);

        DEBUG_NEGC2 (
            errs->println (_func, "pos_index ", pos_index);
        )

        return cur_neg_entry->grammar_entries.lookup ((UintPtr) grammar);
    }

    void cut ()
    {
        NegEntry *neg_entry = neg_cache.getFirst();
        while (neg_entry) {
            NegEntry * const next_neg_entry = neg_cache.getNext (neg_entry);

            if (neg_entry == cur_neg_entry)
                break;

            neg_cache.remove (neg_entry);

            // TODO FIXME Forgot to cut neg_vstack?

            // TODO vstack->vqueue, cut queue tail

            neg_entry = next_neg_entry;
        }
    }

    NegativeCache ()
        : neg_vstack (1 << 16),
          cur_neg_entry (NULL),
          pos_index (0)
    {
    }
};

class VStackContainer : public StReferenced
{
public:
    VStack vstack;

    VStackContainer (Size const block_size)
        : vstack (block_size)
    {
    }
};

// State of the parser.
class ParsingState : public ParserControl
{
public:
    enum Direction {
        Up,
        Down
    };

    class PositionMarker : public ParserPositionMarker
    {
    public:
        TokenStream::PositionMarker token_stream_pos;
        ParsingStep_Compound *compound_step;
        List< StRef<CompoundGrammarEntry> >::Element *cur_subg_el;
        Bool got_nonoptional_match;
        Size go_right_count;

        PositionMarker ()
            : compound_step (NULL)
        {
        }
    };

    StRef<ParserConfig> parser_config;

    ConstMemory default_variant;

    VSlab< ListAcceptor<ParserElement> > list_acceptor_slab;
    VSlab< PtrAcceptor<ParserElement> > ptr_acceptor_slab;

    Bool create_elements;

    StRef<String> variant;

    // Nest level is used for debugging output.
    Size nest_level;

    TokenStream *token_stream;
    LookupData  *lookup_data;
    // User data for accept_func() and match_func().
    void *user_data;

    StRef<VStackContainer> el_vstack_container;
    VStack *el_vstack;

    // Stack of grammar invocations.
    ParsingStepList step_list;
    VStack step_vstack;

    // Direction of the previous movement along the stack:
    // "Up" means we've become one level deeper ('steps' grew),
    // "Down" means we've returned from a nested level ('steps' shrunk).
    // Note: it looks like I've swapped the meanings for "up" and "down" here.
    Direction cur_direction;

    // 'true' if we've come from a nested grammar with a match for that grammar,
    // 'false' otherwise.
    Bool match;
    // 'true' if we've come from a nested grammar with an empty match
    // for that grammar.
    Bool empty_match;

    // 'true' if we're up from a grammar which turns out
    // to be left recursive after attempting to parse it.
    // ("a: b_opt a c", and "b" is no match).
    Bool compound_lr;

    // 'true' if we're up from a compound grammar which is an empty match.
    // This is possible if all subgrammars are optional or if we preset
    // the first element of a left-recursive grammar and all of the following
    // elements are optional.
    Bool compound_empty;

    Bool debug_dump;

    PositiveCacheEntry  positive_cache_root;
    PositiveCacheEntry *cur_positive_cache_entry;

    NegativeCache negative_cache;

    Bool position_changed;

    ParsingStep& getLastStep ()
    {
        assert (!step_list.isEmpty());
        return *step_list.getLast();
    }

  mt_iface (ParserControl)

    void setCreateElements (bool const create_elements)
    {
        this->create_elements = create_elements;
    }

    StRef<ParserPositionMarker> getPosition ();

    mt_throws Result setPosition (ParserPositionMarker *pmark);

    void setVariant (ConstMemory const variant)
    {
        this->variant = st_grab (new (std::nothrow) String (variant));
    }

  mt_iface_end

    ParsingState ()
        : step_vstack (1 << 16 /* block_size */)
    {
        el_vstack_container = st_grab (new (std::nothrow) VStackContainer (1 << 16 /* block_size */));
        assert (el_vstack_container);
        el_vstack = &el_vstack_container->vstack;

        DEBUG_VSTACK (
            errs->println (_func,
                           "list_acceptor_slab vstack: 0x", fmt_hex, (UintPtr) &list_acceptor_slab.vstack, ", "
                           "ptr_acceptor_slab vstack: 0x", fmt_hex, (UintPtr) &ptr_acceptor_slab.vstack);
            errs->println (_func,
                           "el_vstack: 0x", fmt_hex, (UintPtr) el_vstack, ", "
                           "step_vstack: 0x", fmt_hex, (UintPtr) &step_vstack);
            errs->println (_func, "CompoundGrammarEntry::acceptor_slab vstack: "
                           "0x", fmt_hex, (UintPtr) &CompoundGrammarEntry::acceptor_slab.vstack);
        )
    }
};

StRef<ParserPositionMarker>
ParsingState::getPosition ()
{
    ParsingStep &parsing_step = getLastStep ();
    assert (parsing_step.parsing_step_type == ParsingStep::t_Compound);
    ParsingStep_Compound * const compound_step = static_cast <ParsingStep_Compound*> (&parsing_step);

    StRef<PositionMarker> const pmark = st_grab (new (std::nothrow) PositionMarker);
    token_stream->getPosition (&pmark->token_stream_pos);
    pmark->compound_step = compound_step;
    pmark->got_nonoptional_match = compound_step->got_nonoptional_match;
    pmark->go_right_count = parsing_step.go_right_count;

    {
#if 0
// TODO Зачем это было написано (нееверный блок)?
        List< StRef<CompoundGrammarEntry> >::Element *next_subg_el = compound_step->cur_subg_el;
        while (next_subg_el != NULL &&
               // FIXME Спорно. Скорее всего, нужно пропускать
               //       только первую match-функцию - ту, которая вызвана
               //       в данный момент.
               next_subg_el->data->inline_match_func != NULL)
        {
            next_subg_el = next_subg_el->next;
        }

        pmark->cur_subg_el = next_subg_el;
#endif

        pmark->cur_subg_el = compound_step->cur_subg_el;
    }

    return pmark;
}

mt_throws Result
ParsingState::setPosition (ParserPositionMarker * const _pmark)
{
    PositionMarker * const pmark = static_cast <PositionMarker*> (_pmark);

    position_changed = true;

    Size total_go_right = 0;
    {
        ParsingStep * const mark_step = static_cast <ParsingStep*> (pmark->compound_step);
        for (;;) {
            ParsingStep * const cur_step = step_list.getLast();
            if (cur_step == mark_step)
                break;

            total_go_right += cur_step->go_right_count;

            if (!pop_step (this, false /* match */, false /* empty_match */, false /* negative_cache_update */))
                return Result::Failure;
        }
    }

    ParsingStep_Compound * const compound_step =
            static_cast <ParsingStep_Compound*> (&getLastStep ());

    compound_step->cur_subg_el = pmark->cur_subg_el;
    compound_step->got_nonoptional_match = pmark->got_nonoptional_match;

    cur_direction = ParsingState::Up;

    total_go_right += compound_step->go_right_count;
    assert (total_go_right >= pmark->go_right_count);
    for (Size i = 0; i < total_go_right - pmark->go_right_count; i++) {
        DEBUG_NEGC (
            errs->println (_func, "go left");
        )
        negative_cache.goLeft ();
    }

    return token_stream->setPosition (&pmark->token_stream_pos);
}
} // namespace {}

static void
print_whsp (OutputStream * const mt_nonnull outs,
	    Size           const num_spaces)
{
    for (Size i = 0; i < num_spaces; i++)
	outs->print (" ");
}

static void
print_tab (OutputStream * const mt_nonnull outs,
	   Size           const nest_level)
{
    print_whsp (outs, nest_level * 1);
}

static void
push_step (ParsingState * const mt_nonnull parsing_state,
	   ParsingStep  * const mt_nonnull step,
	   Bool           const new_checkpoint = true)
{
    DEBUG_INT (
      errs->println ("Pargen.push_step");
    )

    assert (parsing_state && step);

    parsing_state->token_stream->getPosition (&step->token_stream_pos);

    parsing_state->nest_level ++;

    {
        #warning What's' this?
	// TEST FIXME TEMPORAL
	step->ref ();

	parsing_state->step_list.append (step);
	step->ref ();

#if 0
        errs->println ("--- step_list appended: "
                       "f 0x", fmt_hex, (UintPtr) parsing_state->step_list.getFirst(),
                       ", l 0x", (UintPtr) parsing_state->step_list.getLast(),
                       ", fe 0x", fmt_hex, (UintPtr) parsing_state->step_list.first,
                       ", le 0x", (UintPtr) parsing_state->step_list.last);
#endif
    }

    parsing_state->cur_direction = ParsingState::Up;

    if (new_checkpoint) {
	if (parsing_state->lookup_data)
	    parsing_state->lookup_data->newCheckpoint ();
    }

    DEBUG_PAR (
	if (parsing_state->debug_dump) {
	    errs->print (">");
	    print_tab (errs, parsing_state->nest_level);

	    ParsingStep &_step = parsing_state->getLastStep ();
	    switch (_step.parsing_step_type) {
		case ParsingStep::t_Sequence: {
		    ParsingStep_Sequence &step = static_cast <ParsingStep_Sequence&> (_step);
		    errs->print ("(seq) ", step.grammar->toString ());
		} break;
		case ParsingStep::t_Compound: {
		    ParsingStep_Compound &step = static_cast <ParsingStep_Compound&> (_step);
		    errs->print ("(com) ", step.grammar->toString ());
		} break;
		case ParsingStep::t_Switch: {
		    ParsingStep_Switch &step = static_cast <ParsingStep_Switch&> (_step);
		    errs->print ("(swi) ", step.grammar->toString ());
		} break;
		case ParsingStep::t_Alias: {
		    errs->print ("(alias)");
		}
	    }

	    errs->println (" >");
	}
    )
}

static mt_throws Result
pop_step (ParsingState *parsing_state,
	  bool match,
	  bool empty_match,
	  bool negative_cache_update)
{
    assert (!(empty_match && !match));

    DEBUG_INT (
	errs->println (_func, "match: ", match, ", "
                       "empty_match: ", empty_match);
    );

    DEBUG_PAR (
	if (parsing_state->debug_dump) {
	    if (match)
		errs->print (ConstMemory ("+"));
	    else
		errs->print (ConstMemory (" "));

	    print_tab (errs, parsing_state->nest_level);

	    if (match)
		errs->print ("MATCH ");

	    ParsingStep &_step = parsing_state->getLastStep ();
	    switch (_step.parsing_step_type) {
		case ParsingStep::t_Sequence: {
		    ParsingStep_Sequence &step = static_cast <ParsingStep_Sequence&> (_step);
		    errs->print ("(seq) ", step.grammar->toString ());
		} break;
		case ParsingStep::t_Compound: {
		    ParsingStep_Compound &step = static_cast <ParsingStep_Compound&> (_step);
		    errs->print ("(com) ", step.grammar->toString ());
		} break;
		case ParsingStep::t_Switch: {
		    ParsingStep_Switch &step = static_cast <ParsingStep_Switch&> (_step);
		    errs->print ("(swi) ", step.grammar->toString ());
		} break;
		case ParsingStep::t_Alias: {
		    errs->print ("(alias)");
		} break;
	    }

	    if (match)
		errs->print (" +");
	    else
		errs->print (" <");

            errs->println (ConstMemory (""));
	}
    );

    assert (parsing_state);

    assert (!parsing_state->step_list.isEmpty());
    ParsingStep &step = *parsing_state->step_list.getLast();

    if (!match || empty_match) {
	if (!parsing_state->token_stream->setPosition (&step.token_stream_pos))
            return Result::Failure;
    }

    if (negative_cache_update) {
	if (!match || empty_match) {
	    for (Size i = 0; i < step.go_right_count; i++) {
		DEBUG_NEGC (
                  errs->println (_func, "go left");
		)
		parsing_state->negative_cache.goLeft ();
	    }
	} else {
	    if (parsing_state->step_list.getFirst() != parsing_state->step_list.getLast()) {
		ParsingStep &prv_step = *(parsing_state->step_list.getPrevious (parsing_state->step_list.getLast()));
		prv_step.go_right_count += step.go_right_count;
	    }
	}

	if (!match /* TEST && !empty_match */) {
	    DEBUG_NEGC (
              errs->println (_func, "adding negative ", step.grammar->toString ());
	    )
	    parsing_state->negative_cache.addNegative (step.grammar);
	}
    }

    parsing_state->match = match;
    parsing_state->empty_match = empty_match;

    parsing_state->nest_level --;
    {
	ParsingStep * const tmp_step = parsing_state->step_list.getLast();
	VStack::Level const tmp_level = tmp_step->vstack_level;
	VStack::Level const tmp_el_level = tmp_step->el_level;

	parsing_state->step_list.remove (tmp_step);
	tmp_step->~ParsingStep ();

#if 0
        errs->println ("--- step_list removed: "
                       "f 0x", fmt_hex, (UintPtr) parsing_state->step_list.getFirst(),
                       ", l 0x", (UintPtr) parsing_state->step_list.getLast(),
                       ", fe 0x", fmt_hex, (UintPtr) parsing_state->step_list.first,
                       ", le 0x", (UintPtr) parsing_state->step_list.last);
#endif

	parsing_state->step_vstack.setLevel (tmp_level);

	if (!match)
	    parsing_state->el_vstack->setLevel (tmp_el_level);
    }

    parsing_state->cur_direction = ParsingState::Down;

    if (match) {
	if (parsing_state->lookup_data)
	    parsing_state->lookup_data->commitCheckpoint ();
    } else {
	if (parsing_state->lookup_data)
	    parsing_state->lookup_data->cancelCheckpoint ();
    }

    return Result::Success;
}

// Returns 'true' (@ret_res) if we have a match, 'false otherwise.
static mt_throws Result
parse_Immediate (ParsingState      * const mt_nonnull parsing_state,
		 Grammar_Immediate * const mt_nonnull grammar,
		 Acceptor          * const acceptor,
                 bool              * const mt_nonnull ret_res)
{
    DEBUG_FLO (
      errs->println (_func_);
    );

    assert (parsing_state);

    TokenStream::PositionMarker pmark;
    parsing_state->token_stream->getPosition (&pmark);

    StRef<StReferenced> user_obj;
    void *user_ptr;
    ConstMemory token;
    if (!parsing_state->token_stream->getNextToken (&token, &user_obj, &user_ptr))
        return Result::Failure;
    if (token.len() == 0) {
	if (!parsing_state->token_stream->setPosition (&pmark))
            return Result::Failure;

	DEBUG (
          errs->println (_func, "no token");
	)
        *ret_res = false;
        return Result::Success;
    }

    DEBUG_PAR (
      if (parsing_state->debug_dump)
          errs->println (_func, "token: ", token);
    )

    DEBUG_INT (
      errs->println (_func, "token: ", token);
    )

    if (!grammar->match (token, user_ptr, parsing_state->user_data)) {
	if (!parsing_state->token_stream->setPosition (&pmark))
            return Result::Failure;

	DEBUG_INT (
          errs->println (_func, "!grammar->match()");
	)

        *ret_res = false;
        return Result::Success;
    }

    // Note: This is a strange condition...
    if (acceptor) {
	Byte * const el_token_buf = parsing_state->el_vstack->push_unaligned (token.len());
	memcpy (el_token_buf, token.mem(), token.len());
	ParserElement * const parser_element =
		new (parsing_state->el_vstack->push_malign (
                                    sizeof (ParserElement_Token), alignof (ParserElement_Token)))
                            ParserElement_Token (
                                    ConstMemory (el_token_buf, token.len()),
                                    user_ptr);

	// TODO I think that match_func() and accept_func() should be called
	// regardless of whether 'acceptor' is NULL or not.
	if (grammar->match_func != NULL) {
	    DEBUG_CB (
              errs->println (_func, "calling match_func()");
	    )
	    if (!grammar->match_func (parser_element, parsing_state, parsing_state->user_data)) {
		if (!parsing_state->token_stream->setPosition (&pmark))
                    return Result::Failure;

		DEBUG_INT (
                  errs->println (_func, "match_func() returned false");
		)
                *ret_res = false;
                return Result::Success;
	    }

	    DEBUG_INT (
              errs->println (_func, "match_func() returned true");
	    )
	} else {
	    DEBUG_INT (
              errs->println (_func, "no match_func()");
	    )
	}

	if (grammar->accept_func != NULL) {
	    DEBUG_CB (
              errs->println (_func, "calling accept_func()");
	    )
	    grammar->accept_func (parser_element,
				  parsing_state,
				  parsing_state->user_data);
	}

	acceptor->setParserElement (parser_element);
    }

    *ret_res = true;
    return Result::Success;
}

enum ParsingResult {
    ParseUp,
    ParseNonemptyMatch,
    ParseEmptyMatch,
    ParseNoMatch
};

static void
push_compound_step (ParsingState     * const parsing_state,
		    Grammar_Compound * const grammar,
#ifndef VSLAB_ACCEPTOR
		    Acceptor         * const acceptor,
#else
		    VSlabRef<Acceptor> const acceptor,
#endif
		    bool               const optional,
		    bool               const got_nonoptional_match = false,
		    Size               const go_right_count = 0,
		    bool               const got_cur_subg_el = false,
		    List< StRef<CompoundGrammarEntry> >::Element * const cur_subg_el = NULL)
{
    VStack::Level const tmp_vstack_level = parsing_state->step_vstack.getLevel ();
    VStack::Level const tmp_el_level = parsing_state->el_vstack->getLevel ();
    ParsingStep_Compound * const step =
	    new (parsing_state->step_vstack.push_malign (
                                sizeof (ParsingStep_Compound), alignof (ParsingStep_Compound)))
                        ParsingStep_Compound;
    step->vstack_level = tmp_vstack_level;
    step->el_level = tmp_el_level;
    step->acceptor = acceptor;
    step->optional = optional;
    step->grammar = grammar;
    step->got_nonoptional_match = got_nonoptional_match;
    step->go_right_count = go_right_count;

    if (got_cur_subg_el)
	step->cur_subg_el = cur_subg_el;
    else
	step->cur_subg_el = grammar->grammar_entries.first;

    step->parser_element = grammar->createParserElement (parsing_state->el_vstack);
    push_step (parsing_state, step);
}

static void
push_switch_step (ParsingState       * const parsing_state,
		  Grammar_Switch     * const grammar,
#ifndef VSLAB_ACCEPTOR
		  Acceptor           * const acceptor,
#else
		  VSlabRef<Acceptor>   const acceptor,
#endif
		  bool                 const optional,
		  List< StRef<SwitchGrammarEntry> >::Element * const cur_subg_el = NULL)
{
    VStack::Level const tmp_vstack_level = parsing_state->step_vstack.getLevel ();
    VStack::Level const tmp_el_level = parsing_state->el_vstack->getLevel ();
    ParsingStep_Switch * const step =
	    new (parsing_state->step_vstack.push_malign (
                                sizeof (ParsingStep_Switch), alignof (ParsingStep_Switch)))
                        ParsingStep_Switch;
    step->vstack_level = tmp_vstack_level;
    step->el_level = tmp_el_level;
    step->acceptor = acceptor;
    step->optional = optional;
    step->grammar = grammar;
    step->state = ParsingStep_Switch::State_NLR;
    step->got_empty_nlr_match = false;
    step->got_nonempty_nlr_match = false;
    step->got_lr_match = false;

    if (cur_subg_el != NULL)
	step->cur_nlr_el = cur_subg_el;
    else
	step->cur_nlr_el = grammar->grammar_entries.first;

    step->cur_lr_el = NULL;

    push_step (parsing_state, step);
}

static mt_throws Result
parse_grammar (ParsingState       * const mt_nonnull parsing_state,
	       Grammar            * const mt_nonnull _grammar,
// VSLAB ACCEPTOR	       Acceptor     *acceptor,
	       VSlabRef<Acceptor>   const acceptor,
	       bool                 const optional,
               ParsingResult      * const mt_nonnull ret_res)
{
    DEBUG_FLO (
      errs->println (_func_);
    );

    assert (parsing_state && _grammar);

#ifdef PARGEN_NEGATIVE_CACHE
    if (parsing_state->negative_cache.isNegative (_grammar)) {
      // TODO At this point, we have already checked that the grammar is
      // in negative cache for the current token. But we'll test for this
      // again in pop_step(), which could be avoided if we remembered the result
      // of the lookup that we've just made.

	DEBUG_NEGC (
          errs->println (_func, "negative ", _grammar->toString ());
	)

	if (optional) {
#if 0
	  // FIXME: This looks strange. Why don't we expect this to be called
	  // in parse_Immediate()? This calls seems to be excessive.
	    if (_grammar->accept_func != NULL) {
		_grammar->accept_func (NULL,
				       parsing_state,
				       parsing_state->user_data);
	    }
#endif

	    *ret_res = ParseEmptyMatch;
            return Result::Success;
	}

	*ret_res = ParseNoMatch;
        return Result::Success;
    }
#endif

    switch (_grammar->grammar_type) {
	case Grammar::t_Immediate: {
	    DEBUG_INT (
	      errs->println (_func, "Grammar::_Immediate");
	    )

            bool match;
	    if (!parse_Immediate (parsing_state,
                                  static_cast <Grammar_Immediate*> (_grammar),
                                  acceptor,
                                  &match))
            {
                return Result::Failure;
            }

	    if (match) {
		{
		  // Updating negative cache state (moving right)

		    DEBUG_NEGC (
                      errs->println (_func, "go right");
		    )
		    parsing_state->negative_cache.goRight ();

		    if (!parsing_state->step_list.isEmpty()) {
			ParsingStep * const step = parsing_state->step_list.getLast();
			step->go_right_count ++;
		    }
		}

		*ret_res = ParseNonemptyMatch;
                return Result::Success;
	    }

	    if (optional) {
	      // FIXME: This looks strange. Why don't we expect this to be called
	      // in parse_Immediate()? This call seems to be excessive.

		assert (!match);
		if (_grammar->accept_func != NULL) {
		    _grammar->accept_func (NULL,
					   parsing_state,
					   parsing_state->user_data);
		}

		*ret_res = ParseEmptyMatch;
                return Result::Success;
	    }

	    *ret_res = ParseNoMatch;
            return Result::Success;
	} break;

	case Grammar::t_Compound: {
	    DEBUG_INT (
	      errs->println (_func, "Grammar::_Compound");
	    )

	    Grammar_Compound * const grammar = static_cast <Grammar_Compound*> (_grammar);

	    push_compound_step (parsing_state,
				grammar,
				acceptor,
				optional,
				false /* got_nonoptional_match */,
				0     /* go_right_count */,
				false /* got_cur_subg_el */,
				NULL  /* cur_subg_el */);
	} break;

	case Grammar::t_Switch: {
	    DEBUG_INT (
	      errs->println (_func, "Grammar::_Switch");
	    )

	    Grammar_Switch * const grammar = static_cast <Grammar_Switch*> (_grammar);

	    push_switch_step (parsing_state, grammar, acceptor, optional, NULL /* cur_subg_el */);
	} break;

	case Grammar::t_Alias: {
	    DEBUG_INT (
		errs->print (_func, "Grammar::_Alias");
	    )

	    Grammar_Alias * const grammar = static_cast <Grammar_Alias*> (_grammar);

	    VStack::Level const tmp_vstack_level = parsing_state->step_vstack.getLevel ();
	    VStack::Level const tmp_el_level = parsing_state->el_vstack->getLevel ();

	    ParsingStep_Alias * const step =
		    new (parsing_state->step_vstack.push_malign (
                                        sizeof (ParsingStep_Alias), alignof (ParsingStep_Alias)))
                                ParsingStep_Alias;
	    step->vstack_level = tmp_vstack_level;
	    step->el_level = tmp_el_level;
	    step->acceptor = acceptor;
	    step->optional = optional;
	    step->grammar = grammar;

	    push_step (parsing_state, step);
	} break;

	default:
            unreachable ();
    }

    *ret_res = ParseUp;
    return Result::Success;
}

static mt_throws Result
parse_sequence_no_match (ParsingState         * const mt_nonnull parsing_state,
			 ParsingStep_Sequence * const mt_nonnull step)
{
    DEBUG_FLO (
      errs->println (_func_);
    )

    assert (parsing_state && step);

    if (!step->parser_elements.isEmpty ()) {
	List<ParserElement*>::DataIterator parser_el_iter (step->parser_elements);
	while (!parser_el_iter.done ()) {
	    DEBUG_INT (
	      errs->println (_func, "accepting element");
	    )

	    if (step->acceptor)
		step->acceptor->setParserElement (parser_el_iter.next ());
	}

	DEBUG (
          errs->println (_func, "non-empty list");
	)
	if (!pop_step (parsing_state, true /* match */, false /* empty_match */))
            return Result::Failure;
    } else {
	if (step->optional) {
	    if (!pop_step (parsing_state, true /* match */, true /* empty_match */))
                return Result::Failure;
        } else {
            if (!pop_step (parsing_state, false /* match */, false /* empty_match */))
                return Result::Failure;
        }
    }

    return Result::Success;
}

static mt_throws Result
parse_sequence_match (ParsingState         * const mt_nonnull parsing_state,
		      ParsingStep_Sequence * const mt_nonnull step)
{
    assert (parsing_state && step);

    VSlabRef< ListAcceptor<ParserElement> > const acceptor =
	    VSlabRef< ListAcceptor<ParserElement> >::forRef < ListAcceptor<ParserElement> > (
		    parsing_state->list_acceptor_slab.alloc ());
    acceptor->init (&step->parser_elements);
    for (;;) {
	ParsingResult pres;
        if (!parse_grammar (parsing_state, step->grammar, acceptor, false /* optional */, &pres))
            return Result::Failure;

	if (pres == ParseNonemptyMatch)
	    continue;

	if (pres == ParseEmptyMatch ||
	    pres == ParseNoMatch)
	{
	    if (!parse_sequence_no_match (parsing_state, step))
                return Result::Failure;
	} else {
	    assert (pres == ParseUp);
        }

	break;
    }

    return Result::Success;
}

static mt_throws Result
parse_compound_no_match (ParsingState         * const mt_nonnull parsing_state,
			 ParsingStep_Compound * const mt_nonnull step)
{
    DEBUG_FLO (
      errs->println (_func_);
    )

    assert (parsing_state && step);

#ifdef PARGEN_UPWARDS_JUMPS
    if (parsing_state->parser_config->upwards_jumps &&
	!step->jump_performed                       &&
	step->got_jump)
    {
	do {
	    if (step->jump_cb != NULL) {
		if (!step->jump_cb (step->parser_element, parsing_state->user_data))
		    break;
	    }

	    SwitchGrammarEntry * const switch_ge = step->jump_switch_grammar_entry->data;
	    assert (switch_ge->grammar->grammar_type == Grammar::t_Compound);
	    Grammar_Compound * const grammar__compound = static_cast <Grammar_Compound*> (switch_ge->grammar.ptr ());

	    push_compound_step (parsing_state,
				grammar__compound,
#ifndef VSLAB_ACCEPTOR
				NULL /* acceptor */,
#else
				VSlabRef<Acceptor> () /* acceptor */,
#endif
				false /* optional */,
				step->got_nonoptional_match,
				step->go_right_count,
				true  /* got_cur_subg_el */,
				step->jump_compound_grammar_entry);

	    step->jump_performed = true;
	    step->go_right_count = 0;

	    DEBUG (
              errs->println ("--- JUMP --- 0x", fmt_hex, (Uint64) step->jump_compound_grammar_entry);
	    )

	    return Result::Success;
	} while (0);
    }
#endif

    if (step->optional) {
	if (step->grammar->accept_func != NULL) {
	    step->grammar->accept_func (NULL,
					parsing_state,
					parsing_state->user_data);
	}

	if (!pop_step (parsing_state, true /* mach */, true /* empty_match */))
            return Result::Failure;
    } else {
	if (!pop_step (parsing_state, false /* mach */, false /* empty_match */))
            return Result::Failure;
    }

    return Result::Success;
}

static mt_throws Result
parse_compound_match (ParsingState         * const mt_nonnull parsing_state,
		      ParsingStep_Compound * const mt_nonnull step,
		      bool                   const empty_match)
{
    assert (parsing_state && step);

    if (!empty_match)
	step->got_nonoptional_match = true;

    while (!step->jump_performed &&
	   step->cur_subg_el != NULL)
    {
	CompoundGrammarEntry &entry = *step->cur_subg_el->data;
	step->cur_subg_el = step->cur_subg_el->next;

	if (entry.is_jump) {
	    step->got_jump = true;
	    step->jump_grammar = entry.jump_grammar;
	    step->jump_cb = entry.jump_cb;

	    assert (entry.jump_grammar->grammar_type == Grammar::t_Switch);
	    Grammar_Switch * const grammar__switch = static_cast <Grammar_Switch*> (entry.jump_grammar);

	    if (entry.jump_switch_grammar_entry == NULL) {
		entry.jump_switch_grammar_entry =
			grammar__switch->grammar_entries.getIthElement (entry.jump_switch_grammar_index);
	    }
	    step->jump_switch_grammar_entry = entry.jump_switch_grammar_entry;

	    SwitchGrammarEntry * const switch_ge = entry.jump_switch_grammar_entry->data;
	    assert (switch_ge->grammar->grammar_type == Grammar::t_Compound);
	    Grammar_Compound * const grammar__compound = static_cast <Grammar_Compound*> (switch_ge->grammar.ptr ());

	    if (entry.jump_compound_grammar_entry == NULL &&
		grammar__compound->grammar_entries.getNumElements () != entry.jump_compound_grammar_index)
	    {
		entry.jump_compound_grammar_entry =
			grammar__compound->grammar_entries.getIthElement (entry.jump_compound_grammar_index);
	    }
	    step->jump_compound_grammar_entry = entry.jump_compound_grammar_entry;

	    continue;
	}

	if (entry.inline_match_func != NULL) {
	    DEBUG_CB (
              errs->println (_func, "calling intermediate accept_func()");
	    )
	    // Note: This is the only place where inline match functions are called.
// Deprecated	    entry.accept_func (step->parser_element, parsing_state, parsing_state->user_data);
	    if (!entry.inline_match_func (step->parser_element, parsing_state, parsing_state->user_data)) {
	      // Inline match has failed, so we have no match for the current
	      // compound grammar.
		return parse_compound_no_match (parsing_state, step);
	    }

	    continue;
	}

	if (entry.flags & CompoundGrammarEntry::Sequence) {
	    DEBUG_INT (
	      errs->println (_func, "sequence");
	    )

	    VStack::Level const tmp_vstack_level = parsing_state->step_vstack.getLevel ();
	    VStack::Level const tmp_el_level = parsing_state->el_vstack->getLevel ();
	    ParsingStep_Sequence * const new_step =
		    new (parsing_state->step_vstack.push_malign (
                                        sizeof (ParsingStep_Sequence), alignof (ParsingStep_Sequence)))
                                ParsingStep_Sequence;
	    new_step->vstack_level = tmp_vstack_level;
	    new_step->el_level = tmp_el_level;
	    new_step->acceptor = entry.createAcceptorFor (step->parser_element);
	    new_step->optional = entry.flags & CompoundGrammarEntry::Optional;
	    new_step->grammar = entry.grammar;

// TODO Do not create a new checkpoint for sequence steps, _and_ do not
// commit/cancel the checkpoint in pop_step() accordingly.
//	    push_step (parsing_state, new_step, false /* new_checkpoint */);
	    push_step (parsing_state, new_step);
	    return Result::Success;
	} else {
	    DEBUG_INT (
                errs->println (_func, "not a sequence");
	    )

	    {
                DEBUG_INT (
                  errs->println (_func, "creating acceptor");
                )
                VSlabRef<Acceptor> acceptor = entry.createAcceptorFor (step->parser_element);
                DEBUG_INT (
                  errs->println (_func, "0x", fmt_hex, (Uint64) (Acceptor*) acceptor);
                )
                ParsingResult pres;
                if (!parse_grammar (parsing_state,
                                    entry.grammar,
                                    acceptor,
                                    entry.flags & CompoundGrammarEntry::Optional,
                                    &pres))
                {
                    return Result::Failure;
                }

                if (pres == ParseNonemptyMatch) {
                    step->got_nonoptional_match = true;
                    continue;
                }

                if (pres == ParseEmptyMatch && (entry.flags & CompoundGrammarEntry::Optional))
                    continue;

                if (pres == ParseNoMatch)
                    return parse_compound_no_match (parsing_state, step);
                else
                    assert (pres == ParseUp);

                DEBUG_INT (
                  errs->println (_func, "leaving test scope");
                )
	    }
	    DEBUG_INT (
              errs->println (_func, "left test scope");
	    )

	    return Result::Success;
	}
    }

  // We have parsed all subgrammars.

    bool user_match = true;
// TODO FIXME (explain)
//    if (!empty_match) {
	if (!step->jump_performed &&
	    step->grammar->match_func != NULL)
	{
	    DEBUG_CB (
              errs->println (_func, "calling match_func()");
	    )
	    if (!step->grammar->match_func (step->parser_element, parsing_state, parsing_state->user_data))
		user_match = false;
	}
//    }

    if (user_match) {
	DEBUG_INT (
          errs->println (_func, "match_func() returned true");
	)

	if (!step->jump_performed) {
	    if (step->grammar->accept_func != NULL) {
		DEBUG_CB (
                  errs->println (_func, "calling accept_func()");
		)
		step->grammar->accept_func (step->parser_element,
					    parsing_state,
					    parsing_state->user_data);
	    }

// TODO FIXME (explain)
//	    if (!empty_match)
	    if (step->acceptor) {
		DEBUG (
                  errs->println (_func, "calling setParserElement(): "
                                 "0x", fmt_hex, (Uint64) step->parser_element);
		)
		step->acceptor->setParserElement (step->parser_element);
	    }
	}

	DEBUG (
          errs->println (_func, "user_match");
	)
	if (!pop_step (parsing_state, true /* mach */, !step->got_nonoptional_match /* empty_match */))
            return Result::Failure;
    } else {
	DEBUG_INT (
          errs->println (_func, "match_func() returned false");
	)

	if (!parse_compound_no_match (parsing_state, step))
            return Result::Failure;
    }

    return Result::Success;
}

static mt_throws Result
parse_switch_final_match (ParsingState       * const mt_nonnull parsing_state,
			  ParsingStep_Switch * const mt_nonnull step,
			  bool                 const empty_match)
{
// Wrong condition. We should set parser elements for empty matches as well.
// Otherwise, "key = ;" yields NULL 'value' field in mconfig.
//    if (!empty_match) {
	if (step->acceptor) {
	    DEBUG_INT (
              errs->println (_func, "calling setParserElement(): "
                             "0x", fmt_hex, (Uint64) step->parser_element);
	    )
	    step->acceptor->setParserElement (step->parser_element);
	}
//    }

    if (!pop_step (parsing_state, true /* match */, empty_match))
        return Result::Failure;

    return Result::Success;
}

static mt_throws Result
parse_switch_no_match_yet (ParsingState       * mt_nonnull parsing_state,
			   ParsingStep_Switch * mt_nonnull step);

static mt_throws Result
parse_switch_match (ParsingState       * const mt_nonnull parsing_state,
		    ParsingStep_Switch * const mt_nonnull step,
		    bool                 const match,
		    bool                 const empty_match)
{
    assert (parsing_state &&
	    step &&
	    !(empty_match && !match));

    DEBUG_INT (
      errs->println (_func, "step->parser_element: "
                     "0x", fmt_hex, (Uint64) step->parser_element, ", "
                     "step->nlr_parser_element: 0x", fmt_hex, (Uint64) step->nlr_parser_element);
    )

    switch (step->state) {
	case ParsingStep_Switch::State_NLR: {
	  // We've just parsed a non-left-recursive grammar.

	  // Note: ParserElement parsed is stored in 'step->nlr_parser_element',
	  // as prescribed in the acceptor that we set for NLR grammars.

	    if (empty_match) {
	      // We've got an empty match. For recursion handling, we want to see
	      // if there'll be any non-empty matches for the switch. Empty matches
	      // can't be used for parsing left-recursive grammars, so we move on
	      // to the next non-left-recursive subgrammar.

		step->got_empty_nlr_match = true;

		DEBUG_INT (
                    errs->println (_func, "(NLR, empty): "
                                   "calling parse_switch_no_match_yet()");
		)
		if (!parse_switch_no_match_yet (parsing_state, step))
                    return Result::Failure;
	    } else {
	      // We've got a non-empty match. Let's try parsing left-recursive grammars,
	      // if any, with this match at the left.

		if (match) {
		    if (step->grammar->accept_func != NULL) {
			DEBUG_CB (
                          errs->println (_func, "calling accept_func (NLR, non-empty)");
			)
			step->grammar->accept_func (step->nlr_parser_element,
						    parsing_state,
						    parsing_state->user_data);
		    }

		    step->got_nonempty_nlr_match = true;
		}

		step->state = ParsingStep_Switch::State_LR;
		step->cur_lr_el = static_cast <Grammar_Switch*> (step->grammar)->grammar_entries.first;

		DEBUG_INT (
                  errs->println (_func, "(NLR, non-empty): "
                                 "calling parse_switch_no_match_yet()");
		)
		if (!parse_switch_no_match_yet (parsing_state, step))
                    return Result::Failure;
	    }
	} break;
	case ParsingStep_Switch::State_LR: {
	  // We've been parsing left-recursive grammars.

	    if (empty_match) {
	      // The match is empty. This means that the recursive grammar
	      // has an empty tail, hence the match is not recursive at all.
	      // Moving on to the next left-recursive grammar.

		DEBUG_INT (
                  errs->println (_func, "(LR, empty): "
                                 "calling parse_switch_no_match_yet()");
		)
		if (!parse_switch_no_match_yet (parsing_state, step))
                    return Result::Failure;

		return Result::Success;;
	    }

	    // Note: when we're building a long recursive chain of elements,
	    // each intermediate state of the chain should be a match.
	    // It's not clear at this point if we'll have to call match_func()
	    // on each iteration.

	    // Here we're making a trick: pretending like we've just parsed
	    // a non-left-recursive grammar, so that we can move on with
	    // the recursion.

	    // Calling accept_func early.
	    if (step->grammar->accept_func != NULL) {
		DEBUG_CB (
                  errs->println (_func, "calling accept_func");
		)
		step->grammar->accept_func (step->parser_element,
					    parsing_state,
					    parsing_state->user_data);
	    }

	    step->got_lr_match = true;

	    step->nlr_parser_element = step->parser_element;
	    step->parser_element = NULL;
	    step->cur_lr_el = static_cast <Grammar_Switch*> (step->grammar)->grammar_entries.first;

	    if (!parse_switch_no_match_yet (parsing_state, step))
                return Result::Failure;

// TODO Figure out how to call match_func() properly for left-recursive grammars.
// The net result should be equivalent to what happens when dealing with othre
// type of recursion.
#if 0
	    if (step->grammar->match_func != NULL) {
		DEBUG_CB (
                  errs->println (_func, "calling match_func()");
		)
		if (!step->grammar->match_func (step->parser_element, parsing_state->user_data)) {
		    if (!parse_switch_no_match_yet (parsing_state, step))
                        return Result::Failure;

                    return Result::Success;
		}
	    }
#endif
	} break;
	default:
            unreachable ();
    }

    return Result::Success;
}

static bool
is_cur_variant (ParsingState       * const mt_nonnull parsing_state,
		SwitchGrammarEntry * const mt_nonnull entry)
{
    if (entry->variants.isEmpty ())
	return true;

    List< StRef<String> >::DataIterator variant_iter (entry->variants);
    while (!variant_iter.done ()) {
        StRef<String> &variant = variant_iter.next ();
	if (!parsing_state->variant ||
	    parsing_state->variant->len() == 0)
	{
	    if (equal (variant->mem(), parsing_state->default_variant))
		return true;
	} else {
	    if (equal (variant->mem(), parsing_state->variant->mem()))
		return true;
	}
    }

    return false;
}

// Upwards optimization.
static mt_throws Result
parse_switch_upwards_green_forward (ParsingState       * const mt_nonnull parsing_state,
				    SwitchGrammarEntry * const mt_nonnull switch_grammar_entry,
                                    bool               * const mt_nonnull ret_res)
{
#ifdef PARGEN_FORWARD_OPTIMIZATION
  // Note: this is a raw non-optimized version.
  //     * Uses linked list traversal instead of map/hash lookups;
  //     * The list of tranzition entries is not cleaned up.
  //       It contains overlapping entries (like "any token" at the end
  //       of the list).

    if (switch_grammar_entry->any_tranzition) {
        *ret_res = true;
        return Result::Success;
    }

    ConstMemory token;
    StRef<StReferenced> user_obj;
    void *user_ptr;
    {
	TokenStream::PositionMarker pmark;
	parsing_state->token_stream->getPosition (&pmark);
        {
            if (!parsing_state->token_stream->getNextToken (&token, &user_obj, &user_ptr))
                return Result::Failure;
        }
	if (!parsing_state->token_stream->setPosition (&pmark))
            return Result::Failure;
    }

    if (token.len() == 0) {
        *ret_res = false;
        return Result::Success;
    }

    {
	List< StRef<TranzitionMatchEntry> >::DataIterator iter (
		switch_grammar_entry->tranzition_match_entries);
	while (!iter.done ()) {
	    StRef<TranzitionMatchEntry> &tranzition_match_entry = iter.next ();

	    DEBUG_OPT2 (
              errs->println ("--- TOKEN MATCH CB");
	    )

	    if (tranzition_match_entry->token_match_cb (token,
							user_ptr,
							parsing_state->user_data))
	    {
                *ret_res = true;
                return Result::Success;
	    }
	}
    }

    DEBUG_OPT2 (
      errs->println ("--- FIND: ", token.mem());
    )
    if (switch_grammar_entry->tranzition_entries.lookup (token)) {
        *ret_res = true;
        return Result::Success;
    }

    *ret_res = false;
    return Result::Success;
#else
    *ret_res = true;
    return Result::Success;
#endif // PARGEN_FORWARD_OPTIMIZATION
}

static mt_throws Result
parse_switch_upwards_green (ParsingState       * const mt_nonnull parsing_state,
			    SwitchGrammarEntry * const mt_nonnull switch_grammar_entry,
                            bool               * const mt_nonnull ret_res)
{
  FUNC_NAME (
    char const * const _func_name = "Pargen.Parser.parse_switch_upwards_green";
  )

#ifdef PARGEN_NEGATIVE_CACHE
    if (parsing_state->negative_cache.isNegative (switch_grammar_entry->grammar)) {
	DEBUG_NEGC (
          errs->println (_func, "negative ", switch_grammar_entry->grammar->toString ());
	)

        *ret_res = false;
        return Result::Success;
    }
#endif

    if (switch_grammar_entry->grammar->optimized)
	return parse_switch_upwards_green_forward (parsing_state, switch_grammar_entry, ret_res);

    *ret_res = true;
    return Result::Success;
}

static mt_throws Result
parse_switch_no_match_yet (ParsingState       * const mt_nonnull parsing_state,
			   ParsingStep_Switch * const mt_nonnull step)
{
    DEBUG_FLO (
      errs->print (_func_);
    )

    assert (parsing_state && step);

    // Here we're going to deal with left recursion.
    //
    // The general case for left recursion is the following:
    //
    //     a:
    //         b_opt a c
    //
    // This grammar turns out to be left-recursive if b_opt evalutes
    // to an empty sequence. In this case, we get "a: a c", which can't
    // be processed by out left-to-right parser without special treatment.
    //
    // We're going to deal with this by delaying parsing of left-recursive
    // grammars until any non-left-recursive grammar evaluates
    // to a non-empty sequence.
    //
    // In general, any left-recursive grammar can potentially be parsed
    // only if it is a part of a switch-type parent with at least one
    // non-left-recursive child. Once we've got a match for such a child,
    // we can move forward with parsing the recursive ones, throwing away
    // recursive references at their left. We can do this by simulating
    // a match for the first element of the corresponding compound grammars.
    //
    // Left-recursive grammars are always a better match than non-left-recursive
    // ones.
    //
    // We'll parse the following grammar in two steps:
    //
    //     a:
    //         b
    //         d
    //         a c
    //         a e
    //
    //     1. Parse switch { "b", "d" }, choose the one that matches
    //        (say, "b"), remember it.
    //     2. Parse switch { "a c", "a e" }, with "a" pre-matched as "b".
    //        If we've got a match, then that's the result. If not, then
    //        the result is the remembered "b".
    //
    // If a left-recursive grammar's match sequence is not longer than
    // the remembered one the remember non-left-recursive grammar
    //
    // Note that multiple occurences of references to the parent grammar
    // is not a special case. As long as we've got a match for the leftmost
    // reference, the rest of the grammar is not left-recursive anymore,
    // and we can safely parse it as usual.

    switch (step->state) {
	case ParsingStep_Switch::State_NLR: {
	  // At this step we're parsing non-left-recursive grammars only.

	    DEBUG (
              errs->println (_func, "NLR");
	    )

	    // Workaround for the unfortunate side effect of acceptor initialization:
	    // it nullifies the target pointer.
	    ParserElement *tmp_nlr_parser_element = step->nlr_parser_element;

#ifndef VSLAB_ACCEPTOR
	    StRef<Acceptor> const nlr_acceptor =
		    st_grab (static_cast <Acceptor*> (new (std::nothrow) RefAcceptor<ParserElement> (&step->nlr_parser_element)));
#else
	    VSlabRef< PtrAcceptor<ParserElement> > const nlr_acceptor =
		    VSlabRef< PtrAcceptor<ParserElement> >::forRef < PtrAcceptor<ParserElement> > (
			    parsing_state->ptr_acceptor_slab.alloc ());
	    nlr_acceptor->init (&step->nlr_parser_element);
	    DEBUG_INT (
              errs->println (_func, "NLR: acceptor: "
                             "0x", fmt_hex, (Uint64) (Acceptor*) nlr_acceptor);
	    )
#endif

	    step->nlr_parser_element = tmp_nlr_parser_element;

	    bool got_new_step = false;
	    while (step->cur_nlr_el != NULL) {
		DEBUG (
                  errs->println (_func, "NLR: iteration");
		)

		SwitchGrammarEntry &entry = *step->cur_nlr_el->data;
		step->cur_nlr_el = step->cur_nlr_el->next;

		if (!is_cur_variant (parsing_state, &entry))
		    continue;

                {
                    bool res = false;
                    if (!parse_switch_upwards_green (parsing_state, &entry, &res))
                        return Result::Failure;
                    if (!res)
                        continue;
                }

		// I suppose that the better option is to give up on detecting left-recursive
		// grammars for the cases where subgrammar is not a compound one. For pargen-generated
		// grammars this means we're never giving up :)
		if (entry.grammar->grammar_type == Grammar::t_Compound) {
		    DEBUG (
                      errs->println (_func, "NLR: _Compound");
		    )

		    Grammar_Compound *grammar = static_cast <Grammar_Compound*> (entry.grammar.ptr ());

		    {
			Grammar * const first_subg = grammar->getFirstSubgrammar ();
			if (first_subg == step->grammar)
			{
			  // The subgrammar happens to be a left-recursive one.
			  // We're simply proceeding to the next subgrammar.

			    continue;
			}
		    }

		    VStack::Level const tmp_vstack_level = parsing_state->step_vstack.getLevel ();
		    VStack::Level const tmp_el_level = parsing_state->el_vstack->getLevel ();
		    ParsingStep_Compound * const compound_step =
			    new (parsing_state->step_vstack.push_malign (
                                                sizeof (ParsingStep_Compound), alignof (ParsingStep_Compound)))
                                        ParsingStep_Compound;
		    compound_step->vstack_level = tmp_vstack_level;
		    compound_step->el_level = tmp_el_level;
		    compound_step->lr_parent = step->grammar;
		    compound_step->acceptor = nlr_acceptor;
		    compound_step->optional = false;
		    compound_step->grammar = grammar;
		    compound_step->cur_subg_el = grammar->grammar_entries.first;
		    compound_step->parser_element = grammar->createParserElement (parsing_state->el_vstack);
		    push_step (parsing_state, compound_step);
		} else {
		    DEBUG (
                      errs->println (_func, "NLR: non-compound");
		    )

		    // TEST
                    unreachable ();

		    // Note: this path is currently inadequate.
#if 0
		    ParsingResult pres = parse_grammar (parsing_state, entry.grammar, nlr_acceptor, false /* optional */);
		    if (pres == ParseEmptyMatch ||
			pres == ParseNoMatch)
		    {
			continue;
		    }

		    if (pres == ParseNonemptyMatch) {
			step->parser_element = step->nlr_parser_element;
			// TODO Call match_func here (helps to avoid recursion)
			if (!parse_switch_match (parsing_state, step, true /* match */, false /* empty_match */))
                            return Result::Failure;
		    } else {
			assert (pres == ParseUp);
                    }
#endif
		}

		got_new_step = true;
		break;
	    }

	    if (!got_new_step) {
		if (step->got_empty_nlr_match) {
		  // We've got an empty match. It cannot be used for handling left-recursive grammars,
		  // so we just accept it.

		    DEBUG_INT (
                      errs->println (_func, "empty NLR match, step->nlr_parser_element: "
                                     "0x", fmt_hex, (UintPtr) step->nlr_parser_element);
		    )
		    step->parser_element = step->nlr_parser_element;

		    if (step->grammar->accept_func != NULL) {
			DEBUG_CB (
                          errs->println (_func, "calling accept_func()");
			)
			step->grammar->accept_func (step->parser_element,
						    parsing_state,
						    parsing_state->user_data);
		    }

		    if (!parse_switch_final_match (parsing_state, step, true /* empty_match */))
                        return Result::Failure;
		} else {
		  // We've tried all of the Switch grammar's non-left-recursive subgrammars,
		  // and none of them match.

		  // Still, if there are lef-recursive subgrammars with optional left-recursive part,
		  // then there might be a match for the current switch grammar.
		  // Proceeding to left-recursive subgrammars handling.

		  // Note: This code path and functions' naming is counterintuitive.
		  // It would make sense to sort this out.

		    if (!parse_switch_match (parsing_state, step, false /* match */, false /* empty_match */))
                        return Result::Failure;
		}
	    }
	} break;
	case ParsingStep_Switch::State_LR: {
	  // We're parsing only left-recursive grammars now.

	    DEBUG (
              errs->println (_func, "LR");
	    )

#ifndef VSLAB_ACCEPTOR
	    StRef<Acceptor> const lr_acceptor =
		    st_grab (static_cast <Acceptor*> (new (std::nothrow) RefAcceptor<ParserElement> (&step->parser_element)));
#else
	    VSlabRef< PtrAcceptor<ParserElement> > const lr_acceptor =
		    VSlabRef< PtrAcceptor<ParserElement> >::forRef < PtrAcceptor<ParserElement> > (
			    parsing_state->ptr_acceptor_slab.alloc ());
	    lr_acceptor->init (&step->parser_element);
	    DEBUG_INT (
              errs->println (_func, "LR: acceptor: 0x", fmt_hex, (Uint64) (Acceptor*) lr_acceptor);
	    )
#endif

	    bool got_new_step = false;
	    while (step->cur_lr_el != NULL) {
		DEBUG (
                  errs->println (_func, "LR: iteration");
		)

		SwitchGrammarEntry &entry = *step->cur_lr_el->data;
		step->cur_lr_el = step->cur_lr_el->next;

		if (entry.grammar->grammar_type != Grammar::t_Compound)
		    continue;

		if (!is_cur_variant (parsing_state, &entry))
		    continue;

	      // Note: checking negative cache here is pointless, since it will
	      // be checked in parse_up() anyway.

		Grammar_Compound *grammar = static_cast <Grammar_Compound*> (entry.grammar.ptr ());

		{
		  // Checking if the grammar is left-recursive and suitable.

		    CompoundGrammarEntry * const first_subg = grammar->getFirstSubgrammarEntry ();
		    if (first_subg == NULL ||
			(first_subg->grammar != step->grammar))
		    {
		      // The subgrammar is not a left-recursive one.
		      // Proceeding to the next subgrammar.

			DEBUG (
                          errs->println (_func, "LR: non-lr");
			)

			continue;
		    }

		    if (!step->got_empty_nlr_match    &&
			!step->got_nonempty_nlr_match &&
			!(first_subg->flags & CompoundGrammarEntry::Optional))
		    {
		      // If we've got an empty nlr match, then only left-recursive grammars
		      // with optional left-recursive part can match.

			continue;
		    }
		}

		got_new_step = true;

		VStack::Level const tmp_vstack_level = parsing_state->step_vstack.getLevel ();
		VStack::Level const tmp_el_level = parsing_state->el_vstack->getLevel ();
		ParsingStep_Compound * const compound_step =
			new (parsing_state->step_vstack.push_malign (
                                            sizeof (ParsingStep_Compound), alignof (ParsingStep_Compound)))
                                    ParsingStep_Compound;
		compound_step->vstack_level = tmp_vstack_level;
		compound_step->el_level = tmp_el_level;
		compound_step->lr_parent = step->grammar;
		compound_step->acceptor = lr_acceptor;
		compound_step->optional = false;
		compound_step->grammar = grammar;
		// We'll start parsing from the second subgrammar (the first one is
		// a left-recursive reference to the parent grammar).
		compound_step->cur_subg_el = grammar->getSecondSubgrammarElement ();
		compound_step->parser_element = grammar->createParserElement (parsing_state->el_vstack);

//#if 0
// TODO This breaks normal operation. Look at this carefully.
// Deprecated as long as we don't call leading inline accept callbacks.

		// Note: This is a hack: we create the checkpoint early to be able
		// to call inline accept functions for match simulation.
		if (parsing_state->lookup_data)
		    parsing_state->lookup_data->newCheckpoint ();
//#endif

		{
		  // We're simulating a match. Inline accept callbacks which stand before
		  // the left-recursive subgrammar must be called.

		  // Note: It looks like calling these accept callbacks is confusing and does no good.

		    List< StRef<CompoundGrammarEntry> >::DataIterator iter (grammar->grammar_entries);
		    while (!iter.done ()) {
			StRef<CompoundGrammarEntry> &entry = iter.next ();
			if (entry->inline_match_func == NULL)
			    break;

			// TODO Aborting is a temporal measure.
			//
			// [10.09.15] Not so temporal anymore. I think this should become
			// a permanent ban.
                        errs->println (_func, "Leading inline accept callbacks in left-recursive grammars "
                                       "are never called");
                        unreachable ();
		    }
		}

		if (step->nlr_parser_element != NULL) {
		  // Pre-setting the compound grammar's first subgrammar with
		  // the remembered non-left-recursive match.

		    CompoundGrammarEntry * const cg_entry = grammar->getFirstSubgrammarEntry ();
		    assert (cg_entry);

		    if (cg_entry->assignment_func != NULL)
			cg_entry->assignment_func (compound_step->parser_element, step->nlr_parser_element);
		}

		push_step (parsing_state, compound_step, false /* new_checkpoint */);
		break;
	    }

	    if (!got_new_step) {
	      // None of left-recursive subgrammars match. In this case,
	      // the remembered non-left-recursive match is the result.

		DEBUG (
                  errs->print (_func, "LR: none match");
		)

		step->parser_element = step->nlr_parser_element;

		if (step->got_nonempty_nlr_match &&
		    step->grammar->match_func != NULL)
		{
		    DEBUG_CB (
                      errs->println (_func, "calling match_func()");
		    )
		    if (!step->grammar->match_func (step->parser_element, parsing_state, parsing_state->user_data))
			return pop_step (parsing_state, false /* match */, false /* empty_match */);
		}

		if (step->got_lr_match) {
		  // We've got a left-recursive match.

		    if (!parse_switch_final_match (parsing_state, step, false /* empty_match */))
                        return Result::Failure;
		} else {
		    if (!step->got_nonempty_nlr_match && !step->got_empty_nlr_match) {
			if (step->optional) {
			    if (step->grammar->accept_func != NULL) {
				DEBUG_CB (
                                  errs->println (_func, "calling accept_func(NULL)");
				)
				step->grammar->accept_func (NULL,
							    parsing_state,
							    parsing_state->user_data);
			    }

			    if (!pop_step (parsing_state, true /* match */, true /* empty_match */))
                                return Result::Failure;
			} else
			    if (!pop_step (parsing_state, false /* match */, false /* empty_match */))
                                return Result::Failure;
		    } else {
			if (step->got_empty_nlr_match) {
			    if (step->grammar->accept_func != NULL) {
				DEBUG_CB (
                                  errs->println (_func, "calling accept_func(NULL)");
				)
				step->grammar->accept_func (NULL,
							    parsing_state,
							    parsing_state->user_data);
			    }

			    if (!parse_switch_final_match (parsing_state, step, true /* empty_match */))
                                return Result::Failure;
			} else
			    if (!parse_switch_final_match (parsing_state, step, false /* empty_match */))
                                return Result::Failure;
		    }
		}
	    }
	} break;
	default:
            unreachable ();
    }

    DEBUG (
      errs->println (_func, "done");
    )
    return Result::Success;
}

static mt_throws Result
parse_alias (ParsingState      * const mt_nonnull parsing_state,
	     ParsingStep_Alias * const mt_nonnull step)
{
    DEBUG_FLO (
      errs->println (_func_);
    )

    assert (parsing_state && step);

    VSlabRef< PtrAcceptor<ParserElement> > acceptor =
	    VSlabRef< PtrAcceptor<ParserElement> >::forRef < PtrAcceptor<ParserElement> > (
		    parsing_state->ptr_acceptor_slab.alloc ());
    acceptor->init (&step->parser_element);

    ParsingResult pres;
    if (!parse_grammar (parsing_state,
                        static_cast <Grammar_Alias*> (step->grammar)->aliased_grammar,
                        acceptor,
                        step->optional,
                        &pres))
    {
        return Result::Failure;
    }

    if (pres == ParseUp)
	return Result::Success;

    switch (pres) {
	case ParseNonemptyMatch:
	    if (!pop_step (parsing_state, true /* match */, false /* empty_match */))
                return Result::Failure;
	    break;
	case ParseEmptyMatch:
	    if (!pop_step (parsing_state, true /* match */, true /* empty_match */))
                return Result::Failure;
	    break;
	case ParseNoMatch:
	    DEBUG_INT (
              errs->println (_func, "pop_step: false, false");
	    )
	    if (!pop_step (parsing_state, false /* match */, false /* empty_match */))
                return Result::Failure;
	    break;
	default:
            unreachable ();
    }

    return Result::Success;
}

static mt_throws Result
parse_up (ParsingState * const mt_nonnull parsing_state)
{
    assert (parsing_state);

    ParsingStep &_step = parsing_state->getLastStep ();

    switch (_step.parsing_step_type) {
	case ParsingStep::t_Sequence: {
	    DEBUG_INT (
	      errs->println (_func, "ParsingStep::_Sequence");
	    )
	    ParsingStep_Sequence &step = static_cast <ParsingStep_Sequence&> (_step);

            return parse_sequence_match (parsing_state, &step);
	} break;
	case ParsingStep::t_Compound: {
	    DEBUG_INT (
	      errs->println (_func, "ParsingStep::_Compound");
	    )

	    ParsingStep_Compound &step = static_cast <ParsingStep_Compound&> (_step);

	    if (static_cast <Grammar_Compound*> (step.grammar)->getFirstSubgrammar () == step.grammar) {
	      // FIXME 1) This is now allowed;
	      //       2) This assertion should be hit for lr grammars.
	      //
	      // The compound grammar happens to be left-recursive. We do not support that.
                unreachable ();
	    }

	    if (step.grammar->begin_func != NULL)
		step.grammar->begin_func (parsing_state->user_data);

	    return parse_compound_match (parsing_state, &step, true /* empty_match */);
	} break;
	case ParsingStep::t_Switch: {
	    DEBUG_INT (
	      errs->println (_func, "ParsingStep::_Switch");
	    )

	    ParsingStep_Switch &step = static_cast <ParsingStep_Switch&> (_step);

	    if (step.grammar->begin_func != NULL)
		step.grammar->begin_func (parsing_state->user_data);

	    return parse_switch_no_match_yet (parsing_state, &step);
	} break;
	case ParsingStep::t_Alias: {
	    DEBUG_INT (
              errs->println (_func, "ParsingStep::_Alias");
	    )

	    ParsingStep_Alias &step = static_cast <ParsingStep_Alias&> (_step);

	    if (step.grammar->begin_func != NULL)
		step.grammar->begin_func (parsing_state->user_data);

	    return parse_alias (parsing_state, &step);
	} break;
	default:
            unreachable ();
    };

    unreachable ();
    return Result::Success;
}

static mt_throws Result
parse_down (ParsingState * const mt_nonnull parsing_state)
{
    assert (parsing_state);

    ParsingStep &_step = parsing_state->getLastStep ();

    switch (_step.parsing_step_type) {
	case ParsingStep::t_Sequence: {
	    DEBUG_INT (
	      errs->println (_func, "ParsingStep::_Sequence");
	    )

	    ParsingStep_Sequence &step = static_cast <ParsingStep_Sequence&> (_step);

	    if (parsing_state->match) {
		if (!parse_sequence_match (parsing_state, &step))
                    return Result::Failure;
            } else {
		if (!parse_sequence_no_match (parsing_state, &step))
                    return Result::Failure;
            }
	} break;
	case ParsingStep::t_Compound: {
	    DEBUG_INT (
	      errs->println (_func, "ParsingStep::_Compound");
	    )

	    ParsingStep_Compound &step = static_cast <ParsingStep_Compound&> (_step);

	    if (parsing_state->match) {
		if (!parse_compound_match (parsing_state, &step, parsing_state->empty_match))
                    return Result::Failure;
            } else {
		if (!parse_compound_no_match (parsing_state, &step))
                    return Result::Failure;
            }
	} break;
	case ParsingStep::t_Switch: {
	    DEBUG_INT (
	      errs->println (_func, "ParsingStep::_Switch");
	    )

	    ParsingStep_Switch &step = static_cast <ParsingStep_Switch&> (_step);

	    if (parsing_state->match) {
		if (!parse_switch_match (parsing_state, &step, true /* match */, parsing_state->empty_match))
                    return Result::Failure;
            } else {
		if (!parse_switch_no_match_yet (parsing_state, &step))
                    return Result::Failure;
            }
	} break;
	case ParsingStep::t_Alias: {
	    DEBUG_INT (
              errs->println (_func, "ParsingStep::_Alias");
	    )

	    ParsingStep_Alias &step = static_cast <ParsingStep_Alias&> (_step);

	    if (parsing_state->match && !parsing_state->empty_match) {
		bool user_match = true;
		if (step.grammar->match_func != NULL) {
		    DEBUG_CB (
                      errs->println (_func, "calling match_func()");
		    )
		    parsing_state->position_changed = false;
		    if (!step.grammar->match_func (step.parser_element, parsing_state, parsing_state->user_data))
			user_match = false;

		    if (parsing_state->position_changed)
			return Result::Success;;
		}

		if (user_match) {
		    if (step.grammar->accept_func != NULL)
			step.grammar->accept_func (step.parser_element, parsing_state, parsing_state->user_data);

		    // This is a non-empty match case.
		    if (step.acceptor)
			step.acceptor->setParserElement (step.parser_element);

		    if (!pop_step (parsing_state, true /* match */, false /* empty_match */))
                        return Result::Failure;
		} else {
		  // FIXME Code duplication (see right below)

		    if (_step.optional) {
		      // This is an empty match case.

			if (step.grammar->accept_func != NULL)
			    step.grammar->accept_func (NULL, parsing_state, parsing_state->user_data);

			if (!pop_step (parsing_state, true /* match */, true /* empty_match */))
                            return Result::Failure;
		    } else {
			if (!pop_step (parsing_state, false /* match */, false /* empty_match */))
                            return Result::Failure;
		    }
		}
	    } else {
		if (_step.optional) {
		    // This is an empty match case.
		    assert (!step.parser_element);
		    if (step.grammar->accept_func != NULL)
			step.grammar->accept_func (NULL, parsing_state, parsing_state->user_data);

		    if (!pop_step (parsing_state, true /* match */, true /* empty_match */))
                        return Result::Failure;
		} else {
		    if (!pop_step (parsing_state, false /* match */, false /* empty_match */))
                        return Result::Failure;
		}
	    }
	} break;
	default:
            unreachable ();
    }

    return Result::Success;
}

// If 'tranzition_entries' is NULL, then we're in the process of
// iterating the nodes of the grammar.
//
// If 'tranzition_entries' is non-null, then do_optimizeGrammar() fills
// 'tranzition_entries' with possible tranzitions for 'grammar'.
//
// If 'tranzition_entries' is non-null and the path is fully optional,
// then 'ret_optional' is set to true on return. Otherwise, 'ret_optional'
// is set to false. If 'tranzition_entries' is null, then the value
// of 'ret_optional' after return is undefined. 'ret_optional' may be null.
//
// Returns 'true' if a tranzition has been recorded for the current path.
// If 'tranzition_entries' is null, then return value is undefined.
//
// TODO Separate 'tranzition_entries' filling from initial walkthrough.
//
static bool
do_optimizeGrammar (Grammar                                 * const mt_nonnull grammar,
		    SwitchGrammarEntry::TranzitionEntryHash * const tranzition_entries,
		    List< StRef<TranzitionMatchEntry> >     * const tranzition_match_entries,
		    SwitchGrammarEntry                      * const param_switch_grammar_entry,
		    bool                                    * const ret_optional,
		    Size                                    * const mt_nonnull loop_id)
{
    if (ret_optional)
	*ret_optional = false;

    if (!tranzition_entries) {
	if (grammar->optimized)
	    return false;

	grammar->optimized = true;
    } else {
	if (grammar->loop_id == *loop_id) {
	    if (ret_optional)
		*ret_optional = true;

	    return false;
	}

	grammar->loop_id = *loop_id;
    }

    switch (grammar->grammar_type) {
	case Grammar::t_Immediate: {
	    Grammar_Immediate_SingleToken * const grammar__immediate =
		    static_cast <Grammar_Immediate_SingleToken*> (grammar);

	    if (tranzition_entries) {
		if (!grammar__immediate->getToken() ||
		    grammar__immediate->getToken()->len() == 0)
		{
		    if (grammar__immediate->token_match_cb_name) {
			DEBUG_OPT (
			    errs->print ("  {", grammar__immediate->token_match_cb_name, "}");
			)
		    } else {
			assert (!grammar__immediate->token_match_cb);
			DEBUG_OPT (
			    errs->print (" ANY");
			)

			param_switch_grammar_entry->any_tranzition = true;
		    }
		} else {
		    DEBUG_OPT (
			errs->print (" ", grammar__immediate->getToken ());
		    )
		}

		if (grammar__immediate->token_match_cb_name) {
		    StRef<TranzitionMatchEntry> const tranzition_match_entry = st_grab (new TranzitionMatchEntry);
		    tranzition_match_entry->token_match_cb = grammar__immediate->token_match_cb;
		    DEBUG_OPT2 (
                      errs->println ("--- TRANZITION MATCH ENTRY");
		    )
		    tranzition_match_entries->append (tranzition_match_entry);
		} else
		if (grammar__immediate->getToken () &&
		    grammar__immediate->getToken ()->len() > 0)
		{
		    SwitchGrammarEntry::TranzitionEntry * const tranzition_entry = new SwitchGrammarEntry::TranzitionEntry;
		    tranzition_entry->grammar_name = st_grab (new String (grammar__immediate->getToken()->mem()));
		    tranzition_entries->add (tranzition_entry);
		}
	    }

	    return true;
	} break;
	case Grammar::t_Compound: {
	    Grammar_Compound * const grammar__compound =
		    static_cast <Grammar_Compound*> (grammar);

	    Bool got_tranzition = false;
	    Bool optional = true;
	    List< StRef<CompoundGrammarEntry> >::DataIterator iter (grammar__compound->grammar_entries);
	    while (!iter.done ()) {
		StRef<CompoundGrammarEntry> &compound_grammar_entry = iter.next ();

		if (!compound_grammar_entry->grammar) {
		  // _AcceptCb or _UniversalAcceptCb or UpwardsAnchor.
		    continue;
		}

		if (tranzition_entries) {
		    bool tmp_optional = false;
		    got_tranzition = do_optimizeGrammar (compound_grammar_entry->grammar,
							 tranzition_entries,
							 tranzition_match_entries,
							 param_switch_grammar_entry,
							 &tmp_optional,
							 loop_id);
		    if (!tmp_optional &&
			!(compound_grammar_entry->flags & CompoundGrammarEntry::Optional))
		    {
			optional = false;
		    }

		    if (!optional)
			break;
		} else {
		    do_optimizeGrammar (compound_grammar_entry->grammar,
					NULL /* tranzition_entries */,
					NULL /* tranzition_match_entries */,
					NULL /* switch_grammar_entry */,
					NULL /* ret_optional */,
					loop_id);
		}
	    }

	    if (ret_optional)
		*ret_optional = optional;

	    return got_tranzition;
	} break;
	case Grammar::t_Switch: {
	    Grammar_Switch * const grammar__switch =
		    static_cast <Grammar_Switch*> (grammar);

	    Bool got_tranzition = false;
	    Bool optional = true;
	    List< StRef<SwitchGrammarEntry> >::DataIterator iter (grammar__switch->grammar_entries);
	    while (!iter.done ()) {
		StRef<SwitchGrammarEntry> &switch_grammar_entry = iter.next ();

		if (tranzition_entries) {
		    bool tmp_optional = false;
		    got_tranzition = do_optimizeGrammar (switch_grammar_entry->grammar,
							 tranzition_entries,
							 tranzition_match_entries,
							 param_switch_grammar_entry,
							 &tmp_optional,
							 loop_id);
		    if (!tmp_optional &&
			!(switch_grammar_entry->flags & CompoundGrammarEntry::Optional))
		    {
			optional = false;
		    }
		} else {
		    DEBUG_OPT (
			errs->print (switch_grammar_entry->grammar->toString (), ": ");
		    )
		    {
			bool tmp_optional = false;
			do_optimizeGrammar (switch_grammar_entry->grammar,
					    &switch_grammar_entry->tranzition_entries,
					    &switch_grammar_entry->tranzition_match_entries,
					    switch_grammar_entry,
					    &tmp_optional,
					    loop_id);
			if (tmp_optional) {
			  // Fully optional grammars should not be upwards-optimized.
			  // We add 'any' token to force entering.
			    switch_grammar_entry->any_tranzition = true;
			}
		    }
		    DEBUG_OPT (
			errs->print ("\n");
		    )

		    assert (*loop_id + 1 > *loop_id);
		    (*loop_id) ++;
		    do_optimizeGrammar (switch_grammar_entry->grammar,
					NULL /* tranzition_entries */,
					NULL /* tranzition_match_entries */,
					NULL /* switch_grammar_entry */,
					NULL /* ret_optional */,
					loop_id);
		}
	    }

	    if (ret_optional)
		*ret_optional = optional;

	    return got_tranzition;
	} break;
	case Grammar::t_Alias: {
	    Grammar_Alias * const grammar_alias =
		    static_cast <Grammar_Alias*> (grammar);

	    return do_optimizeGrammar (grammar_alias->aliased_grammar,
				       tranzition_entries,
				       tranzition_match_entries,
				       param_switch_grammar_entry,
				       ret_optional,
				       loop_id);
	} break;
	default:
            unreachable ();
    }

    unreachable ();
    return false;
}

void
optimizeGrammar (Grammar * const mt_nonnull grammar)
{
    Size loop_id = 1;
    do_optimizeGrammar (grammar,
			NULL /* tranzition_entries */,
			NULL /* tranzition_match_entries */,
			NULL /* switch_grammar_entry */,
			NULL /* ret_optional */,
			&loop_id);
}

// Как работает парсер:
//
// Состояние парсера - список ступеней (ParsingStep). Текущая ступень находится в конце списка.
// Первая ступень - искуственно созданная, списочная (ParsingStep_Sequence), это список всех
// вхождений исходной грамматики.
//
// Условно полагаем, что корневая грамматика находится "внизу".
// Движение вверх (ParsingState::Up) - когда мы начинаем рассматривать новую грамматику.
// Движение вниз (ParsingState::Down) - когда мы рассмотрели грамматику и спускаемся на
// предыдущую ступень. Для перемещения на ступень вверх используем push_step(),
// вниз - pop_step().
//
// Ступени могут быть трёх типов: повторения (Sequence), последовательности (Compound) и
// вариативные (Switch).
//     Ступени Sequence описывают произвольное число вхождений подграмматики.
//     Ступени Compound задают строгую последовательность подграмматик.
//     Ступени Switch предполагают возможность вхождения одной из нескольких подграмматик.
//
mt_throws Result
parse (TokenStream    * const mt_nonnull token_stream,
       LookupData     * const lookup_data,
       void           * const user_data,
       Grammar        * const mt_nonnull grammar,
       ParserElement ** const ret_element,
       StRef<StReferenced> * const ret_element_container,
       ConstMemory      const default_variant,
       ParserConfig   *parser_config,
       bool             const debug_dump)
{
    assert (token_stream && grammar);

    if (ret_element)
	*ret_element = NULL;

    if (ret_element_container)
        *ret_element_container = NULL;

    StRef<ParserConfig> tmp_parser_config;
    if (parser_config == NULL) {
	tmp_parser_config = createDefaultParserConfig ();
	parser_config = tmp_parser_config;
    }

    StRef<ParsingState> parsing_state = st_grab (new ParsingState);
    parsing_state->parser_config = parser_config;
    parsing_state->nest_level = 0;
    parsing_state->token_stream = token_stream;
    parsing_state->lookup_data = lookup_data;
    parsing_state->user_data = user_data;
    parsing_state->cur_direction = ParsingState::Up;
    parsing_state->cur_positive_cache_entry = &parsing_state->positive_cache_root;
    parsing_state->negative_cache.goRight ();
    parsing_state->default_variant = default_variant;

    parsing_state->debug_dump = debug_dump;

    VSlabRef< PtrAcceptor<ParserElement> > acceptor =
	    VSlabRef< PtrAcceptor<ParserElement> >::forRef < PtrAcceptor<ParserElement> > (
		    parsing_state->ptr_acceptor_slab.alloc ());
    acceptor->init (ret_element);

    if (parsing_state->lookup_data)
	parsing_state->lookup_data->newCheckpoint ();

    ParsingResult pres;
    if (!parse_grammar (parsing_state, grammar, acceptor, false /* optional */, &pres))
        return Result::Failure;

    if (ret_element_container)
        *ret_element_container = parsing_state->el_vstack_container;

    if (pres == ParseNonemptyMatch ||
	pres == ParseEmptyMatch    ||
	pres == ParseNoMatch)
    {
	return Result::Success;
    }

    assert (pres == ParseUp);

    while (!parsing_state->step_list.isEmpty()) {
	switch (parsing_state->cur_direction) {
	    case ParsingState::Up:
		DEBUG_INT (
		  errs->println (_func, "ParsingState::Up");
		)
		if (!parse_up (parsing_state))
                    return Result::Failure;

		break;
	    case ParsingState::Down:
		DEBUG_INT (
		  errs->println (_func, "ParsingState::Down");
		)
		if (!parse_down (parsing_state))
                    return Result::Failure;

		break;
	    default:
                unreachable ();
	}
    }

    return Result::Success;
}

}

