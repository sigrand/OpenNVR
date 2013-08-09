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


#include <pargen/util.h>

#include <pargen/pargen_task_parser.h>


#define DEBUG(a) ;
#define DEBUG_ANCHORS(a) ;
#define DEBUG_OLD(a) ;

#define FUNC_NAME(a) ;


using namespace M;

namespace Pargen {

namespace {
class LookupData : public StReferenced
{
public:
//protected:
    class LookupNode : public StReferenced
    {
    public:
	StRef<String> name;
	StRef<Declaration_Phrases> decl_phrases;
    };

    Map< StRef<LookupNode>,
	 MemberExtractor< LookupNode,
			  StRef<String>,
			  &LookupNode::name,
			  Memory,
			  AccessorExtractor< String,
					     Memory,
					     &String::mem > >,
	 MemoryComparator<> >
	    lookup_map;

public:
    // Returns false if there is a phrase with such name already.
    bool addDeclaration (Declaration_Phrases * const mt_nonnull decl_phrases,
			 ConstMemory           const name)
    {
	if (!lookup_map.lookup (name).isNull ())
	    return false;

	StRef<LookupNode> const lookup_node = st_grab (new (std::nothrow) LookupNode);
	lookup_node->name = st_grab (new (std::nothrow) String (name));
	lookup_node->decl_phrases = decl_phrases;

	lookup_map.add (lookup_node);

	return true;
    }

    StRef<Declaration_Phrases> lookupDeclaration (ConstMemory   const name,
                                                  Bool        * const ret_is_alias)
    {
	if (ret_is_alias)
	    *ret_is_alias = false;

	Declaration_Phrases *ret_decl_phrases = NULL;

	{
	    ConstMemory cur_name = name;
	    for (;;) {
		DEBUG_OLD (
                  errs->println ("--- lookup: ", cur_name);
		)
		MapBase< StRef<LookupNode> >::Entry const map_entry = lookup_map.lookup (cur_name);
		if (map_entry.isNull ())
		    break;

		assert (map_entry.getData ()->decl_phrases);
		Declaration_Phrases * const decl_phrases = map_entry.getData ()->decl_phrases;
		if (!decl_phrases->is_alias) {
		    ret_decl_phrases = decl_phrases;
		    break;
		}

		if (ret_is_alias)
		    *ret_is_alias = true;

		DEBUG_OLD (
                  errs->println ("--- lookupDeclaration: alias: ", cur_name);
		)
		cur_name = decl_phrases->aliased_name->mem();
	    }
	}

	return ret_decl_phrases;
    }
};
}

static mt_throws Result
getNonwhspToken (TokenStream * const mt_nonnull token_stream,
                 ConstMemory * const mt_nonnull ret_mem)
{
    for (;;) {
        ConstMemory token;
        if (!token_stream->getNextToken (&token))
            return Result::Failure;
	if (token.len() == 0) {
            DEBUG (
              errs->println (_func, "EOF");
            )
            *ret_mem = ConstMemory ();
            return Result::Success;
        }

	if (equal (token, "\n") ||
	    equal (token, "\t") ||
	    equal (token, " "))
	{
	    continue;
	}

	DEBUG (
          errs->println (_func, "token: ", token);
        )

	// Skipping comments
	if (equal (token, "#")) {
	    for (;;) {
                if (!token_stream->getNextToken (&token))
                    return Result::Failure;
		if (token.len() == 0) {
                    *ret_mem = ConstMemory();
                    return Result::Success;
                }

		DEBUG (
                  errs->println (_func, "token (#): ", token);
                )

		if (equal (token, "\n"))
		    break;
	    }

	    continue;
	}

        *ret_mem = token;
        return Result::Success;
    }

    unreachable ();
    return Result::Success;
}

static mt_throws Result
getNextToken (TokenStream * const mt_nonnull token_stream,
              ConstMemory * const mt_nonnull ret_mem)
{
    ConstMemory token;
    if (!token_stream->getNextToken (&token))
        return Result::Failure;

    if (token.len() == 0) {
        *ret_mem = ConstMemory();
        return Result::Success;
    }

    DEBUG (
      errs->println (_func, "token: ", token);
    )

    // Skipping comments
    if (equal (token, "#")) {
	for (;;) {
	    if (!token_stream->getNextToken (&token))
                return Result::Failure;

	    if (token.len() == 0) {
                *ret_mem = ConstMemory();
                return Result::Success;
            }

	    DEBUG (
              errs->println (_func, "token (#): ", token);
	    )

	    if (equal (token, "\n")) {
                *ret_mem = ConstMemory ("\n");
                return Result::Success;
            }
	}
    }

    *ret_mem = token;
    return Result::Success;
}

static mt_throws Result
parsePhrasePart (TokenStream       * const mt_nonnull token_stream,
		 LookupData        * const mt_nonnull lookup_data,
                 StRef<PhrasePart> * const mt_nonnull ret_phrase_part)
{
    assert (token_stream && lookup_data);

    DEBUG (
      errs->println (_func_);
    )

    TokenStream::PositionMarker marker;
    if (!token_stream->getPosition (&marker))
        return Result::Failure;

  {
    StRef<PhrasePart> phrase_part;

    // Phrase part name specialization ("<name>").
    StRef<String> part_name;

    bool vertical_equivalence = false;
    for (;;) {
	ConstMemory token;
        if (!getNonwhspToken (token_stream, &token))
            return Result::Failure;
	if (token.len() == 0)
	    goto _no_match;

	DEBUG (
	  errs->println (_func, "iteration, token: ", token);
	)

	vertical_equivalence = false;

	if (equal (token, ")") ||
	    equal (token, "|"))
	{
	    goto _no_match;
	} else
	if (equal (token, "*")) {
	  // Any token

	    StRef<PhrasePart_Token> const phrase_part__token =
                    st_grab (new (std::nothrow) PhrasePart_Token);
	    phrase_part = phrase_part__token;
	} else
	if (equal (token, "{")) {
	  // Any token with a match callback

	    if (!getNonwhspToken (token_stream, &token))
                return Result::Failure;
	    if (token.len() == 0)
		goto _no_match;

	    StRef<String> const token_match_cb = st_grab (new (std::nothrow) String (token));

	    if (!getNonwhspToken (token_stream, &token))
                return Result::Failure;
	    if (token.len() == 0)
		goto _no_match;

	    if (!equal (token, "}"))
		goto _no_match;

	    StRef<PhrasePart_Token> const phrase_part__token = st_grab (new (std::nothrow) PhrasePart_Token);
	    phrase_part__token->token_match_cb = token_match_cb;

	    phrase_part = phrase_part__token;
	} else
	if (equal (token, "(")) {
	  // TODO Vertical equivalence

	    StRef<String> declaration_name;
	    StRef<String> phrase_name;
	    StRef<String> label_name;
	    StRef<String> jump_cb_name;

	    if (!getNonwhspToken (token_stream, &token))
                return Result::Failure;
	    if (token.len() == 0)
		goto _no_match;

	    declaration_name = capitalizeName (token);

	    if (!getNonwhspToken (token_stream, &token))
                return Result::Failure;
	    if (token.len() == 0)
		goto _no_match;

	    if (equal (token, ":")) {
		if (!getNonwhspToken (token_stream, &token))
                    return Result::Failure;
		if (token.len() == 0)
		    goto _no_match;

		phrase_name = st_grab (new (std::nothrow) String (token));

		if (!getNonwhspToken (token_stream, &token))
                    return Result::Failure;
		if (token.len() == 0)
		    goto _no_match;
	    }

	    if (!equal (token, "@"))
		goto _no_match;

	    if (!getNonwhspToken (token_stream, &token))
                return Result::Failure;
	    if (token.len() == 0)
		goto _no_match;

	    label_name = st_grab (new (std::nothrow) String (token));

	    if (!getNonwhspToken (token_stream, &token))
                return Result::Failure;
	    if (token.len() == 0)
		goto _no_match;

	    if (!equal (token, ")")) {
		jump_cb_name = st_grab (new (std::nothrow) String (token));

		if (!getNonwhspToken (token_stream, &token))
                    return Result::Failure;
		if (token.len() == 0)
		    goto _no_match;

		if (!equal (token, ")"))
		    goto _no_match;
	    }

	    StRef<PhrasePart_UpwardsAnchor> const phrase_part__upwards_anchor =
                    st_grab (new (std::nothrow) PhrasePart_UpwardsAnchor);
	    phrase_part__upwards_anchor->declaration_name = declaration_name;
	    phrase_part__upwards_anchor->phrase_name = phrase_name;
	    phrase_part__upwards_anchor->label_name = label_name;
	    phrase_part__upwards_anchor->jump_cb_name = jump_cb_name;
	    phrase_part = phrase_part__upwards_anchor;

	    vertical_equivalence = true;

	    if (!token_stream->getPosition (&marker))
                return Result::Failure;
	} else
	if (equal (token, "@")) {
	  // Label

	    if (!getNonwhspToken (token_stream, &token))
                return Result::Failure;
	    if (token.len() == 0)
		goto _no_match;

	    StRef<String> const label_name = st_grab (new (std::nothrow) String (token));

	    StRef<PhrasePart_Label> const phrase_part__label = st_grab (new (std::nothrow) PhrasePart_Label);
	    phrase_part__label->label_name = label_name;
	    phrase_part = phrase_part__label;
	} else
	if (equal (token, "<")) {
	  // PhrasePart name override.

	    if (!getNonwhspToken (token_stream, &token))
                return Result::Failure;
	    if (token.len() == 0)
		goto _no_match;

	    part_name = st_grab (new (std::nothrow) String (token));

	    if (!getNonwhspToken (token_stream, &token))
                return Result::Failure;
	    if (token.len() == 0)
		goto _no_match;

	    if (!equal (token, ">"))
		goto _no_match;

	    continue;
	} else
	if (equal (token, "/")) {
	  // Inline accept callback

	    StRef<String> cb_name;

	    if (!getNonwhspToken (token_stream, &token))
                return Result::Failure;
	    if (token.len() == 0)
		goto _no_match;

	    bool repetition = false;
	    if (equal (token, "!")) {
		if (!getNonwhspToken (token_stream, &token))
                    return Result::Failure;
		if (token.len() == 0)
		    goto _no_match;

		repetition = true;
	    }

	    bool universal = false;
	    if (equal (token, "*")) {
		if (!getNonwhspToken (token_stream, &token))
                    return Result::Failure;
		if (token.len() == 0)
		    goto _no_match;

		universal = true;
	    }

	    cb_name = st_grab (new (std::nothrow) String (token));

	    if (!getNonwhspToken (token_stream, &token))
                return Result::Failure;
	    if (!equal (token, "/"))
		goto _no_match;

	    if (universal) {
		StRef<PhrasePart_UniversalAcceptCb> const phrase_part__match_cb =
                        st_grab (new (std::nothrow) PhrasePart_UniversalAcceptCb);
		phrase_part__match_cb->cb_name = cb_name;
		phrase_part__match_cb->repetition = repetition;
		phrase_part = phrase_part__match_cb;
	    } else {
		StRef<PhrasePart_AcceptCb> const phrase_part__match_cb =
                        st_grab (new (std::nothrow) PhrasePart_AcceptCb);
		phrase_part__match_cb->cb_name = cb_name;
		phrase_part__match_cb->repetition = repetition;
		phrase_part = phrase_part__match_cb;
	    }
	} else
	if (equal (token, "[")) {
	  // Particular token

            DEBUG (
              errs->println (_func, "literal token");
            )

	    // Concatenated token
	    StRef<String> cat_token;
	    for (;;) {
		if (!getNonwhspToken (token_stream, &token))
                    return Result::Failure;
		if (token.len() == 0)
		    goto _no_match;

		// TODO Support for escapement: '\\' stands for '\', '\]' stands for ']'.

                DEBUG (
                  errs->println (_func, "literal token iteration: ", token);
                )

		if (equal (token, "]")) {
		    for (;;) {
		      // We can easily distinct between a ']' character at the end of the token
		      // and a closing bracket.

			TokenStream::PositionMarker pmark;
			if (!token_stream->getPosition (&pmark))
                            return Result::Failure;

			if (!getNonwhspToken (token_stream, &token))
                            return Result::Failure;
			if (token.len() == 0)
			    break;

                        DEBUG (
                          errs->println (_func, "literal token close iteration: ", token);
                        )

			if (!equal (token, "]")) {
                            DEBUG (
                              errs->println (_func, "literal token close iteration: end");
                            )
			    token_stream->setPosition (&pmark);
			    break;
			}

			cat_token = st_makeString (cat_token, token);
		    }

		    break;
		}

		// Note: This is (very) ineffective for very long tokens.
		cat_token = st_makeString (cat_token, token);
	    }

	    if (!cat_token) {
		// TODO Parsing error (empty token).
                exc_throw (InternalException, InternalException::BadInput);
                return Result::Failure;
	    }

	    StRef<PhrasePart_Token> const phrase_part__token =
                    st_grab (new (std::nothrow) PhrasePart_Token);
	    phrase_part__token->token = cat_token;
	    phrase_part = phrase_part__token;

            DEBUG (
              errs->println ("literal token extracted: ", phrase_part__token->token);
            )

	    {
		TokenStream::PositionMarker suffix_marker;
		if (!token_stream->getPosition (&suffix_marker))
                    return Result::Failure;

		if (!getNonwhspToken (token_stream, &token))
                    return Result::Failure;
		if (token.len() > 0) {
                    DEBUG (
                      errs->println ("suffix lookahead: ", token);
                    )

		    if (equal (token, "_opt")) {
			phrase_part__token->opt = true;
		    } else {
                        DEBUG (
                          errs->println ("no suffix");
                        )
			if (!token_stream->setPosition (&suffix_marker))
                            return Result::Failure;
                    }
		}
	    }
	} else {
#if 0
	    // TODO It's better to check here than at the very end of this function.

	    DEBUG (
              errf->print ("Pargen.(PargenTaskParser).parsePhrasePart: checking name").pendl ();
	    )

	    {
		Ref<TokenStream::PositionMarker> part_marker = token_stream->getPosition ();

		Ref<String> token = getNextToken (token_stream);
		if (token.getLength () > 0 &&
		    !TokenStream::isNewline (token) &&
		    (compareStrings (token->getData (), ")") ||
		     compareStrings (token->getData (), "|")))
		{
		    goto _no_match;
		}

		token_stream->setPosition (part_marker);
	    }
#endif

	    StRef<PhrasePart_Phrase> const phrase_part__phrase =
                    st_grab (new (std::nothrow) PhrasePart_Phrase);
	    phrase_part = phrase_part__phrase;

	    DEBUG (
              errs->println (_func, "original name: ", token);
	    )
	    ConstMemory name;
	    if (stringHasSuffix (token, "_opt_seq", &name) ||
		stringHasSuffix (token, "_seq_opt", &name))
	    {
		DEBUG (
                  errs->println (_func, "_opt_seq");
		)
		phrase_part__phrase->opt = true;
		phrase_part__phrase->seq = true;
	    } else
	    if (stringHasSuffix (token, "_opt", &name)) {
		DEBUG (
                  errs->println (_func, "_opt");
		)
		phrase_part__phrase->opt = true;
	    } else
	    if (stringHasSuffix (token, "_seq", &name)) {
		DEBUG (
                  errs->println (_func, "_seq");
		)
		phrase_part__phrase->seq = true;
	    }

	    if (part_name) {
		phrase_part->name = part_name;
		phrase_part->name_is_explicit = true;
		part_name = NULL;
	    } else {
		phrase_part->name = capitalizeName (name);

		// Decapitalizing the first letter.
		if (phrase_part->name->len() > 0) {
		    char c = phrase_part->name->mem().mem() [0];
		    if (c >= 0x41 /* 'A' */ &&
			c <= 0x5a /* 'Z' */)
		    {
			phrase_part->name->mem().mem() [0] = c + 0x20;
		    }
		}
	    }

	    phrase_part__phrase->phrase_name = capitalizeName (name);

            DEBUG (
              errs->println (_func, "name: ", phrase_part->name,
                             ", phrase_name: ", phrase_part__phrase->phrase_name);
            )
	}

	break;
    } // for (;;)

    {
	TokenStream::PositionMarker initial_marker;
	if (!token_stream->getPosition (&initial_marker))
            return Result::Failure;

	ConstMemory token;
        if (!getNextToken (token_stream, &token))
            return Result::Failure;
	if (token.len() > 0 &&
	    !equal (token, "\n"))
	{
            DEBUG (
              errs->println (_func, "lookahead token: ", token);
            )

	    if ((equal (token, ")") && !vertical_equivalence) ||
		equal (token, "|") ||
		equal (token, ":") ||
		equal (token, "{") ||
		equal (token, "="))
	    {
		goto _no_match;
	    }
	}

	if (!token_stream->setPosition (&initial_marker))
            return Result::Failure;
    }

    *ret_phrase_part = phrase_part;
    return Result::Success;
  }

_no_match:
    if (!token_stream->setPosition (&marker))
        return Result::Failure;

    *ret_phrase_part = NULL;
    return Result::Success;
}

static mt_throws Result
parsePhrase (TokenStream * const mt_nonnull token_stream,
	     LookupData  * const mt_nonnull lookup_data,
             StRef<Declaration_Phrases::PhraseRecord> * const ret_phrase,
	     bool        * const ret_null_phrase)
{
    assert (token_stream && lookup_data);

    if (ret_null_phrase != NULL)
	*ret_null_phrase = true;

    DEBUG (
      errs->println (_func_);
    )

    TokenStream::PositionMarker marker;
    if (!token_stream->getPosition (&marker))
        return Result::Failure;

    StRef<Declaration_Phrases::PhraseRecord> const phrase_record =
            st_grab (new (std::nothrow) Declaration_Phrases::PhraseRecord);
    phrase_record->phrase = st_grab (new (std::nothrow) Phrase);

    // Parsing phrase header. Some valid examples:
    //
    //     \n)		foo)
    //     A|		|)
    //     |||A||)	A|B|||C|
    //     A|B|foo)	\n|
    // 

    bool got_header = false;

    for (;;) {
	TokenStream::PositionMarker name_marker;
	if (!token_stream->getPosition (&name_marker))
            return Result::Failure;
	{
	    ConstMemory token;

	    DEBUG (
              errs->println (_func, "iteration");
	    )

	    // TODO Can't this be optimized?
	    // Checks in the following loop are pretty much the same.

	    if (!getNonwhspToken (token_stream, &token))
                return Result::Failure;
	    if (token.len() == 0)
		goto _no_name;

	    if (equal (token, ":") ||
		equal (token, "="))
	    {
                FilePosition fpos;
                if (!token_stream->getFilePosition (&fpos))
                    return Result::Failure;
                exc_throw (ParsingException,
                           fpos, st_makeString ("Unexpected '", token, "'"));
                return Result::Failure;
	    }

	    if (equal (token, "["))
		goto _no_name;

	    if (equal (token, ")")) {
	      // Empty name
		got_header = true;

		if (!token_stream->getPosition (&marker))
                    return Result::Failure;

		name_marker = marker;
		break;
	    }

	    if (equal (token, "|")) {
	      // We should get here only for '|' tokens at the very start.

		got_header = true;

		if (!token_stream->getPosition (&marker))
                    return Result::Failure;

		name_marker = marker;
		continue;
	    }

	    for (;;) {
		StRef<String> name = st_grab (new (std::nothrow) String (token));

		if (!getNextToken (token_stream, &token))
                    return Result::Failure;
		if (token.len() == 0) {
		    if (!got_header) {
			goto _no_name;
		    } else {
		      // Empty name
			break;
		    }
		}

		if (equal (token, "\n"))
		    goto _no_name;

		if (equal (token, "|")) {
		  // Got a variant's name

		    got_header = true;

		    phrase_record->variant_names.append (name);

		    for (;;) {
			if (!token_stream->getPosition (&marker))
                            return Result::Failure;

			name_marker = marker;

			if (!getNonwhspToken (token_stream, &token))
                            return Result::Failure;
			if (token.len() == 0)
			    goto _no_name;

			if (equal (token, ":") ||
			    equal (token, "="))
			{
                            FilePosition fpos;
                            if (!token_stream->getFilePosition (&fpos))
                                return Result::Failure;
                            exc_throw (ParsingException,
                                       fpos, st_makeString ("Unexpected ':' or '='"));
                            return Result::Failure;
			}

			if (equal (token, "["))
			    goto _no_name;

			if (equal (token, ")")) {
			    if (!token_stream->getPosition (&marker))
                                return Result::Failure;

			    name_marker = marker;
			    break;
			}

			if (equal (token, "|")) {
			    if (!token_stream->getPosition (&marker))
                                return Result::Failure;

			    name_marker = marker;
			    continue;
			}

			break;
		    }

		    name = st_grab (new (std::nothrow) String (token));
		    continue;
		}

		if (equal (token, ")")) {
		  // We've got phrase's name

		    got_header = true;

		    if (!token_stream->getPosition (&marker))
                        return Result::Failure;

		    name_marker = marker;

		    phrase_record->phrase->phrase_name = name;
		    break;
		}

		// We've stepped into phrase's real contents. Reverting back.
		goto _no_name;
	    } // for (;;)

	    break;
	}

    _no_name:
	if (!token_stream->setPosition (&name_marker))
            return Result::Failure;

	break;
    } // for (;;)

    DEBUG (
      if (phrase_record->phrase->phrase_name)
          errs->println (_func, "phrase_name: ", phrase_record->phrase->phrase_name);
      else
          errs->println (_func, "unnamed phrase");
    )

    for (;;) {
	StRef<PhrasePart> phrase_part;
        if (!parsePhrasePart (token_stream, lookup_data, &phrase_part))
            return Result::Failure;
	if (!phrase_part) {
	    DEBUG (
              errs->println (_func, "null phrase part");
	    )
	    break;
	}

	if (!phrase_record->phrase->phrase_name) {
	  // Auto-generating phrase's name if it is not specified explicitly.

	    // FIXME Sane automatic phrase names
	    switch (phrase_part->phrase_part_type) {
		case PhrasePart::t_Phrase: {
		    PhrasePart_Phrase * const phrase_part__phrase =
                            static_cast <PhrasePart_Phrase*> (phrase_part.ptr());
		    assert (phrase_part__phrase->name);
		    phrase_record->phrase->phrase_name = capitalizeName (phrase_part__phrase->name->mem());
		} break;
		case PhrasePart::t_Token: {
		    PhrasePart_Token * const phrase_part__token =
                            static_cast <PhrasePart_Token*> (phrase_part.ptr());
		    if (!phrase_part__token->token) {
			phrase_record->phrase->phrase_name = st_grab (new (std::nothrow) String ("Any"));
		    } else {
			// Note: This is pretty dumb. What if token is not a valid name?
			phrase_record->phrase->phrase_name = capitalizeName (phrase_part__token->token->mem());
		    }
		} break;
		case PhrasePart::t_AcceptCb: {
		    phrase_record->phrase->phrase_name = st_grab (new (std::nothrow) String ("AcceptCb"));
		} break;
		case PhrasePart::t_UniversalAcceptCb: {
		    phrase_record->phrase->phrase_name = st_grab (new (std::nothrow) String ("UniversalAcceptCb"));
		} break;
		case PhrasePart::t_UpwardsAnchor: {
		    phrase_record->phrase->phrase_name = st_grab (new (std::nothrow) String ("UpwardsAnchor"));
		} break;
		default:
                    unreachable ();
	    }
	}

	phrase_record->phrase->phrase_parts.append (phrase_part);
    }

    if (got_header) {
	if (ret_null_phrase)
	    *ret_null_phrase = false;
    }

    if (phrase_record->phrase->phrase_parts.isEmpty()) {
        *ret_phrase = NULL;
        return Result::Success;
    }

    if (ret_null_phrase)
	*ret_null_phrase = false;

    assert (phrase_record->phrase->phrase_name);
    *ret_phrase = phrase_record;
    return Result::Success;
}

static mt_throws Result
parseDeclaration_Phrases (TokenStream                * const mt_nonnull token_stream,
			  LookupData                 * const mt_nonnull lookup_data,
                          StRef<Declaration_Phrases> * const mt_nonnull ret_decl_phrases)
{
    assert (token_stream && lookup_data);

    DEBUG (
      errs->println (_func_);
    )

    StRef<Declaration_Phrases> const decl = st_grab (new (std::nothrow) Declaration_Phrases);

#if 0
// Note: This might be an alternate mechanism for specifying callbacks
    {
	Ref<TokenStream::PositionMarker> pmark = token_stream->getPosition ();

	Ref<String> token = getNonwhspToken (token_stream);
	if (token.isNull ())
	    return decl;

	if (compareStrings (token->getData (), "$")) {
	    token = getNonwhspToken (token_stream);
	    if (token.isNull ())
		return decl;

	    if (compareStrings (token->getData (), "begin")) {
		bool is_repetition = false;

		token = getNonwhspToken (token_stream);
		if (token.isNull ())
		    throw ParsingException (token_stream->getFilePosition (),
					    String::forData ("Callback name or '!' expected"));

		if (compareStrings (token->getData (), "!")) {
		    is_repetition = true;

		    token = getNonwhspToken (token_stream);
		    if (token.isNull ())
			throw ParsingException (token_stream->getFilePosition (),
						String::forData ("Callback name expected"));
		}

		decl->begin_cb_name = token;
		decl->begin_cb_repetition = is_repetition;
	    } else
		throw ParsingException (token_stream->getFilePosition (),
					String::forData ("Unknown special callback"));
	} else
	    token_stream->setPosition (pmark);
    }
#endif

    for (;;) {
	bool null_phrase;
        StRef<Declaration_Phrases::PhraseRecord> phrase_record;
	if (!parsePhrase (token_stream, lookup_data, &phrase_record, &null_phrase))
            return Result::Failure;
	if (null_phrase) {
	    DEBUG (
              errs->println (_func, "null phrase");
	    )
	    break;
	}

	if (phrase_record) {
	    DEBUG (
              errs->println ("appending phrase: ", phrase_record->phrase->phrase_name);
	    )
	    decl->phrases.append (phrase_record);
	}
    }

    *ret_decl_phrases = decl;
    return Result::Success;
}

static mt_throws Result
parseDeclaration_Callbacks (TokenStream                  * const mt_nonnull token_stream,
                            StRef<Declaration_Callbacks> * const mt_nonnull ret_decl_callbacks)
{
    assert (token_stream);

    DEBUG (
      errs->println (_func_);
    )

    StRef<Declaration_Callbacks> const decl = st_grab (new (std::nothrow) Declaration_Callbacks);

    for (;;) {
	FilePosition fpos;
        if (!token_stream->getFilePosition (&fpos))
            return Result::Failure;

	ConstMemory token;
        if (!getNonwhspToken (token_stream, &token))
            return Result::Failure;
	if (token.len() == 0) {
            exc_throw (ParsingException, fpos, st_makeString ("'}' expected"));
            return Result::Failure;
        }

	if (equal (token, "}"))
	    break;

	StRef<CallbackDecl> const callback_decl = st_grab (new (std::nothrow) CallbackDecl);
	callback_decl->callback_name = st_grab (new (std::nothrow) String (token));
	decl->callbacks.append (callback_decl);
    }

    *ret_decl_callbacks = decl;
    return Result::Success;
}

static mt_throws Result
parseDeclaration_Alias (TokenStream                * const mt_nonnull token_stream,
                        StRef<Declaration_Phrases> * const mt_nonnull ret_decl_phrases)
{
    assert (token_stream);

    DEBUG (
      errs->println (_func_);
    )

    StRef<Declaration_Phrases> const decl = st_grab (new (std::nothrow) Declaration_Phrases);
    decl->is_alias = true;

    ConstMemory token;
    if (!getNonwhspToken (token_stream, &token))
        return Result::Failure;
    if (token.len() == 0) {
        FilePosition fpos;
        if (!token_stream->getFilePosition (&fpos))
            return Result::Failure;
        exc_throw (ParsingException, fpos, st_grab (new (std::nothrow) String ("alias expected")));
        return Result::Failure;
    }

    decl->aliased_name = capitalizeName (token);

    *ret_decl_phrases = decl;
    return Result::Success;
}

static mt_throws Result
parseDeclaration (TokenStream        * const mt_nonnull token_stream,
		  LookupData         * const mt_nonnull lookup_data,
                  StRef<Declaration> * const mt_nonnull ret_decl)
{
    assert (token_stream || lookup_data);

    DEBUG (
      errs->println (_func_);
    )

    TokenStream::PositionMarker marker;
    if (!token_stream->getPosition (&marker))
        return Result::Failure;

  {
    ConstMemory token;
    if (!getNonwhspToken (token_stream, &token))
        return Result::Failure;
    if (token.len() == 0)
	goto _no_match;

    StRef<String> const declaration_name = capitalizeName (token);
    StRef<String> const lowercase_declaration_name = lowercaseName (token);

    FilePosition fpos;
    if (!token_stream->getFilePosition (&fpos))
        return Result::Failure;

    if (!getNonwhspToken (token_stream, &token))
        return Result::Failure;
    if (token.len() == 0) {
        exc_throw (ParsingException,
                   fpos, st_grab (new (std::nothrow) String ("':' or '{' expected")));
        return Result::Failure;
    }

    StRef<Declaration> decl;
    if (equal (token, ":")) {
	StRef<Declaration_Phrases> decl_phrases;
        if (!parseDeclaration_Phrases (token_stream, lookup_data, &decl_phrases))
            return Result::Failure;
	decl = decl_phrases;

	DEBUG (
          errs->println (_func, "adding Declaration_Phrases: ", declaration_name);
	)
	if (!lookup_data->addDeclaration (decl_phrases, declaration_name->mem()))
	    errs->println (_func, "duplicate Declaration_Phrases name: ", declaration_name);
    } else
    if (equal (token, "{")) {
        StRef<Declaration_Callbacks> decl_callbacks;
	if (!parseDeclaration_Callbacks (token_stream, &decl_callbacks))
            return Result::Failure;

        decl = decl_callbacks;
    } else
    if (equal (token, "=")) {
	StRef<Declaration_Phrases> decl_phrases;
        if (!parseDeclaration_Alias (token_stream, &decl_phrases))
            return Result::Failure;
	decl = decl_phrases;

	if (!lookup_data->addDeclaration (decl_phrases, declaration_name->mem()))
            errs->println (_func, "duplicate _Alias name: ", declaration_name);
    } else {
        exc_throw (ParsingException,
                   fpos, st_grab (new (std::nothrow) String ("':' or '{' expected")));
        return Result::Failure;
    }

    assert (decl);

    decl->declaration_name = declaration_name;
    decl->lowercase_declaration_name = lowercase_declaration_name;

    *ret_decl = decl;
    return Result::Success;
  }

_no_match:
    if (!token_stream->setPosition (&marker))
        return Result::Failure;

    *ret_decl = NULL;
    return Result::Success;
}

static bool
linkPhrases (Declaration_Phrases * const mt_nonnull decl_phrases,
	     LookupData          * const mt_nonnull lookup_data)
{
    assert (decl_phrases && lookup_data);

    List< StRef<Declaration_Phrases::PhraseRecord> >::DataIterator phrase_iter (decl_phrases->phrases);
    while (!phrase_iter.done ()) {
	StRef<Declaration_Phrases::PhraseRecord> &phrase_record = phrase_iter.next ();
	assert (phrase_record);

	Phrase * const phrase = phrase_record->phrase;
	List< StRef<PhrasePart> >::DataIterator phrase_part_iter (phrase->phrase_parts);
	while (!phrase_part_iter.done ()) {
	    StRef<PhrasePart> &phrase_part = phrase_part_iter.next ();
	    if (phrase_part->phrase_part_type == PhrasePart::t_Phrase) {
		PhrasePart_Phrase * const phrase_part__phrase =
                        static_cast <PhrasePart_Phrase*> (phrase_part.ptr ());
		assert (phrase_part__phrase->phrase_name);

		if (!phrase_part__phrase->decl_phrases) {
		    Bool is_alias;
		    StRef<Declaration_Phrases> const decl_phrases =
			    lookup_data->lookupDeclaration (phrase_part__phrase->phrase_name->mem(), &is_alias);
		    if (!decl_phrases) {
			// TODO throw ParsingException
			errs->println (_func, "unresolved name: ", phrase_part__phrase->phrase_name);
			return false;
		    }

		    phrase_part__phrase->decl_phrases = decl_phrases;

		    if (is_alias &&
			!phrase_part->name_is_explicit)
		    {
			phrase_part->name = st_grab (new (std::nothrow) String (decl_phrases->declaration_name->mem()));

			// Decapitalizing the first letter.
			if (phrase_part->name->len() > 0) {
			    char c = phrase_part->name->mem().mem() [0];
			    if (c >= 0x41 /* 'A' */ &&
				c <= 0x5a /* 'Z' */)
			    {
				phrase_part->name->mem().mem() [0] = c + 0x20;
			    }
			}
		    }
		}
	    }
	}
    }

    return true;
}

typedef Map < StRef<Declaration_Callbacks>,
	      DereferenceExtractor< Declaration_Callbacks,
				    Memory,
				    MemberExtractor< Declaration,
						     StRef<String>,
						     &Declaration::declaration_name,
						     Memory,
						     AccessorExtractor< String,
									Memory,
									&String::mem > > >,
	      MemoryComparator<> >
	Map__Declaration_Callbacks;

static void
linkCallbacks (Declaration_Phrases        * const mt_nonnull decl_phrases,
	       Map__Declaration_Callbacks &decl_callbacks)
{
    assert (decl_phrases);

    assert (decl_phrases->declaration_name);
    MapBase< StRef<Declaration_Callbacks> >::Entry const cb_entry =
	    decl_callbacks.lookup (decl_phrases->declaration_name->mem());
    if (!cb_entry.isNull ()) {
	StRef<Declaration_Callbacks> &decl_callbacks = cb_entry.getData ();
	assert (decl_callbacks);

	List< StRef<CallbackDecl> >::DataIterator cb_iter (decl_callbacks->callbacks);
	while (!cb_iter.done()) {
	    StRef<CallbackDecl> &callback_decl = cb_iter.next ();
	    assert (callback_decl && callback_decl->callback_name);
	    decl_phrases->callbacks.add (callback_decl);
	}
    }
}

static bool
linkAliases (PargenTask * const mt_nonnull pargen_task,
	     LookupData * const mt_nonnull lookup_data)
{
    List< StRef<Declaration> >::DataIterator decl_iter (pargen_task->decls);
    while (!decl_iter.done()) {
	StRef<Declaration> &decl = decl_iter.next ();
	if (decl->declaration_type != Declaration::t_Phrases)
	    continue;

	Declaration_Phrases &decl_phrases = static_cast <Declaration_Phrases&> (*decl);
	if (!decl_phrases.is_alias)
	    continue;

	StRef<Declaration_Phrases> const aliased_decl =
		lookup_data->lookupDeclaration (decl_phrases.aliased_name->mem(),
						NULL /* ret_is_alias */);
	if (!aliased_decl) {
	    errs->println (_func, "unresolved name: ", decl_phrases.aliased_name);
	    return false;
	}

	decl_phrases.deep_aliased_name = aliased_decl->declaration_name;

	{
	    MapBase< StRef<LookupData::LookupNode> >::Entry const map_entry =
		    lookup_data->lookup_map.lookup (decl_phrases.aliased_name->mem());
	    if (map_entry.isNull ()) {
		errs->println (_func, "unresolved name: ", decl_phrases.aliased_name);
		return false;
	    }

	    assert (map_entry.getData ()->decl_phrases);
	    decl_phrases.aliased_decl = map_entry.getData ()->decl_phrases;
	}
    }

    return true;
}

static bool
linkUpwardsAnchors_linkAnchor (LookupData               * const mt_nonnull lookup_data,
			       PhrasePart_UpwardsAnchor * const mt_nonnull phrase_part__upwards_anchor)
{
    StRef<Declaration_Phrases> const decl_phrases =
	    lookup_data->lookupDeclaration (
		    phrase_part__upwards_anchor->declaration_name->mem(),
		    NULL /* ret_is_alias */);
    if (!decl_phrases) {
	errs->println (_func, "unresolved name: ", phrase_part__upwards_anchor->declaration_name);
	return false;
    }

    bool got_switch_grammar_index = false;
    Size switch_grammar_index = 0;
    bool got_compound_grammar_index = false;
    Size compound_grammar_index = 0;
    List< StRef<Declaration_Phrases::PhraseRecord> >::DataIterator phrase_iter (decl_phrases->phrases);
    while (!phrase_iter.done()) {
	StRef<Declaration_Phrases::PhraseRecord> const phrase_record = phrase_iter.next ();

	if (equal (phrase_record->phrase->phrase_name->mem(),
                   phrase_part__upwards_anchor->phrase_name->mem()))
	{
	    got_switch_grammar_index = true;

	    List< StRef<PhrasePart> >::DataIterator part_iter (phrase_record->phrase->phrase_parts);
	    bool got_jump = false;
	    while (!part_iter.done()) {
		StRef<PhrasePart> &phrase_part = part_iter.next ();

		errs->println ("    ", phrase_part->toString ());

		switch (phrase_part->phrase_part_type) {
		    case PhrasePart::t_Label: {
			PhrasePart_Label * const phrase_part__label =
				static_cast <PhrasePart_Label*> (phrase_part.ptr());

			if (equal (phrase_part__label->label_name->mem(),
                                   phrase_part__upwards_anchor->label_name->mem()))
			{
			    errs->println ("    MATCH ", phrase_part__label->label_name);
			    got_compound_grammar_index = true;
			    got_jump = true;
			}
		    } break;
		    default: {
		      // Note that labels are not counted, because they do not
		      // appear in resulting compound grammars.
			compound_grammar_index ++;
		    } break;
		}

		if (got_jump)
		    break;
	    }

	    break;
	}

	switch_grammar_index ++;
    }

    if (!got_switch_grammar_index) {
	errs->println (_func, "switch subgrammar not found: ",
		       phrase_part__upwards_anchor->declaration_name,":",
		       phrase_part__upwards_anchor->phrase_name);
	return false;
    }

    if (!got_compound_grammar_index) {
	errs->println (_func, ": switch label not found: ",
		       phrase_part__upwards_anchor->declaration_name, ":",
		       phrase_part__upwards_anchor->phrase_name, "@",
		       phrase_part__upwards_anchor->label_name);
	return false;
    }

    DEBUG_ANCHORS (
      errs->println ("jump: ",
                     phrase_part__upwards_anchor->declaration_name, ":",
                     phrase_part__upwards_anchor->phrase_name, "@",
                     phrase_part__upwards_anchor->label_name, " "
                     "switch_i: ", switch_grammar_index, ", "
                     "compound_i: ", compound_grammar_index);
    )

    phrase_part__upwards_anchor->switch_grammar_index = switch_grammar_index;
    phrase_part__upwards_anchor->compound_grammar_index = compound_grammar_index;

    return true;
}

static Result
linkUpwardsAnchors (PargenTask * const mt_nonnull pargen_task,
		    LookupData * const mt_nonnull lookup_data)
{
    List< StRef<Declaration> >::DataIterator decl_iter (pargen_task->decls);
    while (!decl_iter.done()) {
	StRef<Declaration> &decl = decl_iter.next ();
	if (decl->declaration_type != Declaration::t_Phrases)
	    continue;

	Declaration_Phrases * const decl_phrases =
                static_cast <Declaration_Phrases*> (decl.ptr());

	List< StRef<Declaration_Phrases::PhraseRecord> >::DataIterator phrase_iter (decl_phrases->phrases);
	while (!phrase_iter.done()) {
	    StRef<Declaration_Phrases::PhraseRecord> &phrase_record = phrase_iter.next ();

	    List< StRef<PhrasePart> >::DataIterator part_iter (phrase_record->phrase->phrase_parts);
	    while (!part_iter.done()) {
		StRef<PhrasePart> &phrase_part = part_iter.next ();

		switch (phrase_part->phrase_part_type) {
		    case PhrasePart::t_UpwardsAnchor: {
			PhrasePart_UpwardsAnchor * const phrase_part__upwards_anchor =
				static_cast <PhrasePart_UpwardsAnchor*> (phrase_part.ptr());

			if (!linkUpwardsAnchors_linkAnchor (lookup_data,
							    phrase_part__upwards_anchor))
			{
			    // TODO FilePosition
                            exc_throw (ParsingException, FilePosition(), (String*) NULL /* message */);
                            return Result::Failure;
			}
		    } break;
		    default:
		      // Nothing to do
			;
		}
	    }
	}
    }

    return Result::Success;
}

Result
parsePargenTask (TokenStream       * const mt_nonnull token_stream,
                 StRef<PargenTask> * const mt_nonnull ret_pargen_task)
{
    assert (token_stream);

    StRef<PargenTask> const pargen_task = st_grab (new (std::nothrow) PargenTask ());

    Map__Declaration_Callbacks decls_callbacks;

    StRef<LookupData> const lookup_data = st_grab (new (std::nothrow) LookupData);
    for (;;) {
	StRef<Declaration> decl;
        if (!parseDeclaration (token_stream, lookup_data, &decl))
            return Result::Failure;
	if (!decl)
	    break;

	switch (decl->declaration_type) {
	    case Declaration::t_Phrases: {
		Declaration_Phrases * const decl_phrases =
                        static_cast <Declaration_Phrases*> (decl.ptr());
		pargen_task->decls.append (decl_phrases);
	    } break;
	    case Declaration::t_Callbacks: {
		Declaration_Callbacks * const decl_callbacks =
                        static_cast <Declaration_Callbacks*> (decl.ptr());
		decls_callbacks.add (decl_callbacks);
	    } break;
#if 0
	    case Declaration::t_Alias: {
		Declaration_Alias * const decl_alias = static_cast <Declaration_Alias*> (decl.ptr ());
		pargen_task->decls.append (decl_alias);
	    } break;
#endif
	    default:
                unreachable ();
	}
    }

    if (!linkAliases (pargen_task, lookup_data)) {
        exc_throw (ParsingException, FilePosition (), (String*) NULL /* message */);
        return Result::Failure;
    }

    List< StRef<Declaration> >::DataIterator decl_iter (pargen_task->decls);
    while (!decl_iter.done()) {
	StRef<Declaration> &decl = decl_iter.next ();
	if (decl->declaration_type != Declaration::t_Phrases)
	    continue;

	Declaration_Phrases &decl_phrases = static_cast <Declaration_Phrases&> (*decl);

	if (!linkPhrases (&decl_phrases, lookup_data)) {
            exc_throw (ParsingException, FilePosition (), (String*) NULL /* message */);
            return Result::Failure;
        }

	linkCallbacks (&decl_phrases, decls_callbacks);
    }

    if (!linkUpwardsAnchors (pargen_task, lookup_data))
        return Result::Failure;

    *ret_pargen_task = pargen_task;
    return Result::Success;
}

void
dumpDeclarations (PargenTask const * const mt_nonnull pargen_task)
{
    assert (pargen_task);

    errs->println (_func_);

    {
	List< StRef<Declaration> >::DataIterator decl_iter (pargen_task->decls);
	while (!decl_iter.done()) {
	    StRef<Declaration> &decl = decl_iter.next ();
            assert (decl);
	    errs->print ("Declaration: ", decl->declaration_name, "\n");
	}
    }

    errs->flush ();
}

}

