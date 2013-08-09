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


#include <pargen/header_compiler.h>


using namespace M;

namespace Pargen {

static mt_throws Result
compileHeader_Phrase_External (File                                    * const mt_nonnull file,
			       Declaration_Phrases::PhraseRecord const * const mt_nonnull phrase_record,
			       CompilationOptions                const * const mt_nonnull opts,
			       ConstMemory                               const declaration_name)
{
    assert (file && phrase_record && opts);

    ConstMemory decl_name;
    if (equal (declaration_name, ConstMemory ("*")))
	decl_name = ConstMemory ("Grammar");
    else
	decl_name = declaration_name;

    List< StRef<PhrasePart> >::DataIterator phrase_part_iter (phrase_record->phrase->phrase_parts);
    while (!phrase_part_iter.done ()) {
	StRef<PhrasePart> &phrase_part = phrase_part_iter.next ();

	switch (phrase_part->phrase_part_type) {
	    case PhrasePart::t_Phrase: {
	      // No-op
	    } break;
	    case PhrasePart::t_Token: {
	      // No-op
	    } break;
	    case PhrasePart::t_AcceptCb: {
		PhrasePart_AcceptCb * const phrase_part__accept_cb =
			static_cast <PhrasePart_AcceptCb*> (phrase_part.ptr());

		if (phrase_part__accept_cb->repetition)
		    continue;

                if (!file->print ("bool ", phrase_part__accept_cb->cb_name, " (\n"
                                  "        ", opts->capital_header_name, "_", decl_name, " *parser_element,\n"
                                  "        Pargen::ParserControl *parser_control,\n"
                                  "        void *data);\n"
                                  "\n"))
                {
                    return Result::Failure;
                }
	    } break;
	    case PhrasePart::t_UniversalAcceptCb: {
		PhrasePart_UniversalAcceptCb * const phrase_part__universal_accept_cb =
			static_cast <PhrasePart_UniversalAcceptCb*> (phrase_part.ptr());

		if (phrase_part__universal_accept_cb->repetition)
		    continue;

                if (!file->print ("bool ", phrase_part__universal_accept_cb->cb_name, " (\n"
                                  "        ", opts->capital_header_name, "Element *parser_element,\n"
                                  "        Pargen::ParserControl *parser_control,\n"
                                  "        void *data);\n"
                                  "\n"))
                {
                    return Result::Failure;
                }
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

    return Result::Success;
}

static mt_throws Result
compileHeader_Phrase (File                                    * const mt_nonnull file,
		      Declaration_Phrases::PhraseRecord const * const mt_nonnull phrase_record,
		      CompilationOptions                const * const mt_nonnull opts,
		      bool                                      const initializers_mode = false)
{
    assert (file && phrase_record && opts);

    List< StRef<PhrasePart> >::DataIterator phrase_part_iter (phrase_record->phrase->phrase_parts);
    while (!phrase_part_iter.done ()) {
	StRef<PhrasePart> &phrase_part = phrase_part_iter.next ();

	switch (phrase_part->phrase_part_type) {
	    case PhrasePart::t_Phrase: {
		PhrasePart_Phrase * const phrase_part__phrase =
                        static_cast <PhrasePart_Phrase*> (phrase_part.ptr());

		if (phrase_part__phrase->seq) {
		    if (!initializers_mode) {
                        if (!file->print ("    M::IntrusiveList<", opts->capital_header_name, "_",
                                          phrase_part__phrase->decl_phrases->declaration_name, "> ",
                                          phrase_part__phrase->name, "s;\n"))
                        {
                            return Result::Failure;
                        }
		    }
		} else {
		    if (initializers_mode) {
			if (!file->print ("          , ", phrase_part__phrase->name, " (NULL)\n")) {
                            return Result::Failure;
                        }
		    } else {
			if (!file->print ("    ", opts->capital_header_name, "_",
                                          phrase_part__phrase->decl_phrases->declaration_name, " *",
                                          phrase_part__phrase->name, ";\n"))
                        {
                            return Result::Failure;
                        }
		    }
		}
	    } break;
	    case PhrasePart::t_Token: {
		PhrasePart_Token * const phrase_part__token =
                        static_cast <PhrasePart_Token*> (phrase_part.ptr());

		if (!phrase_part__token->token ||
		    phrase_part__token->token->len() == 0)
		{
		    if (initializers_mode) {
                        if (!file->print ("          , any_token (NULL)\n"))
                            return Result::Failure;
		    } else {
                        if (!file->print ("    Pargen::ParserElement_Token *any_token;\n"))
                            return Result::Failure;
		    }
		}
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

    return Result::Success;
}

mt_throws Result
compileHeader (File                     * const mt_nonnull file,
	       PargenTask         const * const mt_nonnull pargen_task,
	       CompilationOptions const * const mt_nonnull opts)
{
    assert (file && pargen_task && opts);

    if (!file->print ("#ifndef __", opts->all_caps_module_name, "__", opts->all_caps_header_name, "_PARGEN__H__\n"
                      "#define __", opts->all_caps_module_name, "__", opts->all_caps_header_name, "_PARGEN__H__\n"
                      "\n"
                      "\n"
                      "#include <libmary/libmary.h>\n"
                      "\n"
                      "#include <pargen/parser_element.h>\n"
                      "#include <pargen/grammar.h>\n"
                      "\n"
                      "\n"
                      "namespace ", opts->capital_namespace_name, " {\n"
                      "\n"
                      "class ", opts->capital_header_name, "Element : public Pargen::ParserElement, public M::IntrusiveListElement<>\n"
                      "{\n"
                      "public:\n"
                      "    enum Type {"))
    {
        return Result::Failure;
    }

    Bool got_global_grammar;
    {
	List< StRef<Declaration> >::DataIterator decl_iter (pargen_task->decls);
	Bool decl_iter_started;
	while (!decl_iter.done ()) {
	    StRef<Declaration> &_decl = decl_iter.next ();
	    if (_decl->declaration_type != Declaration::t_Phrases)
		continue;

	    Declaration_Phrases const * const decl =
                    static_cast <Declaration_Phrases const *> (_decl.ptr());

	    if (decl->is_alias)
		continue;

	    if (decl_iter_started) {
                if (!file->print (",\n"))
                    return Result::Failure;
            } else {
		decl_iter_started = true;
                if (!file->print ("\n"))
                    return Result::Failure;
	    }

	    if (equal (decl->declaration_name->mem(), "*")) {
		got_global_grammar = true;
                if (!file->print ("        t_Grammar"))
                    return Result::Failure;
	    } else {
                if (!file->print ("        t_", decl->declaration_name))
                    return Result::Failure;
            }
	}
    }

    if (!file->print ("\n"
                      "    };\n"
                      "\n"
                      "    const Type ", opts->header_name, "_element_type;\n"
                      "\n"
                      "    ", opts->capital_header_name, "Element (Type type)\n"
                      "        : ", opts->header_name, "_element_type (type)\n"
                      "    {\n"
                      "    }\n"
                      "};\n"
                      "\n"))
    {
        return Result::Failure;
    }

    {
	List< StRef<Declaration> >::DataIterator decl_iter (pargen_task->decls);
	while (!decl_iter.done ()) {
	    StRef<Declaration> &_decl = decl_iter.next ();
	    if (_decl->declaration_type != Declaration::t_Phrases)
		continue;

	    Declaration_Phrases const * const decl =
                    static_cast <Declaration_Phrases const *> (_decl.ptr()); 

	    if (decl->is_alias)
		continue;

	    if (equal (decl->declaration_name->mem(), "*"))
		continue;

            if (!file->print ("class ", opts->capital_header_name, "_", decl->declaration_name, ";\n"))
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

	    Declaration_Phrases const * const &decl_phrases =
                    static_cast <Declaration_Phrases const *> (decl.ptr());

	    if (decl_phrases->is_alias)
		continue;

	    ConstMemory decl_name;
	    ConstMemory lowercase_decl_name;
	    if (equal (decl->declaration_name->mem(), "*")) {
		decl_name = ConstMemory ("Grammar");
		lowercase_decl_name = ConstMemory ("grammar");
	    } else {
		decl_name = decl->declaration_name->mem();
		lowercase_decl_name = decl->lowercase_declaration_name->mem();
	    }

            if (!compileHeader_Phrase_External (file,
                                                decl_phrases->phrases.first->data,
                                                opts,
                                                decl_name))
            {
                return Result::Failure;
            }

            if (!file->print ("class ", opts->capital_header_name, "_", decl_name, " : "
                                      "public ", opts->capital_header_name, "Element\n"
                              "{\n"
                              "public:\n"))
            {
                return Result::Failure;
            }

	    if (decl_phrases->phrases.getNumElements () > 1) {
		if (!file->print ("    enum Type {"))
                    return Result::Failure;

		{
		    Size phrase_index = 0;
		    List< StRef<Declaration_Phrases::PhraseRecord> >::DataIterator phrase_iter (decl_phrases->phrases);
		    Bool phrase_iter_started;
		    while (!phrase_iter.done ()) {
			StRef<Declaration_Phrases::PhraseRecord> &phrase_record = phrase_iter.next ();
			Phrase * const phrase = phrase_record->phrase;

			if (phrase_iter_started) {
			    if (!file->print (",\n"))
                                return Result::Failure;
                        } else {
			    phrase_iter_started = true;
			    if (!file->print ("\n"))
                                return Result::Failure;
			}

			StRef<String> phrase_name;
			if (!phrase->phrase_name)
			    phrase_name = st_makeString ("phrase", phrase_index);
			else
			    phrase_name = phrase->phrase_name;

                        if (!file->print ("        t_", phrase_name))
                            return Result::Failure;
		    }
		}

                if (!file->print ("\n"
                                  "    };\n"
                                  "\n"
                                  "    const Type ", lowercase_decl_name, "_type;\n"
                                  "\n"
                                  "    ", opts->capital_header_name, "_", decl_name, " (Type type)\n"
                                  "        : ", opts->capital_header_name, "Element (",
                                                      opts->capital_header_name, "Element::t_", decl_name, "),\n"
                                  "          ", lowercase_decl_name, "_type (type)\n"
                                  "    {\n"
                                  "    }\n"
                                  "};\n"
                                  "\n"))
                {
                    return Result::Failure;
                }

		{
		    List< StRef<Declaration_Phrases::PhraseRecord> >::DataIterator phrase_iter (decl_phrases->phrases);
		    Size phrase_index = 0;
		    while (!phrase_iter.done ()) {
			StRef<Declaration_Phrases::PhraseRecord> &phrase_record = phrase_iter.next ();
			Phrase * const phrase = phrase_record->phrase;

			StRef<String> phrase_name;
			if (!phrase->phrase_name) {
			    phrase_name = st_makeString ("phrase", phrase_index);
			    phrase_index ++;
			} else {
			    phrase_name = phrase->phrase_name;
                        }

			if (!compileHeader_Phrase_External (file, phrase_record, opts, decl_name))
                            return Result::Failure;

                        if (!file->print ("class ", opts->capital_header_name, "_", decl_name, "_",
                                                  phrase_name, " : "
                                                  "public ", opts->capital_header_name, "_", decl_name, "\n"
                                          "{\n"
                                          "public:\n"))
                        {
                            return Result::Failure;
                        }

                        if (!compileHeader_Phrase (file, phrase_record, opts))
                            return Result::Failure;

                        if (!file->print ("\n"
                                          "    ", opts->capital_header_name, "_", decl_name, "_", phrase_name, " ()\n"
                                          "        : ", opts->capital_header_name, "_", decl_name, " (",
                                                           opts->capital_header_name, "_", decl_name, "::"
                                                               "t_", phrase_name, ")\n"))
                        {
                            return Result::Failure;
                        }

			if (!compileHeader_Phrase (file, phrase_record, opts, true /* initializers_mode */))
                            return Result::Failure;

                        if (!file->print ("    {\n"
                                          "    }\n"
                                          "};\n"
                                          "\n"))
                        {
                            return Result::Failure;
                        }
		    }
		}
	    } else {
		if (decl_phrases->phrases.first != NULL) {
                    if (!compileHeader_Phrase (file, decl_phrases->phrases.first->data, opts))
                        return Result::Failure;
                }

                if (!file->print ("\n"
                                  "    ", opts->capital_header_name, "_", decl_name, " ()\n"
                                  "        : ", opts->capital_header_name, "Element (",
                                                   opts->capital_header_name, "Element::t_", decl_name, ")\n"))
                {
                    return Result::Failure;
                }

		if (decl_phrases->phrases.first != NULL) {
                    if (!compileHeader_Phrase (file,
                                               decl_phrases->phrases.first->data,
                                               opts,
                                               true /* initializers_mode */))
                    {
                        return Result::Failure;
                    }
                }

                if (!file->print ("    {\n"
                                  "    }\n"
                                  "};\n"
                                  "\n"))
                {
                    return Result::Failure;
                }
	    }

	    if (!equal (decl->declaration_name->mem(), "*")) {
		if (!file->print ("void dump_", opts->header_name, "_", decl->declaration_name, " (",
                                          opts->capital_header_name, "_", decl->declaration_name, " const *, "
                                          "M::Size nest_level = 0);\n\n"))
                {
                    return Result::Failure;
                }
	    }
	}
    }

    if (got_global_grammar) {
        if (!file->print ("M::StRef<Pargen::Grammar> create_", opts->header_name, "_grammar ();\n"
                          "\n"
                          "void dump_", opts->header_name, "_grammar (", opts->capital_header_name, "_Grammar const *el);\n"
                          "\n"))
        {
            return Result::Failure;
        }
    }

    if (!file->print ("}\n"
                      "\n"
                      "\n"
                      "#endif /* __", opts->all_caps_module_name, "__", opts->all_caps_header_name, "_PARGEN__H__ */\n"
                      "\n"))
    {
        return Result::Failure;
    }

    return Result::Success;
}

}

