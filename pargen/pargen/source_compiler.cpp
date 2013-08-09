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


#include <pargen/source_compiler.h>


using namespace M;

namespace Pargen {

static mt_throws Result
compileSource_Phrase (File                     * const mt_nonnull file,
		      Phrase const             * const mt_nonnull phrase,
		      CompilationOptions const * const mt_nonnull opts,
		      ConstMemory                const declaration_name,
		      ConstMemory                const lowercase_declaration_name,
		      ConstMemory                const phrase_name,
		      bool                       const subtype,
		      bool                       const has_begin,
		      bool                       const has_match,
		      bool                       const has_accept)
{
    assert (file && phrase && opts);

    ConstMemory decl_name;
    ConstMemory lowercase_decl_name;
    bool global_grammar;
    if (equal (declaration_name, "*")) {
	decl_name = ConstMemory ("Grammar");
	lowercase_decl_name = ConstMemory ("grammar");
	global_grammar = true;
    } else {
	decl_name = declaration_name;
	lowercase_decl_name = lowercase_declaration_name;
	global_grammar = false;
    }

    StRef<String> phrase_prefix;
    if (subtype)
	phrase_prefix = st_makeString (decl_name, "_", phrase_name);
    else
	phrase_prefix = st_makeString (decl_name);

    {
	List< StRef<PhrasePart> >::DataIterator phrase_part_iter (phrase->phrase_parts);
	while (!phrase_part_iter.done ()) {
	    StRef<PhrasePart> &phrase_part = phrase_part_iter.next ();

	    if (phrase_part->phrase_part_type == PhrasePart::t_AcceptCb) {
		PhrasePart_AcceptCb * const phrase_part__accept_cb =
                        static_cast <PhrasePart_AcceptCb*> (phrase_part.ptr());

		if (phrase_part__accept_cb->repetition)
		    continue;

		if (!file->print ("bool ", phrase_part__accept_cb->cb_name, " (\n"
                                  "        ", opts->capital_header_name, "_", decl_name, " *parser_element,\n"
                                  "        Pargen::ParserControl *parser_control,\n"
                                  "        void *data);\n"
                                  "\n"
                                  "bool __pargen_", phrase_part__accept_cb->cb_name, " (\n"
                                  "        ParserElement *parser_element,\n"
                                  "        Pargen::ParserControl *parser_control,\n"
                                  "        void *data)\n"
                                  "{\n"
                                  "    ", opts->capital_header_name, "Element * const &_el =\n"
                                  "            static_cast <", opts->capital_header_name, "Element*> (parser_element);\n"
                                  "    assert (_el->", opts->header_name, "_element_type == ",
                                           opts->capital_header_name, "Element::t_", decl_name, ");\n"
                                  "\n"
                                  "    ", opts->capital_header_name, "_", decl_name, " * const &el =\n"
                                  "            static_cast <", opts->capital_header_name, "_", decl_name, "*> (_el);\n"
                                  "\n"
                                  "    return ", phrase_part__accept_cb->cb_name, " (el, parser_control, data);\n"
                                  "}\n"
                                  "\n"))
                {
                    return Result::Failure;
                }

		continue;
	    }

	    if (phrase_part->phrase_part_type == PhrasePart::t_UniversalAcceptCb) {
		PhrasePart_UniversalAcceptCb * const phrase_part__universal_accept_cb =
                        static_cast <PhrasePart_UniversalAcceptCb*> (phrase_part.ptr());

		if (phrase_part__universal_accept_cb->repetition)
		    continue;

		if (!file->print ("bool ", phrase_part__universal_accept_cb->cb_name, " (\n"
                                  "        ", opts->capital_header_name, "Element *parser_element,\n"
                                  "        Pargen::ParserControl *parser_control,\n"
                                  "        void *data);\n"
                                  "\n"
                                  "bool __pargen_", phrase_part__universal_accept_cb->cb_name, " (\n"
                                  "        ParserElement *parser_element,\n"
                                  "        Pargen::ParserControl *parser_control,\n"
                                  "        void *data)\n"
                                  "{\n"
                                  "    ", opts->capital_header_name, "Element * const &_el =\n"
                                  "            static_cast <", opts->capital_header_name, "Element*> (parser_element);\n",
                                  "\n"
                                  "    return ", phrase_part__universal_accept_cb->cb_name, " (_el, parser_control, data);\n"
                                  "}\n"
                                  "\n"))
                {
                    return Result::Failure;
                }

		continue;
	    }

	    if (phrase_part->phrase_part_type == PhrasePart::t_UpwardsAnchor) {
		PhrasePart_UpwardsAnchor * const phrase_part__upwards_anchor =
			static_cast <PhrasePart_UpwardsAnchor*> (phrase_part.ptr());

		if (phrase_part__upwards_anchor->jump_cb_name) {
		    if (!file->print ("bool ", phrase_part__upwards_anchor->jump_cb_name, " (\n"
                                      "        ParserElement *parser_element,\n"
                                      "        void *data);\n"
                                      "\n"))
                    {
                        return Result::Failure;
                    }
		}

		continue;
	    }

	    if (phrase_part->phrase_part_type == PhrasePart::t_Label) {
	      // TODO
		continue;
	    }

	    ConstMemory name_to_set;
	    ConstMemory type_to_set;
	    bool any_token = false;
	    if (phrase_part->phrase_part_type == PhrasePart::t_Token) {
		PhrasePart_Token * const phrase_part__token =
                    static_cast <PhrasePart_Token*> (phrase_part.ptr());

		if (!phrase_part__token->token ||
		    phrase_part__token->token->len() == 0)
		{
		    any_token = true;
		    name_to_set = ConstMemory ("any_token");
		    // This doesn't matter for "any" token;
		    type_to_set = ConstMemory ("__invalid_type__");
		} else {
		    continue;
		}
	    } else {
		assert (phrase_part->phrase_part_type == PhrasePart::t_Phrase);
		
		PhrasePart_Phrase * const phrase_part__phrase =
                        static_cast <PhrasePart_Phrase*> (phrase_part.ptr());

		name_to_set = phrase_part__phrase->name->mem();
		type_to_set = phrase_part__phrase->decl_phrases->declaration_name->mem();
	    }

	    if (!file->print ("static void\n",
                              opts->header_name, "_", phrase_prefix, "_set_", name_to_set, " (\n"
                              "        ParserElement *___self,\n"
                              "        ParserElement *___el)\n"
                              "{\n"
#if 0
                              "    assert (___self && ___el);\n"
#endif
                              "    if (___self == NULL ||\n"
                              "        ___el == NULL)\n"
                              "    {\n"
                              "        return;\n"
                              "    }\n"
                              "\n"
                              "    ", opts->capital_header_name, "Element * const &__self = "
                                      "static_cast <", opts->capital_header_name, "Element*> (___self);\n"))
            {
                return Result::Failure;
            }

	    if (!any_token) {
		if (!file->print ("    ", opts->capital_header_name, "Element * const &__el = "
                                          "static_cast <", opts->capital_header_name, "Element*> (___el);\n"))
                {
                    return Result::Failure;
                }
	    }

	    if (!file->print ("\n"
                              "    assert (__self->", opts->header_name, "_element_type == ",
                                      opts->capital_header_name, "Element::t_", decl_name))
            {
                return Result::Failure;
            }

	    if (!any_token) {
		if (!file->print (" ||\n"
                                  "             __el->", opts->header_name, "_element_type != ",
                                          opts->capital_header_name, "Element::t_", type_to_set))
                {
                    return Result::Failure;
                }
	    }
	    
	    if (!file->print (");\n"
                              "\n"))
            {
                return Result::Failure;
            }

	    if (subtype) {
		if (!file->print ("    ", opts->capital_header_name, "_", decl_name, " * const &_self = "
                                          "static_cast <", opts->capital_header_name, "_", decl_name, "*> (__self);\n"
                                  "\n"
                                  "    assert (_self->", lowercase_decl_name, "_type", " == ",
                                          opts->capital_header_name, "_", decl_name, "::t_", phrase_name, ");\n"
                                  "\n"
                                  "    ", opts->capital_header_name, "_", decl_name, "_", phrase_name, " * const &self = "
                                          "static_cast <", opts->capital_header_name, "_", decl_name, "_", phrase_name, "*> (_self);\n"
                                  "\n"))
                {
                    return Result::Failure;
                }
	    } else {
                if (!file->print ("    ", opts->capital_header_name, "_", decl_name, " * const &self = "
                                          "static_cast <", opts->capital_header_name, "_", decl_name, "*> (__self);\n"
                                  "\n"))
                {
                    return Result::Failure;
                }
	    }

	    if (!file->print ("    self->", name_to_set))
                return Result::Failure;

	    if (any_token) {
		if (!file->print (" = static_cast <ParserElement_Token*> (___el);\n"))
                    return Result::Failure;
	    } else {
		if (phrase_part->seq) {
		    if (!file->print ("s.append (static_cast <", opts->capital_header_name, "_", type_to_set, "*> (__el));\n"))
                        return Result::Failure;
                } else {
		    if (!file->print (" = static_cast <", opts->capital_header_name, "_", type_to_set, "*> (___el);\n"))
                        return Result::Failure;
                }
	    }

	    if (!file->print ("}\n"
                              "\n"))
            {
                return Result::Failure;
            }
	}
    }

    if (!file->print ("static ParserElement*\n",
                      opts->header_name, "_", phrase_prefix, "_creation_func (VStack * const vstack /* non-null */)\n"
                      "{\n"
                      "    return new (vstack->push_malign ("
                              "sizeof  (", opts->capital_header_name, "_", phrase_prefix, "), "
                              "alignof (", opts->capital_header_name, "_", phrase_prefix, ")"
                              ")) ",
                              opts->capital_header_name, "_", phrase_prefix, ";\n"
                      "}\n"
                      "\n"))
    {
        return Result::Failure;
    }

    if (!global_grammar) {
	if (!file->print ("static "))
            return Result::Failure;
    }

    if (!file->print ("StRef<Grammar>\n"
                      "create_", opts->header_name, "_", (global_grammar ? ConstMemory ("grammar") : phrase_prefix->mem()), " ()\n"
                      "{\n"
                      "    static StRef<Grammar_Compound> grammar;\n"
                      "    if (grammar)\n"
                      "        return grammar;\n"
                      "\n"
                      "    grammar = st_grab (new (std::nothrow) Grammar_Compound (", opts->header_name, "_", phrase_prefix, "_creation_func));\n"
                      "    grammar->name = st_grab (new (std::nothrow) String (\"", phrase_prefix, "\"));\n"))
    {
        return Result::Failure;
    }

    if (has_begin) {
	if (!file->print ("    grammar->begin_func = ", opts->header_name, "_", phrase_prefix, "_begin_func;\n"))
            return Result::Failure;
    }

    if (has_match) {
	if (!file->print ("    grammar->match_func = __pargen_", opts->header_name, "_", phrase_prefix, "_match_func;\n"))
            return Result::Failure;
    }

    if (has_accept) {
	if (!file->print ("    grammar->accept_func = __pargen_", opts->header_name, "_", phrase_prefix, "_accept_func;\n"))
            return Result::Failure;
    }

    if (!file->print ("\n"))
        return Result::Failure;

    {
	List< StRef<PhrasePart> >::DataIterator phrase_part_iter (phrase->phrase_parts);
	while (!phrase_part_iter.done ()) {
	    StRef<PhrasePart> &phrase_part = phrase_part_iter.next ();

	    if (phrase_part->phrase_part_type == PhrasePart::t_Label)
		continue;

	    if (!file->print ("    {\n"
                              "        StRef<CompoundGrammarEntry> const entry = st_grab (new (std::nothrow) CompoundGrammarEntry ());\n"))
            {
                return Result::Failure;
            }

	    if (phrase_part->opt) {
		if (!file->print ("        entry->flags |= CompoundGrammarEntry::Optional;\n"))
                    return Result::Failure;
            }

	    if (phrase_part->seq) {
		if (!file->print ("        entry->flags |= CompoundGrammarEntry::Sequence;\n"))
                    return Result::Failure;
            }

	    switch (phrase_part->phrase_part_type) {
		case PhrasePart::t_Phrase: {
		    PhrasePart_Phrase * const phrase_part__phrase =
			    static_cast <PhrasePart_Phrase*> (phrase_part.ptr());

		    if (!file->print ("        entry->grammar = "
                                                       "create_", opts->header_name,
                                                       "_", phrase_part__phrase->phrase_name, " ();\n"
                                      "        entry->assignment_func = ",
                                                       opts->header_name, "_", phrase_prefix,
                                                       "_set_", phrase_part__phrase->name, ";\n"))
                    {
                        return Result::Failure;
                    }
		} break;
		case PhrasePart::t_Token: {
		    PhrasePart_Token * const phrase_part__token =
			    static_cast <PhrasePart_Token*> (phrase_part.ptr());

		    if (!file->print ("\n"
                                      "        StRef<Grammar_Immediate_SingleToken> const grammar__immediate = "
                                                       "st_grab (new (std::nothrow) Grammar_Immediate_SingleToken ("
                                                               "\"", phrase_part__token->token, "\"));\n"))
                    {
                        return Result::Failure;
                    }

		    if (phrase_part__token->token_match_cb &&
			phrase_part__token->token_match_cb->len() > 0)
		    {
			if (!file->print ("\n"
                                          "        bool ", phrase_part__token->token_match_cb, " (\n"
                                          "                ConstMemory const &token,\n"
                                          "                void              *token_user_ptr,\n"
                                          "                void              *user_data);\n"
                                          "\n"))
                        {
                            return Result::Failure;
                        }

			if (!file->print ("        grammar__immediate->token_match_cb = ",
                                                           phrase_part__token->token_match_cb, ";\n"))
                        {
                            return Result::Failure;
                        }

			if (!file->print ("        grammar__immediate->token_match_cb_name = "
                                                           "st_grab (new (std::nothrow) String ("
                                                                   "\"", phrase_part__token->token_match_cb, "\"));\n"
                                          "\n"))
                        {
                            return Result::Failure;
                        }
		    }

		    if (!file->print ("        entry->grammar = grammar__immediate;\n"))
                        return Result::Failure;

		    if (!phrase_part__token->token ||
			phrase_part__token->token->len() == 0)
		    {
			if (!file->print ("        entry->assignment_func = ",
                                                           opts->header_name, "_", phrase_prefix,
                                                           "_set_any_token;\n"))
                        {
                            return Result::Failure;
                        }
		    }
		} break;
		case PhrasePart::t_AcceptCb: {
		    PhrasePart_AcceptCb * const phrase_part__match_cb =
			    static_cast <PhrasePart_AcceptCb*> (phrase_part.ptr());

		    if (!file->print ("        entry->inline_match_func = __pargen_", phrase_part__match_cb->cb_name, ";\n"))
                        return Result::Failure;
		} break;
		case PhrasePart::t_UniversalAcceptCb: {
		    PhrasePart_UniversalAcceptCb * const phrase_part__match_cb =
			    static_cast <PhrasePart_UniversalAcceptCb*> (phrase_part.ptr());

		    if (!file->print ("        entry->inline_match_func = __pargen_", phrase_part__match_cb->cb_name, ";\n"))
                        return Result::Failure;
		} break;
		case PhrasePart::t_UpwardsAnchor: {
		    PhrasePart_UpwardsAnchor * const phrase_part__upwards_anchor =
			    static_cast <PhrasePart_UpwardsAnchor*> (phrase_part.ptr());

		    if (!file->print ("        entry->is_jump = true;\n"
                                      "        entry->jump_grammar = create_", opts->header_name, "_",
                                                       phrase_part__upwards_anchor->declaration_name, " ();\n"))
                    {
                        return Result::Failure;
                    }

		    if (phrase_part__upwards_anchor->jump_cb_name) {
			if (!file->print ("        entry->jump_cb = ", phrase_part__upwards_anchor->jump_cb_name, ";\n"))
                            return Result::Failure;
		    }

		    if (!file->print ("        entry->jump_switch_grammar_index = ",
                                                       phrase_part__upwards_anchor->switch_grammar_index, ";\n"
                                      "        entry->jump_compound_grammar_index = ",
                                                       phrase_part__upwards_anchor->compound_grammar_index, ";\n"))
                    {
                        return Result::Failure;
                    }
		} break;
		case PhrasePart::t_Label: {
                    unreachable ();
		} break;
		default:
                    unreachable ();
	    }

	    if (!file->print ("        grammar->grammar_entries.append (entry);\n"
                              "    }\n"
                              "\n"))
            {
                return Result::Failure;
            }
	}
    }

    if (!file->print ("    return grammar;\n"
                      "}\n"
                      "\n"))
    {
        return Result::Failure;
    }

  // Dumping

    if (!file->print ("void\n"
                      "dump_", opts->header_name, "_",
                              (global_grammar ? ConstMemory ("grammar") : phrase_prefix->mem()),
                              " (", opts->capital_header_name, "_", phrase_prefix,
                              " const *el", (global_grammar ? ConstMemory ("") : ConstMemory (", Size nest_level")), ")\n"
                      "{\n"
                      "    if (!el) {\n"))
    {
        return Result::Failure;
    }

    if (!global_grammar) {
	if (!file->print ("        print_tab (errs, nest_level);\n"))
            return Result::Failure;
    } else {
	if (!file->print ("        print_tab (errs, 0);\n"))
            return Result::Failure;
    }

    if (!file->print ("        errs->print (\"(null) ", phrase_prefix, "\\n\");\n"
                      "        return;\n"
                      "    }\n"
                      "\n"))
    {
        return Result::Failure;
    }

    if (!global_grammar) {
	if (!file->print ("    print_tab (errs, nest_level);\n"))
            return Result::Failure;
    }

    if (!file->print ("    errs->print (\"", phrase_prefix, "\\n\");\n"
                      "\n"))
    {
        return Result::Failure;
    }

    {
	List< StRef<PhrasePart> >::DataIterator phrase_part_iter (phrase->phrase_parts);
	while (!phrase_part_iter.done ()) {
	    StRef<PhrasePart> &phrase_part = phrase_part_iter.next ();
	    switch (phrase_part->phrase_part_type) {
		case PhrasePart::t_Phrase: {
		    PhrasePart_Phrase * const phrase_part__phrase =
                            static_cast <PhrasePart_Phrase*> (phrase_part.ptr());

		    if (phrase_part->seq) {
			if (!file->print ("    {\n"
                                          "        IntrusiveList<", opts->capital_header_name, "_",
                                                         phrase_part__phrase->decl_phrases->declaration_name,
                                                         ">::iterator sub_iter (el->",
                                                         phrase_part__phrase->name, "s);\n"
                                          "        while (!sub_iter.done ()) {\n"
                                          "            ", opts->capital_header_name, "_",
                                                               phrase_part__phrase->decl_phrases->declaration_name,
                                                               " * const sub = sub_iter.next ();\n"
                                          "            dump_", opts->header_name, "_",
                                                               phrase_part__phrase->decl_phrases->declaration_name,
                                                               " (sub, ", (global_grammar ? ConstMemory ("1") : ConstMemory ("nest_level + 1")), ");\n"
                                          "        }\n"
                                          "    }\n"
                                          "\n"))
                        {
                            return Result::Failure;
                        }
		    } else {
			if (!file->print ("    dump_", opts->header_name, "_",
                                                       phrase_part__phrase->decl_phrases->declaration_name,
                                                       " (el->", phrase_part__phrase->name, ", ",
                                                       (global_grammar ? ConstMemory ("1") : ConstMemory ("nest_level + 1")), ");\n"
                                          "\n"))
                        {
                            return Result::Failure;
                        }
		    }
		} break;
		case PhrasePart::t_Token: {
		    PhrasePart_Token * const phrase_part__token =
                            static_cast <PhrasePart_Token*> (phrase_part.ptr());

		    if (!global_grammar) {
			if (!file->print ("    print_tab (errs, nest_level + 1);\n"))
                            return Result::Failure;
                    }

		    if (!file->print ("    errs->print (\"token: ", phrase_part__token->token))
                        return Result::Failure;

		    if (phrase_part->opt || phrase_part->seq) {
			if (phrase_part->opt) {
			    if (!file->print ("_opt"))
                                return Result::Failure;
                        }

			if (phrase_part->seq) {
			    if (!file->print ("_seq"))
                                return Result::Failure;
                        }
		    }

		    if (!file->print ("\\n\");\n"))
                        return Result::Failure;
		} break;
		case PhrasePart::t_AcceptCb: {
		  // No-op
		} break;
		case PhrasePart::t_UniversalAcceptCb: {
		  // No-op
		} break;
		case PhrasePart::t_UpwardsAnchor: {
		  // No-op
		} break;
		case PhrasePart::t_Label: {
		  // No-op
		} break;
		default:
                    unreachable ();
	    }
	}
    }

    if (!file->print ("}\n"
                      "\n"))
    {
        return Result::Failure;
    }

    return Result::Success;
}

static mt_throws Result
compileSource_Alias (File                     * const mt_nonnull file,
		     Declaration_Phrases      * const mt_nonnull decl_phrases,
		     CompilationOptions const * const opts,
		     bool                       const has_begin,
		     bool                       const has_match,
		     bool                       const has_accept)
{
    Declaration const * const decl = decl_phrases;

    assert (decl_phrases->is_alias);

    bool global_grammar;
    assert (decl->declaration_name);
    if (equal (decl->declaration_name->mem(), "*"))
	global_grammar = true;
    else
	global_grammar = false;

    StRef<String> const phrase_prefix = decl->declaration_name;

    if (!global_grammar) {
	if (!file->print ("static "))
            return Result::Failure;
    }

    if (!file->print ("StRef<Grammar>\n"
                      "create_", opts->header_name, "_", (global_grammar ? ConstMemory ("grammar") : phrase_prefix->mem()), " ()\n"
                      "{\n"
                      "    static StRef<Grammar_Alias> grammar;\n"
                      "    if (grammar)\n"
                      "        return grammar;\n"
                      "\n"
                      "    grammar = st_grab (new (std::nothrow) Grammar_Alias);\n"
                      "    grammar->name = st_grab (new (std::nothrow) String (\"", phrase_prefix, "\"));\n"
                      "    grammar->aliased_grammar = create_", opts->header_name, "_", decl_phrases->aliased_name, " ();\n"
                      "\n"))
    {
        return Result::Failure;
    }

    if (has_begin) {
	if (!file->print ("    grammar->begin_func = ", opts->header_name, "_", phrase_prefix, "_begin_func;\n"))
            return Result::Failure;
    }

    if (has_match) {
	if (!file->print ("    grammar->match_func = __pargen_", opts->header_name, "_", phrase_prefix, "_match_func;\n"))
            return Result::Failure;
    }

    if (has_accept) {
	if (!file->print ("    grammar->accept_func = __pargen_", opts->header_name, "_", phrase_prefix, "_accept_func;\n"))
            return Result::Failure;
    }

    if (!file->print ("\n"
                      "    return grammar;\n"
                      "}\n"
                      "\n"))
    {
        return Result::Failure;
    }

    return Result::Success;
}

mt_throws Result
compileSource (File                     * const mt_nonnull file,
	       PargenTask const         * const mt_nonnull pargen_task,
	       CompilationOptions const * const mt_nonnull opts)
{
    assert (file && pargen_task && opts);

    if (!file->print ("#include <libmary/libmary.h>\n"
                      "\n"
                      "#include \"", opts->header_name, "_pargen.h\"\n"
                      "\n"
                      "using namespace M;\n"
                      "using namespace Pargen;\n"
                      "\n"
                      "namespace ", opts->capital_namespace_name, " {\n"
                      "\n"
                      "\n"
                      "static void\n"
                      "print_whsp (OutputStream *file,\n"
                      "            Size num_spaces)\n"
                      "{\n"
                      "    for (Size i = 0; i < num_spaces; i++)\n"
                      "        file->print (\" \");\n"
                      "}\n"
                      "\n"
                      "static void\n"
                      "print_tab (OutputStream *file,\n"
                      "           Size nest_level)\n"
                      "{\n"
                      "    print_whsp (file, nest_level * 1);\n"
                      "}\n"
                      "\n"))
    {
        return Result::Failure;
    }

    {
	List< StRef<Declaration> >::DataIterator decl_iter (pargen_task->decls);
	while (!decl_iter.done ()) {
	    StRef<Declaration> &decl = decl_iter.next ();
	    if (decl->declaration_type != Declaration::t_Phrases)
		continue;

	    if (equal (decl->declaration_name->mem(), "*"))
		continue;

	    if (!file->print ("static StRef<Grammar> create_", opts->header_name, "_", decl->declaration_name, " ();\n"))
                return Result::Failure;
	}
	if (!file->print ("\n"))
            return Result::Failure;
    }

    {
	List< StRef<Declaration> >::DataIterator decl_iter (pargen_task->decls);
	while (!decl_iter.done ()) {
	    StRef<Declaration> &decl = decl_iter.next ();

	    if (decl->declaration_type != Declaration::t_Phrases)
		continue;

	    Declaration_Phrases * const decl_phrases =
                    static_cast <Declaration_Phrases*> (decl.ptr());

	    ConstMemory decl_name;
	    ConstMemory lowercase_decl_name;
	    bool global_grammar;
	    assert (decl->declaration_name);
	    if (equal (decl->declaration_name->mem(), "*")) {
		decl_name = ConstMemory ("Grammar");
		lowercase_decl_name = ConstMemory ("grammar");
		global_grammar = true;
	    } else {
		decl_name = decl->declaration_name->mem();
		lowercase_decl_name = decl->lowercase_declaration_name->mem();
		global_grammar = false;
	    }

	    ConstMemory elem_name = decl_name;
	    if (decl_phrases->is_alias)
		elem_name = decl_phrases->deep_aliased_name->mem();

	    bool has_begin = false;
	    if (!decl_phrases->callbacks.lookup ("begin").isNull()) {
		has_begin = true;
		if (!file->print ("void ", opts->header_name, "_", decl_name, "_begin_func (\n"
                                  "        void *data);\n"
                                  "\n"))
                {
                    return Result::Failure;
                }
	    }

	    bool has_match = false;
	    if (!decl_phrases->callbacks.lookup ("match").isNull()) {
		has_match = true;
		if (!file->print ("bool ", opts->header_name, "_", decl_name, "_match_func (\n"
                                  "        ", opts->capital_header_name, "_", elem_name, " *parser_element,\n"
                                  "        Pargen::ParserControl *parser_control,\n"
                                  "        void *data);\n"
                                  "\n"
                                  "bool __pargen_", opts->header_name, "_", decl_name, "_match_func (\n"
                                  "        ParserElement *parser_element,\n"
                                  "        Pargen::ParserControl *parser_control,\n"
                                  "        void *data)\n"
                                  "{\n"
                                  "    ", opts->capital_header_name, "Element * const &_el = "
                                          "static_cast <", opts->capital_header_name, "Element*> (parser_element);\n"
                                  "    assert (_el->", opts->header_name, "_element_type == ",
                                          opts->capital_header_name, "Element::t_", elem_name, ");\n"
                                  "\n"
                                  "    ", opts->capital_header_name, "_", elem_name, " * const &el = static_cast <",
                                          opts->capital_header_name, "_", elem_name, "*> (_el);\n"
                                  "\n"
                                  "    return ", opts->header_name, "_", decl_name, "_match_func (el, parser_control, data);\n"
                                  "}\n"
                                  "\n"))
                {
                    return Result::Failure;
                }
	    }

	    bool has_accept = false;
	    if (!decl_phrases->callbacks.lookup ("accept").isNull()) {
		has_accept = true;
		if (!file->print ("void ", opts->header_name, "_", decl_name, "_accept_func (\n"
                                  "        ", opts->capital_header_name, "_", elem_name, " *parser_element,\n"
                                  "        Pargen::ParserControl *parser_control,\n"
                                  "        void *data);\n"
                                  "\n"
                                  "void __pargen_", opts->header_name, "_", decl_name, "_accept_func (\n"
                                  "        ParserElement *parser_element,\n"
                                  "        Pargen::ParserControl *parser_control,\n"
                                  "        void *data)\n"
                                  "{\n"
                                  "    if (!parser_element) {\n"
                                  "        ", opts->header_name, "_", decl_name, "_accept_func (NULL, parser_control, data);\n"
                                  "        return;\n"
                                  "    }\n"
                                  "\n"
                                  "    ", opts->capital_header_name, "Element * const &_el = "
                                          "static_cast <", opts->capital_header_name, "Element*> (parser_element);\n"
                                  "    assert (_el->", opts->header_name, "_element_type == ",
                                          opts->capital_header_name, "Element::t_", elem_name, ");\n"
                                  "\n"
                                  "    ", opts->capital_header_name, "_", elem_name, " * const &el = "
                                          "static_cast <", opts->capital_header_name, "_", elem_name, "*> (_el);\n"
                                  "\n"
                                  "    ", opts->header_name, "_", decl_name, "_accept_func (el, parser_control, data);\n"
                                  "}\n"
                                  "\n"))
                {
                    return Result::Failure;
                }
	    }

	    if (decl_phrases->phrases.getNumElements() > 1) {
		{
		    List< StRef<Declaration_Phrases::PhraseRecord> >::DataIterator phrase_iter (decl_phrases->phrases);
		    while (!phrase_iter.done ()) {
			StRef<Declaration_Phrases::PhraseRecord> &phrase_record = phrase_iter.next ();
			assert (phrase_record
				&& phrase_record->phrase
				&& phrase_record->phrase->phrase_name);
			Phrase * const phrase = phrase_record->phrase;
			compileSource_Phrase (file,
					      phrase,
					      opts,
					      decl_name,
					      lowercase_decl_name,
					      phrase->phrase_name->mem(),
					      true  /* subtype */,
					      false /* has_begin */,
					      false /* has_match */,
					      false /* has_accept */);
		    }
		}

		if (!equal (decl->declaration_name->mem(), "*")) {
		    if (!file->print ("static "))
                        return Result::Failure;
                }

		if (!file->print ("StRef<Grammar>\n"
                                  "create_", opts->header_name, "_", (global_grammar ? ConstMemory ("grammar") : decl_name), " ()\n"
                                  "{\n"
                                  "    static StRef<Grammar_Switch> grammar;\n"
                                  "    if (grammar)\n"
                                  "        return grammar;\n"
                                  "\n"
                                  "    grammar = st_grab (new (std::nothrow) Grammar_Switch ());\n"
                                  "    grammar->name = st_grab (new (std::nothrow) String (\"", decl_name, "\"));\n"))
                {
                    return Result::Failure;
                }

		if (has_begin) {
		    if (!file->print ("    grammar->begin_func = ", opts->header_name, "_", decl_name, "_begin_func;\n"))
                        return Result::Failure;
                }

		if (has_match) {
		    if (!file->print ("    grammar->match_func = __pargen_", opts->header_name, "_", decl_name, "_match_func;\n"))
                        return Result::Failure;
                }

		if (has_accept) {
		    if (!file->print ("    grammar->accept_func = __pargen_", opts->header_name, "_", decl_name, "_accept_func;\n"))
                        return Result::Failure;
                }

		if (!file->print ("\n"))
                    return Result::Failure;

		{
		    List< StRef<Declaration_Phrases::PhraseRecord> >::DataIterator phrase_iter (decl_phrases->phrases);
		    while (!phrase_iter.done ()) {
			StRef<Declaration_Phrases::PhraseRecord> &phrase_record = phrase_iter.next ();
			Phrase * const phrase = phrase_record->phrase;
			if (!file->print ("    {\n"
                                          "        StRef<SwitchGrammarEntry> const entry = st_grab (new (std::nothrow) SwitchGrammarEntry ());\n"
                                          "        entry->grammar = create_", opts->header_name, "_", decl_name, "_", phrase->phrase_name, " ();\n"))
                        {
                            return Result::Failure;
                        }

			List< StRef<String> >::DataIterator variant_iter (phrase_record->variant_names);
			while (!variant_iter.done ()) {
			    StRef<String> &variant = variant_iter.next ();
			    if (!file->print ("    entry->variants.append (st_grab (new (std::nothrow) String (\"", variant, "\")));\n"))
                                return Result::Failure;
			}

			if (!file->print ("\n"
                                          "        grammar->grammar_entries.append (entry);\n"
                                          "    }\n"
                                          "\n"))
                        {
                            return Result::Failure;
                        }
		    }
		}

		if (!file->print ("    return grammar;\n"
                                  "}\n"
                                  "\n"))
                {
                    return Result::Failure;
                }

	      // Dumping

		if (!file->print ("void\n"
                                  "dump_", opts->header_name, "_", (global_grammar ? ConstMemory ("grammar") : decl_name), " (",
                                          opts->capital_header_name, "_", decl_name, " const *el",
                                          (global_grammar ? ConstMemory() : ConstMemory (", Size nest_level")), ")\n"
                                  "{\n"
                                  "    if (!el) {\n"))
                {
                    return Result::Failure;
                }

		if (!global_grammar) {
		    if (!file->print ("        print_tab (errs, nest_level);\n"))
                        return Result::Failure;
                } else {
		    if (!file->print ("        print_tab (errs, 0);\n"))
                        return Result::Failure;
                }

		if (!file->print ("        errs->print (\"(null) ", decl_name, "\\n\");\n"
                                  "        return;\n"
                                  "    }\n"
                                  "\n"
                                  "    switch (el->", lowercase_decl_name, "_type) {\n"))
                {
                    return Result::Failure;
                }

		{
		    List< StRef<Declaration_Phrases::PhraseRecord> >::DataIterator phrase_iter (decl_phrases->phrases);
		    while (!phrase_iter.done ()) {
			StRef<Declaration_Phrases::PhraseRecord> &phrase_record = phrase_iter.next ();
			Phrase * const phrase = phrase_record->phrase;

			if (!file->print ("        case ", opts->capital_header_name, "_", decl_name, "::t_", phrase->phrase_name, ":\n"
                                          "            dump_", opts->header_name, "_", decl_name, "_", phrase->phrase_name, " ("
                                                  "static_cast <", opts->capital_header_name, "_", decl_name, "_", phrase->phrase_name, " const *> (el), ",
                                                  (global_grammar ? ConstMemory ("0") : ConstMemory ("nest_level + 0")), ");\n"
                                          "            break;\n"))
                        {
                            return Result::Failure;
                        }
		    }
		}

		if (!file->print ("        default:\n"
                                  "            unreachable ();\n"
                                  "    }\n"
                                  "}\n"
                                  "\n"))
                {
                    return Result::Failure;
                }
	    } else {
	      // The grammar consists of only one phrase.
	      // Aliases go this way.

		if (decl_phrases->is_alias) {
		    compileSource_Alias (file, decl_phrases, opts, has_begin, has_match, has_accept);
		} else {
		    if (!decl_phrases->phrases.isEmpty ()) {
			StRef<Phrase> &phrase = decl_phrases->phrases.first->data->phrase;
			assert (phrase);
			compileSource_Phrase (file,
					      phrase,
					      opts,
					      decl->declaration_name->mem(),
					      decl->lowercase_declaration_name->mem(),
					      ConstMemory(),
					      false /* subtype */,
					      has_begin,
					      has_match,
					      has_accept);
		    }
		}
	    }
	}
    }

    if (!file->print ("}\n"
                      "\n"))
    {
        return Result::Failure;
    }

    return Result::Success;
}

}

