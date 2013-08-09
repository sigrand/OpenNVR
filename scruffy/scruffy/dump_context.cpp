/*  Scruffy - C/C++ parser and source code analyzer
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


#include <mycpp/io.h>

#include <scruffy/cpp_util.h>

#include <scruffy/dump_context.h>


#define DEBUG(a) ;

#define FUNC_NAME(a) ;


using namespace MyCpp;

namespace Scruffy {
namespace Cpp {

void
DumpContext::outTab (Size nest_level)
{
    for (Size i = 0; i < nest_level * tab_size; i++)
	file->out (" ");
}

void
DumpContext::outNewline ()
{
    file->out ("\n");
}

void
DumpContext::outNewlineTab (Size nest_level)
{
    outNewline ();
    outTab (nest_level);
}

#if 0
void
Dump

void
DumpContext::dumpFunctionRetType (Cpp::TypeDesc const * const type_desc,
				  Cpp::TypeDesc const * const func_type_desc,
				  Cpp::Name     const * const func_name,
				  Size                  const nest_level)
{
    dumpDeclaration ();
}
#endif

namespace {

#if 0
// Unused
static bool
got_prefix_type_atoms (List< Ref<Cpp::TypeDesc::TypeAtom> >::Element const *cur_el)
{
    while (cur_el != NULL) {
	Ref<Cpp::TypeDesc::TypeAtom> const &type_atom = cur_el->data;
	switch (type_atom->type_atom_type) {
	    case Cpp::TypeDesc::TypeAtom::t_Pointer: {
		return true;
	    } break;
	    case Cpp::TypeDesc::TypeAtom::t_PointerToMember: {
		return true;
	    } break;
	    case Cpp::TypeDesc::TypeAtom::t_Array: {
	    } break;
	    default:
		abortIfReached ();
	}

	cur_el = cur_el->next;
    }

    return false;
}
#endif

static bool
type_atom_is_prefix (Cpp::TypeDesc::TypeAtom const &type_atom)
{
    switch (type_atom.type_atom_type) {
	case Cpp::TypeDesc::TypeAtom::t_Pointer: {
	    return true;
	} break;
	case Cpp::TypeDesc::TypeAtom::t_PointerToMember: {
	    return true;
	} break;
	case Cpp::TypeDesc::TypeAtom::t_Array: {
	    return false;
	} break;
	default:
	    abortIfReached ();
    }

    return false;
}

}

void
DumpContext::dumpTypeSpecifier (TypeDesc * const type_desc)
{
    switch (type_desc->getType ()) {
	case Cpp::TypeDesc::t_BasicType: {
	    Cpp::TypeDesc_BasicType const * const type_desc__basic_type =
		    static_cast <TypeDesc_BasicType const *> (type_desc);

	    if (type_desc->is_const)
		file->out ("const ");

	    if (type_desc->is_volatile)
		file->out ("volatile ");

#if 0
// TODO Object::*

	    if (type_desc__data->is_auto)
		file->out ("auto ");

	    if (type_desc__data->is_register)
		file->out ("register ");

	    if (type_desc__data->is_static)
		file->out ("static ");

	    if (type_desc__data->is_extern)
		file->out ("extern ");

	    if (type_desc__data->is_mutable)
		file->out ("mutable ");
#endif

	    switch (type_desc__basic_type->basic_type) {
		case TypeDesc_BasicType::Char: {
		    file->out ("char");
		} break;
		case TypeDesc_BasicType::SignedChar: {
		    file->out ("signed char");
		} break;
		case TypeDesc_BasicType::UnsignedChar: {
		    file->out ("unsigned char");
		} break;
		case TypeDesc_BasicType::ShortInt: {
		    file->out ("short int");
		} break;
		case TypeDesc_BasicType::Int: {
		    file->out ("int");
		} break;
		case TypeDesc_BasicType::LongInt: {
		    file->out ("long int");
		} break;
		case TypeDesc_BasicType::UnsignedShortInt: {
		    file->out ("unsigned short int");
		} break;
		case TypeDesc_BasicType::UnsignedInt: {
		    file->out ("unsigned int");
		} break;
		case TypeDesc_BasicType::UnsignedLongInt: {
		    file->out ("unsigned long int");
		} break;
		case TypeDesc_BasicType::WcharT: {
		    file->out ("wchar_t");
		} break;
		case TypeDesc_BasicType::Bool: {
		    file->out ("bool");
		} break;
		case TypeDesc_BasicType::Float: {
		    file->out ("float");
		} break;
		case TypeDesc_BasicType::Double: {
		    file->out ("double");
		} break;
		case TypeDesc_BasicType::LongDouble: {
		    file->out ("long double");
		} break;
		case TypeDesc_BasicType::Void: {
		    file->out ("void");
		} break;
		default:
		    abortIfReached ();
	    }
	} break;
	case TypeDesc::t_Class: {
	    TypeDesc_Class const * const type_desc__class =
		    static_cast <TypeDesc_Class const *> (type_desc);

	    if (type_desc->is_const)
		file->out ("const ");

	    if (type_desc->is_volatile)
		file->out ("volatile ");

	    file->out ("class ");

	    // TODO
	    file->out ("classTS ");
	    abortIfReached ();
	} break;
	case TypeDesc::t_Enum: {
	    TypeDesc_Enum const * const type_desc__enum =
		    static_cast <TypeDesc_Enum const *> (type_desc);

	    if (type_desc->is_const)
		file->out ("const ");

	    if (type_desc->is_volatile)
		file->out ("volatile ");

	    // TODO
	    file->out ("ENUM ");
	} break;
	default:
	    abortIfReached ();
    }
}

void
DumpContext::dumpClass (Class * const class_, 
			Size          nest_level)
{
    abortIf (class_ == NULL);

    {
	List< Ref<Cpp::Class::ParentEntry> >::DataIterator iter (class_->parents);
	while (!iter.done ()) {
	    Ref<Cpp::Class::ParentEntry> &parent_entry = iter.next ();
	    abortIf (parent_entry.isNull ());

	    file->out ("[PARENT]");
	}
    }

    outNewline ();
    file->out ("{");
    outNewline ();

    {
	// TODO dump types

	Member::MemberMap::DataIterator iter (class_->members);
	while (!iter.done ()) {
	    Ref<Cpp::Member> &object_member = iter.next ();
	    abortIf (object_member.isNull ());

//	    errf->print ("--- calling dumpObject()").pendl ();
	    dumpObject (object_member, nest_level + 1);
//	    errf->print ("--- dumpObject() returned").pendl ();

	    outNewline ();
	}
    }

    file->out ("} ");
    outNewline ();
}

void
DumpContext::dumpInitializer (Initializer * const initializer)
{
    if (initializer == NULL)
	return;

    switch (initializer->getType ()) {
	case Initializer::t_Assignment: {
	    Initializer_Assignment const * const initializer__assignment =
		    static_cast <Initializer_Assignment const *> (initializer);

	    file->out (" = ");
	    dumpExpression (initializer__assignment->expression);
	} break;
	case Initializer::t_Constructor: {
	    Initializer_Constructor const * const initializer__constructor =
		    static_cast <Initializer_Constructor const *> (initializer);

	    file->out (" (");

	    {
		List< Ref<Expression> >::DataIterator iter (initializer__constructor->expressions);
		while (!iter.done ()) {
		    Ref<Expression> &expression = iter.next ();
		    dumpExpression (expression);

		    if (!iter.done ())
			file->out (", ");
		}
	    }

	    file->out (")");
	} break;
	case Initializer::t_InitializerList: {
	    Initializer_InitializerList const * const initializer__initializer_list =
		    static_cast <Initializer_InitializerList const *> (initializer);

	    file->out (" = {");

	    {
		List< Ref<Expression> >::DataIterator iter (initializer__initializer_list->expressions);
		while (!iter.done ()) {
		    Ref<Expression> &expression = iter.next ();
		    dumpExpression (expression);

		    if (!iter.done ())
			file->out (", ");
		}
	    }

	    file->out ("}");
	} break;
	default:
	    abortIfReached ();
    }
}


class DumpContext::DumpDeclaration_Data
{
public:
    List<Cpp::TypeDesc*> inner_type_descs;
};

void
DumpContext::dumpName (Cpp::Name * const name)
{
    if (name->is_destructor)
	file->out ("~");

    if (name->is_operator) {
	file->out ("operator ");
	// TODO operators
    }

    file->out (name->name_str);

    if (name->is_template) {
	file->out (" < ");

	List< Ref<Cpp::TemplateArgument> >::DataIterator arg_iter (name->template_arguments);
	while (!arg_iter.done ()) {
	    Ref<Cpp::TemplateArgument> &template_argument = arg_iter.next ();

	    switch (template_argument->getType ()) {
		case Cpp::TemplateArgument::t_Type: {
		    Cpp::TemplateArgument_Type * const template_argument__type =
			    static_cast <TemplateArgument_Type*> (template_argument.ptr ());

		    dumpDeclaration (template_argument__type->type_desc, NULL);
		} break;
		case Cpp::TemplateArgument::t_Expression: {
		    TemplateArgument_Expression * const template_argument__expression =
			    static_cast <TemplateArgument_Expression*> (template_argument.ptr ());

		    dumpExpression (template_argument__expression->expression);
		} break;
		default:
		    abortIfReached ();
	    }

	    if (!arg_iter.done ())
		file->out (", ");
	}

	file->out (" > ");
    }
}

void
DumpContext::dumpMember (Cpp::Member      const &member,
			 Size             const nest_level,
			 Cpp::Container * const nested_name_container)
{
//    errf->print ("--- DumpContext::dumpMember ---").pendl ();

    {
	bool is_template = false;
	List< Ref<Member> > const *template_parameters = NULL;

	switch (member.getType ()) {
	    case Member::t_Type: {
		TypeDesc const * const type_desc = member.type_desc;
		switch (type_desc->getType ()) {
		    case TypeDesc::t_Class: {
			TypeDesc_Class const * const type_desc__class =
				static_cast <TypeDesc_Class const *> (type_desc);

			Class const * const class_ = type_desc__class->class_;

			is_template = class_->is_template;
			template_parameters = &class_->template_parameters;
		    } break;
		    default:
		      // Nothing to do
			;
		}
	    } break;
	    case Member::t_Function: {
		Member_Function const * const member__function =
			static_cast <Member_Function const *> (&member);
		Function const * const function = member__function->function;

		is_template = function->is_template;
		template_parameters = &function->template_parameters;
	    } break;
	    case Member::t_TypeTemplateParameter: {
		Member_TypeTemplateParameter const * const member__type_template_parameter =
			static_cast <Member_TypeTemplateParameter const *> (&member);

		is_template = member__type_template_parameter->is_template;
		template_parameters = &member__type_template_parameter->template_parameters;
	    } break;
	    default:
	      // Nothing to do
		;
	}

	if (is_template) {
	    file->out ("template < ");

	    List< Ref<Member> >::DataIterator param_member_iter (*template_parameters);
	    while (!param_member_iter.done ()) {
		Ref<Member> &param_member = param_member_iter.next ();

		// TODO nested-name-specifiers for members from foreign namespaces
		dumpMember (*param_member, nest_level, NULL /* nested-name-container */);

		if (!param_member_iter.done ())
		    file->out (", ");
	    }

	    file->out (" > ");
	}
    }

    if (member.getType () == Member::t_TypeTemplateParameter) {
//	Member_TypeTemplateParameter const * const member__type_template_parameter =
//		static_cast <Member_TypeTemplateParameter const *> (&member);

	file->out ("class ").out (member.name->toString ());

	return;
    }

    if (member.getType () == Member::t_Type) {
	Member_Type const * const member__type = static_cast <Member_Type const *> (&member);
	if (member__type->is_typedef)
	    file->out ("typedef ");
    }

    DumpDeclaration_Data data;
    do_dumpDeclaration (member.type_desc,
			nested_name_container,
			member.name,
			nest_level,
			false /* after_ret_type */,
			true  /* first_type_atom */,
			NULL  /* cur_type_atom */,
			true  /* expand_definitions */,
			&data);
}

void
DumpContext::dumpType (Cpp::Member * const member,
		       Size          const nest_level)
{
  FUNC_NAME (
    static char const * const _func_name = "Cpp.DumpContext.dumpType";
  )

    abortIf (member == NULL              ||
	     member->type_desc.isNull () ||
	     member->name.isNull ());

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    // TODO is_typedef

    DumpDeclaration_Data data;
    do_dumpDeclaration (member->type_desc,
			NULL  /* nested_name_container */,
			member->name,
			nest_level,
			false /* after_ret_type */,
			true  /* first_type_atom */,
			NULL  /* cur_type_atom */,
			true  /* expand_definitions */,
			&data);
}

void
DumpContext::dumpClassNestedNameSpecifier (Container * const dst_container)
{
    List<Class*> class_containers;
    {
      // Filling 'class_containers'

	Container *cur_container = dst_container;
	while (cur_container != NULL) {
	    if (cur_container->getType () != Container::t_Class)
		break;

	    class_containers.append (static_cast <Class*> (cur_container));

	    cur_container = cur_container->parent_container;
	}
    }

    List<Class*>::DataIterator class_iter (class_containers);
    while (!class_iter.done ()) {
	Class * const class_ = class_iter.next ();

	// TODO template names

	abortIf (class_->name.isNull ());
	file->out (class_->name->toString ()).out ("::");
    }
}

// Note: 'nest_level' is unused.
void
DumpContext::dumpDeclaration (Cpp::TypeDesc * const type_desc,
			      Cpp::Name     * const name,
			      Size            const nest_level)
{
    DumpDeclaration_Data data;
    do_dumpDeclaration (type_desc,
			NULL  /* nested_name_container */,
			name,
			nest_level,
			false /* after_ret_type */,
			true  /* first_type_atom */,
			NULL  /* cur_type_atom */,
			false /* expand_definitions */,
			&data);
}

void
DumpContext::do_dumpDeclaration (Cpp::TypeDesc        * const type_desc,
				 Cpp::Container       * const nested_name_container,
				 Cpp::Name            * const name,
				 Size                   const nest_level,
				 Bool                   const after_ret_type,
				 Bool                   const first_type_atom,
				 List< Ref<Cpp::TypeDesc::TypeAtom> >::Element const * const cur_type_atom,
				 Bool                   const expand_definitions,
				 DumpDeclaration_Data * const data)
{
    abortIf (type_desc == NULL ||
	     data == NULL);

    if (!after_ret_type) {
	abortIf (cur_type_atom != NULL);

//	dumpTypeSpecifier (type_desc);

	switch (type_desc->getType ()) {
	    case Cpp::TypeDesc::t_BasicType: {
		Cpp::TypeDesc_BasicType const * const type_desc__basic_type =
			static_cast <TypeDesc_BasicType const *> (type_desc);

		if (type_desc->is_const)
		    file->out ("const ");

		if (type_desc->is_volatile)
		    file->out ("volatile ");

#if 0
// TODO Object::*

		if (type_desc__data->is_auto)
		    file->out ("auto ");

		if (type_desc__data->is_register)
		    file->out ("register ");

		if (type_desc__data->is_static)
		    file->out ("static ");

		if (type_desc__data->is_extern)
		    file->out ("extern ");

		if (type_desc__data->is_mutable)
		    file->out ("mutable ");
#endif

		switch (type_desc__basic_type->basic_type) {
		    case TypeDesc_BasicType::Char: {
			file->out ("char");
		    } break;
		    case TypeDesc_BasicType::SignedChar: {
			file->out ("signed char");
		    } break;
		    case TypeDesc_BasicType::UnsignedChar: {
			file->out ("unsigned char");
		    } break;
		    case TypeDesc_BasicType::ShortInt: {
			file->out ("short int");
		    } break;
		    case TypeDesc_BasicType::Int: {
			file->out ("int");
		    } break;
		    case TypeDesc_BasicType::LongInt: {
			file->out ("long int");
		    } break;
		    case TypeDesc_BasicType::UnsignedShortInt: {
			file->out ("unsigned short int");
		    } break;
		    case TypeDesc_BasicType::UnsignedInt: {
			file->out ("unsigned int");
		    } break;
		    case TypeDesc_BasicType::UnsignedLongInt: {
			file->out ("unsigned long int");
		    } break;
		    case TypeDesc_BasicType::WcharT: {
			file->out ("wchar_t");
		    } break;
		    case TypeDesc_BasicType::Bool: {
			file->out ("bool");
		    } break;
		    case TypeDesc_BasicType::Float: {
			file->out ("float");
		    } break;
		    case TypeDesc_BasicType::Double: {
			file->out ("double");
		    } break;
		    case TypeDesc_BasicType::LongDouble: {
			file->out ("long double");
		    } break;
		    case TypeDesc_BasicType::Void: {
			file->out ("void");
		    } break;
		    default:
			abortIfReached ();
		}
	    } break;
	    case TypeDesc::t_Class: {
		TypeDesc_Class const * const type_desc__class =
			static_cast <TypeDesc_Class const *> (type_desc);

		if (type_desc->is_const)
		    file->out ("const ");

		if (type_desc->is_volatile)
		    file->out ("volatile ");

#if 0
		file->out ("classDDD ");

		if (expand_definitions &&
		    !type_desc__class->class_.isNull ())
		{
		    file->out ("class def ");
		    dumpClass (type_desc__class->class_, nest_level);
		} else
#endif
		file->out ("class ").out (type_desc__class->class_->name->toString ());
	    } break;
	    case TypeDesc::t_Enum: {
		TypeDesc_Enum const * const type_desc__enum =
			static_cast <TypeDesc_Enum const *> (type_desc);

		if (type_desc->is_const)
		    file->out ("const ");

		if (type_desc->is_volatile)
		    file->out ("volatile ");

		// TODO
		file->out ("ENUM ");
	    } break;
	    case Cpp::TypeDesc::t_Function: {
		Cpp::TypeDesc_Function * const type_desc__function =
			static_cast <TypeDesc_Function*> (type_desc);

		if (!type_desc__function->return_type.isNull ()) {
		    data->inner_type_descs.append (type_desc__function);

		    do_dumpDeclaration (type_desc__function->return_type,
					nested_name_container,
					name,
					nest_level,
					false /* after_ret_type */,
					true  /* first_type_atom */,
					NULL  /* cur_type_atom */,
					false /* expand_definitions */,
					data);
		    return;
		}
	    } break;
	    default:
		abortIfReached ();
	}
    } // if (!after_ret_type)

    bool need_braces = false;
    if (type_desc->getType () == Cpp::TypeDesc::t_Function) {
	if (!type_desc->type_atoms.isEmpty ()) {
	    if (type_atom_is_prefix (*type_desc->type_atoms.last->data))
		need_braces = true;
	}
    } else {
	if (after_ret_type) {
	    if (cur_type_atom != NULL) {
		if (type_atom_is_prefix (*cur_type_atom->data))
		    need_braces = true;
	    } else {
		if (type_desc->is_reference)
		    need_braces = true;
	    }
	}
    }

    if (need_braces)
	file->out (" (");

    if (first_type_atom || cur_type_atom != NULL) {
	List< Ref<Cpp::TypeDesc::TypeAtom> >::Element const *cur_el = cur_type_atom;
	if (first_type_atom)
	    cur_el = type_desc->type_atoms.last;

	while (cur_el != NULL) {
	    Ref<Cpp::TypeDesc::TypeAtom> const &type_atom = cur_el->data;

	    switch (type_atom->type_atom_type) {
		case Cpp::TypeDesc::TypeAtom::t_Pointer: {
		    TypeDesc::TypeAtom_Pointer * const type_atom__pointer =
			    static_cast <TypeDesc::TypeAtom_Pointer*> (type_atom.ptr ());

		    file->out ("*");

		    if (type_atom__pointer->is_const)
			file->out (" const ");

		    if (type_atom__pointer->is_volatile)
			file->out (" volatile ");
		} break;
		case Cpp::TypeDesc::TypeAtom::t_PointerToMember: {
		    // TODO const/volatile?

		    file->out (" .* ");
		} break;
		case Cpp::TypeDesc::TypeAtom::t_Array: {
		    do_dumpDeclaration (type_desc,
					nested_name_container,
					name,
					nest_level,
					true  /* after_ret_type */,
					false /* first_type_atom */,
					cur_el->previous,
					false /* expand_definitions */,
					data);

		    // TODO Array subscript.
		    file->out (" []");

		    if (need_braces)
			file->out (")");

		    return;
		} break;
		default:
		    abortIfReached ();
	    }

	    cur_el = cur_el->previous;
	}
    }

    if (!data->inner_type_descs.isEmpty ()) {
	Cpp::TypeDesc * const inner_type_desc = data->inner_type_descs.last->data;
	data->inner_type_descs.remove (data->inner_type_descs.last);

	do_dumpDeclaration (inner_type_desc,
			    nested_name_container,
			    name,
			    nest_level,
			    true  /* after_ret_type */,
			    true  /* first_type_atom */,
			    NULL  /* cur_type_atom */,
			    false /* expand_definitions */,
			    data);
    } else {
	if (name != NULL && !need_braces)
	    file->out (" ");

	if (type_desc->is_reference)
	    file->out ("&");

	if (name != NULL) {
	    if (nested_name_container != NULL)
		dumpClassNestedNameSpecifier (nested_name_container);

//	    file->out (name->toString ());
	    dumpName (name);
	}
    }

    if (need_braces)
	file->out (")");

    if (type_desc->getType () == Cpp::TypeDesc::t_Function) {
	Cpp::TypeDesc_Function const * const type_desc__function =
		static_cast <TypeDesc_Function const *> (type_desc);

	file->out (" (");

	{
	    List< Ref<Cpp::Member> >::DataIterator iter (type_desc__function->parameters);
	    bool first = true;
	    while (!iter.done ()) {
		Ref<Cpp::Member> &function_parameter = iter.next ();

		if (!first) {
		    file->out (", ");
		} else
		    first = false;

		dumpDeclaration (function_parameter->type_desc,
				 function_parameter->name,
				 nest_level);

		switch (function_parameter->getType ()) {
		    case Cpp::Member::t_Object: {
			Cpp::Member_Object const * const member__object =
				static_cast <Cpp::Member_Object const *> (function_parameter.ptr ());

			dumpInitializer (member__object->initializer);
		    } break;
		    case Cpp::Member::t_DependentObject: {
			Cpp::Member_DependentObject const * const member__dependent_object =
				static_cast <Cpp::Member_DependentObject const *> (function_parameter.ptr ());

			dumpInitializer (member__dependent_object->initializer);
		    } break;
		    case Cpp::Member::t_Function: {
			// TODO inlines, virtuals, explicits (explicits ?)
		    } break;
		    default:
			abortIfReached ();
		}
	    }
	}

	file->out (")");

	if (type_desc->is_const)
	    file->out ("const ");

	if (type_desc->is_volatile)
	    file->out ("volatile ");
    }
}

void
DumpContext::dumpObject (Member  * const member,
			 Size      const nest_level)
{
    abortIf (member == NULL);

    dumpDeclaration (member->type_desc, member->name, nest_level);

    switch (member->getType ()) {
	case Cpp::Member::t_Object: {
	    Cpp::Member_Object const * const member__object =
		    static_cast <Cpp::Member_Object const *> (member);

	    dumpInitializer (member__object->initializer);
	} break;
	case Cpp::Member::t_DependentObject: {
	    Cpp::Member_DependentObject const * const member__dependent_object =
		    static_cast <Cpp::Member_DependentObject const *> (member);

	    dumpInitializer (member__dependent_object->initializer);
	} break;
	default:
	  // No-op
	    ;
    }

    if (member->getType () == Member::t_Function) {
	Member_Function const * const member__function =
		static_cast <Member_Function const *> (member);

	if (!member__function->function.isNull () &&
	     member__function->function->got_definition)
	{
	    outNewline ();
//	    errf->print ("--- dumpObject(): dumping function body").pendl ();
	    dumpFunction (member__function->function, nest_level);
	    outNewline ();
	} else {
	    file->out (";");
	    outNewline ();
	}
    } else {
	file->out (";");
	outNewline ();
    }
}

void
DumpContext::dumpExpression (Expression * const expression,
			     Size         const nest_level)
{
    if (expression == NULL)
	return;

    switch (expression->getOperation ()) {
	case Expression::Operation::Literal: {
	    // TODO
	    file->out ("(literal)");
	} break;
	case Expression::Operation::This: {
	    file->out ("this");
	} break;
	case Expression::Operation::Id: {
	    abortIf (expression->getType () != Expression::Type::Id);
	    Expression_Id * const expression__id = static_cast <Expression_Id*> (expression);

	    // TODO Dump nested name specifier
	    dumpName (expression__id->name);
	} break;
	case Expression::Operation::Subscripting: {
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	    file->out (" [");
	    dumpExpression (expression->expressions.first->next->data, nest_level);
	    file->out ("]");
	} break;
	case Expression::Operation::FunctionCall: {
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	    file->out (" (");
	    {
		List< Ref<Expression> >::DataIterator iter (expression->expressions.first->next);
		bool first = true;
		while (!iter.done ()) {
		    Ref<Expression> &argument = iter.next ();

		    if (first)
			first = false;
		    else
			file->out (", ");

		    dumpExpression (argument, nest_level);
		}
	    }
	    file->out (")");
	} break;
	case Expression::Operation::TypeConversion: {
	    dumpTypeSpecifier (expression->type_argument);
	    file->out (" (");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	    file->out (")");
	} break;
	case Expression::Operation::DotMemberAccess: {
	    abortIf (expression->getType () != Expression::Type::MemberAccess);
	    Expression_MemberAccess * const expression__member_access =
		    static_cast <Expression_MemberAccess*> (expression);

	    dumpExpression (expression->expressions.getFirst (), nest_level);
	    file->out (".");
	    abortIf (expression__member_access->member_name.isNull ());
	    dumpName (expression__member_access->member_name);
	} break;
	case Expression::Operation::ArrowMemberAccess: {
	    abortIf (expression->getType () != Expression::Type::MemberAccess);
	    Expression_MemberAccess * const expression__member_access =
		    static_cast <Expression_MemberAccess*> (expression);

	    dumpExpression (expression->expressions.getFirst (), nest_level);
	    file->out ("->");
	    abortIf (expression__member_access->member_name.isNull ());
	    dumpName (expression__member_access->member_name);
	} break;
	case Expression::Operation::PostfixIncrement: {
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	    file->out (" ++");
	} break;
	case Expression::Operation::PostfixDecrement: {
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	    file->out (" --");
	} break;
	case Expression::Operation::DynamicCast: {
	    file->out ("dynamic_cast <");
	    dumpDeclaration (expression->type_argument, NULL, nest_level);
	    file->out ("> (");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	    file->out (")");
	} break;
	case Expression::Operation::StaticCast: {
	    file->out ("static_cast <");
	    dumpDeclaration (expression->type_argument, NULL, nest_level);
	    file->out ("> (");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	    file->out (")");
	} break;
	case Expression::Operation::ReinterpretCast: {
	    file->out ("reinterpret_cast <");
	    dumpDeclaration (expression->type_argument, NULL, nest_level);
	    file->out ("> (");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	    file->out (")");
	} break;
	case Expression::Operation::ConstCast: {
	    file->out ("const_cast <");
	    dumpDeclaration (expression->type_argument, NULL, nest_level);
	    file->out ("> (");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	    file->out (")");
	} break;
	case Expression::Operation::TypeidExpression: {
	    file->out ("typeid (");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	    file->out (")");
	} break;
	case Expression::Operation::TypeidType: {
	    file->out ("typeid (");
	    dumpDeclaration (expression->type_argument, NULL, nest_level);
	    file->out (")");
	} break;
	case Expression::Operation::PrefixIncrement: {
	    file->out ("++ ");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	} break;
	case Expression::Operation::PrefixDecrement: {
	    file->out ("-- ");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	} break;
	case Expression::Operation::SizeofExpression: {
	    file->out ("sizeof (");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	    file->out (")");
	} break;
	case Expression::Operation::SizeofType: {
	    file->out ("sizeof (");
	    dumpDeclaration (expression->type_argument, NULL, nest_level);
	    file->out (")");
	} break;
	case Expression::Operation::Indirection: {
	    file->out ("*");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	} break;
	case Expression::Operation::PointerTo: {
	    file->out ("&");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	} break;
	case Expression::Operation::UnaryPlus: {
	    file->out ("+");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	} break;
	case Expression::Operation::UnaryMinus: {
	    file->out ("-");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	} break;
	case Expression::Operation::LogicalNegation: {
	    file->out ("!");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	} break;
	case Expression::Operation::OnesComplement: {
	    file->out ("~");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	} break;
	case Expression::Operation::Delete: {
	} break;
	case Expression::Operation::Cast: {
	    file->out ("(");
	    dumpDeclaration (expression->type_argument, NULL, nest_level);
	    file->out (") ");
	    dumpExpression (expression->expressions.getFirst (), nest_level);
	} break;
	case Expression::Operation::DotPointerToMember: {
	  // TODO
	} break;
	case Expression::Operation::ArrowPointerToMember: {
	  // TODO
	} break;
	case Expression::Operation::Multiplication: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" * ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Division: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" / ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Remainder: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" % ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Addition: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" + ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Subtraction: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" - ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::LeftShift: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" << ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::RightShift: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" >> ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Less: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" < ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Greater: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" > ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::LessOrEqual: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" <= ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::GreaterOrEqual: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" >= ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Equal: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" == ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::NotEqual: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" != ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::BitwiseAnd: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" & ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::BitwiseExclusiveOr: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" ^ ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::BitwiseInclusiveOr: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" | ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::LogicalAnd: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" && ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::LogicalOr: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" || ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Conditional: {
	    // TODO
	} break;
	case Expression::Operation::Assignment: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" = ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Assignment_Multiplication: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" *= ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Assignment_Division: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" /= ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Assignment_Remainder: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" %= ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Assignment_Addition: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" += ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Assignment_Subtraction: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" -= ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Assignment_RightShift: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" >>= ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Assignment_LeftShift: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" <<= ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Assignment_BitwiseAnd: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" &= ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Assignment_BitwiseExclusiveOr: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" ^= ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Assignment_BitwiseInclusiveOr: {
	    dumpExpression (expression->expressions.getFirst ());
	    file->out (" |= ");
	    dumpExpression (expression->expressions.first->next->data);
	} break;
	case Expression::Operation::Throw: {
	    file->out ("throw ");
	    dumpExpression (expression->expressions.getFirst ());
	} break;
	case Expression::Operation::Comma: {
	    {
		List< Ref<Expression> >::DataIterator iter (expression->expressions);
		while (!iter.done ()) {
		    Ref<Expression> &tmp_expression = iter.next ();

		    dumpExpression (tmp_expression);

		    if (!iter.done ())
			file->out (", ");
		}
	    }
	} break;
	default:
	    abortIfReached ();
    }
}

void
DumpContext::dumpStatement (Statement * const statement,
			    Size        const nest_level)
{
    if (statement == NULL)
	return;

    switch (statement->getType ()) {
	case Statement::t_Label: {
	    file->print ("Statement_Label\n");
	} break;
	case Statement::t_Expression: {
	    Statement_Expression const * const statement__expression =
		    static_cast <Statement_Expression const *> (statement);

	    outTab (nest_level);

	    if (!statement__expression->expression.isNull ())
		dumpExpression (statement__expression->expression, nest_level);

	    file->out (";");
	    outNewline ();
	} break;
	case Statement::t_Compound: {
	    Statement_Compound const * const statement__compound =
		    static_cast <Statement_Compound const *> (statement);

	    outTab (nest_level);
	    file->out ("{");
	    outNewline ();

	    List< Ref<Statement> >::DataIterator iter (statement__compound->statements);
	    while (!iter.done ()) {
		Ref<Statement> &statement = iter.next ();

		dumpStatement (statement, nest_level + 1);
	    }

	    outTab (nest_level);
	    file->out ("}");
	    outNewline ();
	} break;
	case Statement::t_If: {
	    Statement_If const * const statement__if =
		    static_cast <Statement_If const *> (statement);

	    outTab (nest_level);
	    file->out ("if (");
	    dumpExpression (statement__if->condition);
	    file->out (")");

	    abortIf (statement__if->if_statement.isNull ());
	    if (!statement__if->if_statement.isNull ()) {
		file->out ("\n");
		dumpStatement (statement__if->if_statement, nest_level + 1);
	    } else {
		file->out (";\n");
	    }

	    if (!statement__if->else_statement.isNull ()) {
		outTab (nest_level);
		file->out ("else\n");
		dumpStatement (statement__if->else_statement, nest_level + 1);
	    }
	} break;
	case Statement::t_Switch: {
	    file->print ("Statement_Switch\n");
	} break;
	case Statement::t_While: {
	    Statement_While const * const statement__while =
		    static_cast <Statement_While const *> (statement);

	    outTab (nest_level);
	    file->out ("while (");
	    dumpExpression (statement__while->condition_expression);

	    if (!statement__while->statement.isNull ()) {
		file->out (")\n");
		dumpStatement (statement__while->statement, nest_level + 1);
	    } else {
		file->out (";\n");
	    }
	} break;
	case Statement::t_DoWhile: {
	    Statement_DoWhile const * const statement__do_while =
		    static_cast <Statement_DoWhile const *> (statement);

	    outTab (nest_level);
	    file->out ("do");

	    if (!statement__do_while->statement.isNull ()) {
		file->out ("\n");
		dumpStatement (statement__do_while->statement, nest_level + 1);
	    } else {
		file->out (";\n");
	    }

	    outTab (nest_level);
	    file->out ("while (");
	    dumpExpression (statement__do_while->condition_expression);
	    file->out (");\n");
	} break;
	case Statement::t_For: {
	    Statement_For const * const statement__for =
		    static_cast <Statement_For const *> (statement);

	    outTab (nest_level);
	    file->out ("for (; ");
	    dumpExpression (statement__for->condition_expression);
	    file->out ("; ");
	    dumpExpression (statement__for->iteration_expression);
	    file->out (")");

	    if (!statement__for->statement.isNull ()) {
		file->out ("\n");
		dumpStatement (statement__for->statement, nest_level + 1);
	    } else {
		file->out (";\n");
	    }
	} break;
	case Statement::t_Break: {
	    file->print ("Statement_Break\n");
	} break;
	case Statement::t_Continue: {
	    file->print ("Statement_Continue\n");
	} break;
	case Statement::t_Return: {
	    file->print ("Statement_Return\n");
	} break;
	case Statement::t_Goto: {
	    file->print ("Statement_Goto\n");
	} break;
	case Statement::t_Declaration: {
	    Statement_Declaration const * const statement__declaration =
		    static_cast <Statement_Declaration const *> (statement);

	    outTab (nest_level);

	    abortIf (statement__declaration->member.isNull ());

	    dumpDeclaration (statement__declaration->member->type_desc,
			     statement__declaration->member->name,
			     nest_level + 1);

	    switch (statement__declaration->member->getType ()) {
		case Cpp::Member::t_Object: {
		    Cpp::Member_Object const * const member__object =
			    static_cast <Cpp::Member_Object const *> (statement__declaration->member.ptr ());

		    dumpInitializer (member__object->initializer);
		} break;
		case Cpp::Member::t_DependentObject: {
		    Cpp::Member_DependentObject const * const member__dependent_object =
			    static_cast <Cpp::Member_DependentObject const *> (statement__declaration->member.ptr ());

		    dumpInitializer (member__dependent_object->initializer);
		} break;
		default:
		  // No-op
		    ;
	    }

#if 0
// Deprecated
	    switch (statement__declaration->declration_desc->getType ()) {
		case DeclarationDesc::t_Object: {
		    DeclarationDesc_Object const * const declaration_desc__object =
			    static_cast <DeclarationDesc_Object const *> (statement__declaration->declaration_desc.ptr ());

		    abortIf (declaration_desc__object->declaration_entry.isNull ());
		    dumpDeclaration (declaration_desc__object->declaration_entry->type_desc,
				     declaration_desc__object->declaration_entry->name,
				     nest_level + 1);
		} break;
		case DeclarationDesc::t_Type: {
		    // TODO
		    file->out ("TYPE_DECL");
		} break;
	    }
#endif

	    file->out (";");
	    outNewline ();
	} break;
	case Statement::t_Try: {
	    file->print ("Statement_Try\n");
	} break;
	default:
	    abortIfReached ();
    }
}

void
DumpContext::dumpMemInitializer (MemInitializer * const mem_initializer,
				 Size             const nest_level)
{
    file->out ("(mem_id) (");

    {
	List< Ref<Expression> >::DataIterator iter (mem_initializer->arguments);
	bool first = true;
	while (!iter.done ()) {
	    Ref<Expression> &expression = iter.next ();

	    if (first)
		first = false;
	    else
		file->out (", ");

	    abortIf (expression.isNull ());
	    dumpExpression (expression);
	}
    }

    file->out (")");
}

void
DumpContext::dumpFunction (Function * const function_,
			   Size       const nest_level)
{
    abortIf (function_ == NULL);

    if (!function_->mem_initializers.isEmpty ()) {
	outTab (nest_level + 1);
	file->out (": ");

	List< Ref<MemInitializer> >::DataIterator iter (function_->mem_initializers);
	bool first = true;
	while (!iter.done ()) {
	    Ref<MemInitializer> &mem_initializer = iter.next ();

	    if (first)
		first = false;
	    else {
		file->out (",");
		outNewlineTab (nest_level + 1);
		file->out ("  ");
	    }

	    abortIf (mem_initializer.isNull ());
	    dumpMemInitializer (mem_initializer, nest_level);
	}
	outNewline ();
    }

    List< Ref<Statement> >::DataIterator iter (function_->statements);
    while (!iter.done ()) {
	Ref<Statement> &statement = iter.next ();

//	errf->print ("--- dumpFunction(): statement iteration").pendl ();

	abortIf (statement.isNull ());
	dumpStatement (statement);
    }
}

void
DumpContext::dumpNamespace (Namespace * const namespace_,
			    Size        const nest_level)
{
//    static char const * const _func_name = "Scruffy.DumpContext.dumpNamespace";

//    errf->print (_func_name).pendl ();

    if (namespace_ == NULL)
	return;

    NamespaceEntry::NamespaceEntryMap::DataIterator namespace_entry_iter (namespace_->namespace_entries);
    while (!namespace_entry_iter.done ()) {
	Ref<NamespaceEntry> &namespace_entry = namespace_entry_iter.next ();

	outTab (nest_level);
	file->out ("namespace ").out (namespace_entry->name).out (" {");
	outNewline ();

	dumpNamespace (namespace_entry->namespace_, nest_level + 1);

	outTab (nest_level);
	file->out ("}");
	outNewline ();
    }

    outNewline ();

#if 0
    {
	Member::MemberMap::DataIterator type_entry_iter (namespace_->types);
	while (!type_entry_iter.done ()) {
	    Ref<Member> &type_entry = type_entry_iter.next ();
	    abortIf (type_entry.isNull ());

	    errf->print ("NAMESPACE TYPE").pendl ();
	    dumpType (type_entry, nest_level);

	    outNewline ();
	}
    }

    outNewline ();
#endif

    {
	Member::MemberMap::DataIterator object_entry_iter (namespace_->members);
	while (!object_entry_iter.done ()) {
	    Ref<Member> &object_entry = object_entry_iter.next ();
	    abortIf (object_entry.isNull ());

	    dumpObject (object_entry, nest_level);
	}
    }

    outNewline ();

#if 0
  // Dumping names

    {
//	Name::NameMap::DataIterator name_iter (namespace_->names);
	Name::NameMap::DataIterator name_iter = namespace_->names.createDataIterator ();
	while (!name_iter.done ()) {
	    Ref<Name> &name = name_iter.next ();
	    errf->print ("    ").print (name->name_str).pendl ();
	}
    }
#endif

    // TODO sub-namespaces
}

namespace {

    class DumpTranslationUnit_Context /* : public DumpContext */
    {
    private:
	class ContainerState
	{
	public:
	    enum ContainerState_
	    {
		None,
		Defined,
		Declared
	    };

	private:
	    ContainerState_ value;

	public:
	    operator ContainerState_ () const
	    {
		return value;
	    }

	    ContainerState (ContainerState_ const value)
		: value (value)
	    {
	    }

	    ContainerState ()
	    {
	    }
	};

	class ProcessingState
	{
	public:
	    enum ProcessingState_
	    {
		NotStarted,
		Started,
		InDefinition
	    };

	private:
	    ProcessingState_ value;

	public:
	    operator ProcessingState_ () const
	    {
		return value;
	    }

	    ProcessingState (ProcessingState_ const value)
		: value (value)
	    {
	    }

	    ProcessingState ()
	    {
	    }
	};

	DumpContext &dump_context;
	File &file;

      // {
      //   Current container path

	    #ifdef Scruffy__DumpTranslationUnit_Context__CurPathMap_type
	    #error
	    #endif
	    #define Scruffy__DumpTranslationUnit_Context__CurPathMap_type		\
		    Map< PathEntry,							\
			 MemberExtractor< PathEntry const,				\
					  Container const *,				\
					  &PathEntry::container,			\
					  UidType,					\
					  AccessorExtractor< UidProvider const,		\
							     UidType,			\
							     &Container::getUid > >,	\
			 DirectComparator<UidType> >

	    class PathEntry;
	    typedef List<PathEntry const *> CurPathList;

	    class PathEntry
	    {
	    public:
		Container const *container;

		Scruffy__DumpTranslationUnit_Context__CurPathMap_type::Entry map_link;
		CurPathList::Element *cur_path_link;
	    };

	    typedef Scruffy__DumpTranslationUnit_Context__CurPathMap_type CurPathMap;

	    #undef Scruffy__DumpTranslationUnit_Context__CurPathMap_type

	    CurPathMap cur_path_map;
	    CurPathList cur_path;

	    // TODO Make use of 'cur_nest_level'
	    Size cur_nest_level;
      // }

      // {
      //   Map of all known containers.

	    class ContainerEntry
	    {
	    public:
		ContainerState container_state;
		ProcessingState processing_state;
		Bool processing;
		Cpp::Container const *container;
	    };

	    typedef Map< ContainerEntry,
			 MemberExtractor< ContainerEntry,
					  Cpp::Container const *,
					  &ContainerEntry::container,
					  UidType,
					  AccessorExtractor< UidProvider const,
							     UidType,
							     &Container::getUid > >,
			 DirectComparator<UidType> >
		    ContainerEntryMap;

	    ContainerEntryMap known_containers;
      // }

#if 0
	Ref<String> getCurPathString ()
	{
	    if (!cur_dump_path.isEmpty ())
		return cur_dump_path.last->data->qualified_name_str;

	    return String::nullString ();
	}

	Ref<String> getQualifiedName (ConstMemoryDesc const &name)
	{
	    return String::forPrintTask (Pr << getCurPathString () << name);
	}
#endif

    public:
#if 0
	void dumpClassDefinition (Class const &class_)
	{
	    abortIf (class_.name.isNull ());
	    file->out ("class ").out (class_.name->toString ()).out (";\n");
	}
#endif

	// @namespace_ may be null, in which case we're leaving to global scope.
	void enterNamespace (Namespace const * const namespace_)
	{
	  FUNC_NAME (
	    static char const * const _func_name = "Scruffy.DumpTranslationUnit_Context.enterNamespace";
	  )

	    List<Container const *> containers_to_enter;

	    CurPathMap::Entry path_entry;
	    {
	      // Determing intersection between the current and requested paths.

		Container const *cur_container = namespace_;
		while (cur_container != NULL &&
		       cur_container->getParentContainer () != NULL)
		{
		    DEBUG (
			errf->print (_func_name).print (": intersection iteration").pendl ();
			errf->print (_func_name).print (": intersection lookup 0x").printHex (cur_container->getUid ()).pendl ();
		    )

		    path_entry = cur_path_map.lookup (cur_container->getUid ());
		    if (!path_entry.isNull ()) {
		      // The paths do intersect.
		      // 'path_entry' is the point of intersection.
			DEBUG (
			    errf->print (_func_name).print (": the paths intersect").pendl ();
			)

			break;
		    }

		    containers_to_enter.prepend (cur_container);

		    cur_container = cur_container->getParentContainer ();
		}
	    }

	    {
	      // Closing non-matching path elements.

		CurPathList::InverseIterator iter (cur_path.last);
		while (!iter.done ()) {
		    CurPathList::Element &tmp_el = iter.next ();

		    if (!path_entry.isNull ()) {
			if (&tmp_el == path_entry.getData ().cur_path_link)
			    break;
		    }

		    PathEntry const &cur_path_entry = *tmp_el.data;

		    switch (cur_path_entry.container->getType ()) {
			case Container::t_Namespace: {
			    file.out ("}\n");
			} break;
			case Container::t_Class: {
			    abortIfReached ();
			} break;
			default:
			    abortIfReached ();
		    }

		    cur_path.remove (&tmp_el);
		    cur_path_map.remove (cur_path_entry.map_link);
		}
	    }

	    {
	      // Entering the destination namespace.

		List<Container const *>::DataIterator iter (containers_to_enter);
		while (!iter.done ()) {
		    Container const * const container = iter.next ();

		    DEBUG (
			errf->print (_func_name).print (": entering container 0x").printHex (container->getUid ()).pendl ();
		    )

		    switch (container->getType ()) {
			case Container::t_Namespace: {
			    Namespace const * const tmp_namespace =
				    static_cast <Namespace const *> (container);

			    DEBUG (
				errf->print (_func_name).print (": uid to path: 0x").printHex (container->getUid ()).pendl ();
			    )
			    CurPathMap::Entry map_entry = cur_path_map.addFor (container->getUid ());
			    PathEntry &path_entry = map_entry.getData ();
			    path_entry.container = container;
			    path_entry.map_link = map_entry;
			    path_entry.cur_path_link = cur_path.append (&path_entry);

			    file.out ("namespace ").out (tmp_namespace->primary_name).out (" {\n");
			} break;
			case Container::t_Class: {
			    abortIfReached ();
			} break;
			default:
			    abortIfReached ();
		    }
		}
	    }
	}

	void dumpMemberDependencies (Member const * const member)
	{
	    switch (member->getType ()) {
		case Member::t_Type: {
		    Member_Type const * const member__type =
			    static_cast <Member_Type const *> (member);

		    if (member__type->is_typedef) {
		      // TODO Request type declaration.
		    } else {
			if (member->type_desc->getType () == TypeDesc::t_Class) {
			    abortIf (!member->type_desc->type_atoms.isEmpty () ||
				     member->type_desc->is_reference);

#if 0
			    if (member->type_desc->type_atoms.isEmpty () &&
				!member->type_desc->is_reference)
			    {
#if 0
// This is a nested class. We'll deal with such classes later.

				// TODO Dump class

				TypeDesc_Class const * const type_desc__class =
					static_cast <TypeDesc_Class const *> (member->type_desc.ptr ());
				if (!type_desc__class.isNull ()) {
				  // This is a nested class definition.

				    dumpClass (*type_desc__class->class_);
				} else {
				  // This is a nested class declaration.

				    Ref<String> name_str = member->name->toString ();
				    if (all_types.lookup (getQualifiedName (name_str->getMemoryDesc ())->getMemoryDesc ()).isNull ()) {
					abortIf (member->name.isNull ());
					file.out ("class ").out (name_str).out (";\n");
				    }
				}
#endif
			    } else {
				abortIfReached ();
			    }
#endif
			}
		    }
		} break;
		case Member::t_Object: {
		    if (member->type_desc->getType () == TypeDesc::t_Class) {
			if (member->type_desc->type_atoms.isEmpty () &&
			    !member->type_desc->is_reference)
			{
			    // TODO Dump dependency
			} else {
			    // TODO Request declaration.
			}
		    }
		} break;
		case Member::t_Function: {
		    // TODO
		} break;
		default:
		    abortIfReached ();
	    }
	}

#if 0
	void dumpBoundMember (Member * const member /* non-null */)
	{
	  FUNC_NAME (
	    static char const * const _func_name = "Scruffy.DumpTranslationUnit_Context.dumpBoundMember";
	  )

#if 0
	    CurPathMap::Entry path_entry;
	    {
	      // Determing intersection between the current and requested paths.

		Container const *cur_container = namespace_;
		while (cur_container != NULL &&
		       cur_container->getParentContainer () != NULL)
		{
		    DEBUG (
			errf->print (_func_name).print (": intersection iteration").pendl ();
			errf->print (_func_name).print (": intersection lookup 0x").printHex (cur_container->getUid ()).pendl ();
		    )

		    path_entry = cur_path_map.lookup (cur_container->getUid ());
		    if (!path_entry.isNull ()) {
		      // The paths do intersect.
		      // 'path_entry' is the point of intersection.
			DEBUG (
			    errf->print (_func_name).print (": the paths intersect").pendl ();
			)

			break;
		    }

		    containers_to_enter.prepend (cur_container);

		    cur_container = cur_container->getParentContainer ();
		}
	    }
#endif

	    asd
	}
#endif

	void dumpMember (Member    * const member    /* non-null */,
			 // TODO @container is unnecessary
			 Container * const container /* non-null */,
			 Container * const nested_name_container = NULL)
	{
	  FUNC_NAME (
	    static char const * const _func_name = "Scruffy.DumpTranslationUnit_Context.dumpMember";
	  )

	    DEBUG (
		errf->print (_func_name).pendl ();
	    )

	    switch (member->getType ()) {
		case Member::t_Type: {
		    DEBUG (
			errf->print (_func_name).print (": Member::t_Type").pendl ();
		    )

		    Member_Type const * const member__type =
			    static_cast <Member_Type const *> (member);

		    if (!member__type->is_typedef &&
			member->type_desc->getType () == TypeDesc::t_Class &&
			member->type_desc->type_atoms.isEmpty ()           &&
			!member->type_desc->is_reference)
		    {
		      // This is a class declaration (at least).

			DEBUG (
			    errf->print (_func_name).print (": class declaration").pendl ();
			)

			TypeDesc_Class const * const type_desc__class =
				static_cast <TypeDesc_Class const *> (member->type_desc.ptr ());
			abortIf (type_desc__class->class_.isNull ());

			if (type_desc__class->class_->got_definition) {
			  // This is a class definition.

			    DEBUG (
				errf->print (_func_name).print (": class definition").pendl ();
			    )

			    dumpClass (type_desc__class->class_);
//#if 0
			} else {
			  // This is a class declaration.
			  // We simply declare the class if it hasn't been declared yet.

			    abortIf (member->name.isNull ());
			    if (known_containers.lookup (type_desc__class->class_->getUid ()).isNull ()) {
#if 0
				if (container->getType () == Container::t_Namespace) {
				    Namespace const * const namespace_ =
					    static_cast <Namespace const *> (container);
				    enterNamespace (namespace_);
				}
#endif
				file.out ("class ").out (member->name->toString ());
			    }
//#endif
			}
		    } else {
			dump_context.dumpMember (*member, 0 /* nest_level */, nested_name_container);
//			file.out (";\n");
		    }
		} break;
		case Member::t_Function: {
		    Member_Function * const member__function = static_cast <Member_Function*> (member);

		    dump_context.dumpMember (*member__function, 0 /* nest_level */, nested_name_container);

#if 0
// Unnecessary
		    if (!member__function->function.isNull () &&
			 member__function->function->got_definition)
		    {
			errf->print ("--- dumpMember: calling dump_context.dumpFunction()").pendl ();
			dump_context.dumpFunction (member__function->function);
			errf->print ("--- dumpMember: dump_context.dumpFunction() returned").pendl ();
		    } else {
			file.out (";\n");
		    }
#endif
		} break;
		default: {
		    dump_context.dumpMember (*member, 0 /* nest_level */, nested_name_container);
//		    file.out (";\n");
		    break;
		}
	    }
	}

	void dumpClass (Class * const class_)
	{
	  FUNC_NAME (
	    static char const * const _func_name = "Scruffy.DumpTranslationUnit_Context.dumpClass";
	  )

	    DEBUG (
		errf->print (_func_name).pendl ();
	    )

	    abortIf (class_ == NULL);

	  // Dumping class declaration.

	    ContainerEntry *container_entry = NULL;
	    if (known_containers.lookup (class_->getUid ()).isNull ()) {
		container_entry = &known_containers.addFor (class_->getUid ()).getData ();
		container_entry->container_state = ContainerState::None;
		container_entry->processing_state = ProcessingState::NotStarted;
		container_entry->processing = true;
		container_entry->container = class_;
	    }

#if 0
	    abortIf (class_->name.isNull ());
	    Ref<String> name_str = class_->name->toString ();
	    abortIf (name_str.isNull ());
#endif

	    {
//		Ref<String> qualified_name = getQualifiedName (name_str->getMemoryDesc ());
//		if (all_types.lookup (qualified_name->getMemoryDesc ()).isNull ()) {
#if 0
// Unnecessary
#endif
	    }

//	    if (all_types.lookup (getQualifiedName (name_str->getMemoryDesc ())->getMemoryDesc ()).isNull ())
//		file.out ("class ").out (name_str).out (";\n");

	  // Dumping all dependencies for the class.

	    {
		Member::MemberMap::DataIterator member_iter (class_->members);
		while (!member_iter.done ()) {
		    Ref<Member> &member = member_iter.next ();
		    dumpMemberDependencies (member);
		}
	    }

	  // We've walked through all of the dependencies.
	  // Now let's dump the class.

	    file.out ("class");
	    if (!class_->name.isNull ())
		file.out (" ").out (class_->name->toString ());
	    file.out ("\n"
		      "{\n");

	    {
		Member::MemberMap::DataIterator iter (class_->members);
		while (!iter.done ()) {
		    DEBUG (
			errf->print (_func_name).print (": member iteration").pendl ();
		    )

		    Ref<Cpp::Member> &member = iter.next ();
		    abortIf (member.isNull ());

		    dumpMember (member, class_);
		    file.out (";\n");
		}
	    }

	    file.out ("}");
	}

	void do_dumpNamespace (Namespace * const namespace_ /* non-null */)
	{
	  FUNC_NAME (
	    static char const * const _func_name = "Scruffy.DumpTranslationUnit_Context.do_dumpNamespace";
	  )

	    DEBUG (
		errf->print (_func_name).pendl ();
	    )

	    {
		Member::MemberMap::DataIterator member_iter (namespace_->members);
		while (!member_iter.done ()) {
		    Ref<Member> &member = member_iter.next ();
//		    dumpMemberDependencies (member, namespace_);

		    DEBUG (
			errf->print (_func_name).print (": calling dumpMember()").pendl ();
		    )
		    enterNamespace (namespace_);
		    dumpMember (member, namespace_);
		    file.out (";\n");
		}
	    }

	    {
		Container::NamespaceMap::DataIterator namespace_iter (namespace_->namespaces);
		while (!namespace_iter.done ()) {
		    DEBUG (
			errf->print (_func_name).print (": namespace").pendl ();
		    )

		    Ref<Namespace> &subnamespace = namespace_iter.next ();
		    do_dumpNamespace (subnamespace);
		}
	    }
	}

	void do_dumpNamespace_methodBodies (Namespace * const namespace_ /* non-null */)
	{
	  FUNC_NAME (
	    static char const * const _func_name = "Scruffy.DumpTranslationUnit_Context.do_dumpNamespace";
	  )

	  // TODO Дампить функции-члены классов.

	    Member::MemberMap::DataIterator member_iter (namespace_->members);
	    while (!member_iter.done ()) {
		Ref<Member> &member = member_iter.next ();

		DEBUG (
		    errf->print (_func_name).print (": member: ");
		    dumpMemberInfo (member);
		    errf->pendl ();
		)

		switch (member->getType ()) {
		    case Member::t_Type: {
			Member_Type const * const member__type =
				static_cast <Member_Type const *> (member.ptr ());

			if (!member__type->is_typedef &&
			    member->type_desc->getType () == TypeDesc::t_Class &&
			    member->type_desc->type_atoms.isEmpty ()           &&
			    !member->type_desc->is_reference)
			{
			  // Dumping bodies of class methods (member functions).

			    TypeDesc_Class const * const type_desc__class =
				    static_cast <TypeDesc_Class const *> (member->type_desc.ptr ());
			    abortIf (type_desc__class->class_.isNull ());

			    if (type_desc__class->class_->got_definition) {
				Member::MemberMap::DataIterator iter (type_desc__class->class_->members);
				while (!iter.done ()) {
				    Ref<Cpp::Member> &member = iter.next ();
				    abortIf (member.isNull ());

				    if (member->getType () == Member::t_Function) {
					Member_Function * const member__function =
						static_cast <Member_Function*> (member.ptr ());

//					errf->print ("--- member__function: 0x").printHex ((Uint64) member__function).pendl ();
//					errf->print ("--- got_definition: ").print (member__function->function->got_definition ? "true" : "false").pendl ();

					if (!member__function->function.isNull () &&
					     member__function->function->got_definition)
					{
					    enterNamespace (namespace_);
					    dumpMember (member, type_desc__class->class_, type_desc__class->class_);
					    file.out ("\n");
					    dump_context.dumpFunction (member__function->function);
					}
				    }
				}
			    }
			}
		    } break;
		    case Member::t_Function: {
			Member_Function * const member__function =
				static_cast <Member_Function*> (member.ptr ());

			if (!member__function->function.isNull () &&
			     member__function->function->got_definition)
			{
			    DEBUG (
				errf->print (_func_name).print (": member__function->function: 0x")
					     .printHex ((Uint64) member__function->function.ptr ()).pendl ();
			    )

			    enterNamespace (namespace_);
			    dumpMember (member, namespace_);
			    file.out ("\n");
			    dump_context.dumpFunction (member__function->function);
			} else {
//			    errf->print ("--- NO DEFINITION: member: ").printHex ((Uint64) member__function).pendl ();
//			    errf->print ("--- NO DEFINITION: ").printHex ((Uint64) member__function->function.ptr ()).pendl ();
			}
		    } break;
		    default:
		      // Nothing to do
			;
		}
	    }

	    {
		Container::NamespaceMap::DataIterator namespace_iter (namespace_->namespaces);
		while (!namespace_iter.done ()) {
		    Ref<Namespace> &subnamespace = namespace_iter.next ();
		    do_dumpNamespace_methodBodies (subnamespace);
		}
	    }
	}

	void dumpNamespace (Namespace * const namespace_ /* non-null */)
	{
	  FUNC_NAME (
	    static char const * const _func_name = "Scruffy.DumpTranslationUnit_Context.dumpNamespace";
	  )

	    DEBUG (
		errf->print (_func_name).pendl ();
	    )

	  // Dumping all user-defined types (classes, enums).

	    do_dumpNamespace (namespace_);

	  // Dumping method bodies

	    do_dumpNamespace_methodBodies (namespace_);
	}

	DumpTranslationUnit_Context (DumpContext &dump_context,
				     File        &file
				     /* Size  const tab_size */)
//	    : DumpContext (&file, tab_size),
	    : dump_context (dump_context),
	      file (file),
	      cur_nest_level (0)
	{
	}
    };

}

// We dump translation units in the following order:
// 1. Dump all user-defined types (classes, enums).
//        * member functions are specified with their prototypes only;
//        * the order of declarations depends on by-value data members
//          and inheritance;
// 2. Dump prototypes for all functions which are not class members;
// 3. Dump all data objects which are not class members;
// 4. Dump implementations for all functions.
//
// Note that:
//     * all identifiers are fully qualified to avoid dependence
//       on the order of declarations;
//     * function overloading is resolved by explicit casts for
//       all uses of an overloaded function;
//     * template specializations are not allowed for instantiated
//       templates, hence they are not a problem.
//
//
// Consider the follwing example:
//
//     namespace X {
//         class Y;
//     }
//
//     namespace N {
//         class B {
//             Y y_obj;
//         }
//     }
//
// We assume that when we iterate over a container, we get its members
// in alphabetical order (which is actually the case).
//
// Lets make a dump of this code snippet:
//     * First, we're iterating over namespaces. We get namespace N
//       and dive into it;
//     * We see class B. To dump a class, we should first dump
//       its dependencies;
//     * We see that class B depends on 'Y' being declared. Lookup of 'Y'
//       brings us to class X::Y;
//     * Dumping class Y. It has no definition, so all we can do is
//       dump a declaration for it. We've not began any namespace yet
//       in the dump, so we're opening namespace X, then dumping
//       a declaration for class Y;
//     * Now we can dump class B. We're closing namespace X, opening
//       namespace N and dumping definition for class B.
//
// Let's take a look at another example:
//
//     class C {
//         class D {
//         };
//     };
//
//     class A {
//         class B {
//             C::D d_obj;
//         };
//     };
//
//  Here, A::B depends on both C and C::D being defined, because C::D can only
//  be defined withing a definition of C.
//
void
DumpContext::dumpTranslationUnit (Namespace * const namespace_)
{
  FUNC_NAME (
    static char const * const _func_name = "Scruffy.DumpContext.dumpTranslationUnit";
  )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    DumpTranslationUnit_Context ctx (*this, *file /* , tab_size */);
    ctx.dumpNamespace (namespace_);
    // Leaving all namespaces (closing all unmatched opening curly braces).
    ctx.enterNamespace (NULL);
}

} // namespace Cpp
} // namespace Scruffy

