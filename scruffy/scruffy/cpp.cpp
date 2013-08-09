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

#include <scruffy/cpp.h>

using namespace MyCpp;

namespace Scruffy {

namespace Cpp {

ComparisonResult
Name::NameComparator::compare (Name const &left,
			       Name const &right)
{
  // nested-name-specifier does not affect the result of comparison.

  // Destructors comparison

    if (left.is_destructor) {
	if (right.is_destructor) {
	    abortIf (left.name_str.isNull () ||
		     right.name_str.isNull ());

	    return compareByteArrays (left.name_str->getMemoryDesc (),
				      right.name_str->getMemoryDesc ());
	}

	return ComparisonGreater;
    }

    if (right.is_destructor)
	return ComparisonLesser;

  // Conversion operator comparison

    if (left.is_conversion_operator) {
	if (right.is_conversion_operator) {
	    abortIf (left.conversion_type.isNull () ||
		     right.conversion_type.isNull ());

	    return TypeDesc::TypeDescComparator::compare (*left.conversion_type,
							  *right.conversion_type);
	}

	return ComparisonGreater;
    }

    if (right.is_conversion_operator)
	return ComparisonLesser;

  // Operator comparison

    if (left.is_operator) {
	if (right.is_operator) {
	    if (left.operator_type > right.operator_type)
		return ComparisonGreater;

	    if (left.operator_type < right.operator_type)
		return ComparisonLesser;

	    abortIf (!left.operator_type == right.operator_type);
	    return ComparisonEqual;
	}

	return ComparisonGreater;
    }

    if (right.is_operator)
	return ComparisonLesser;

  // Template comparison

    if (left.is_template) {
	// TODO Compare template names, template arguments.
	abortIfReached ();
    }

    if (right.is_template)
	return ComparisonLesser;

  // Name comparison

    abortIf (left.name_str.isNull () ||
	     right.name_str.isNull ());

    return compareByteArrays (left.name_str->getMemoryDesc (),
			      right.name_str->getMemoryDesc ());
}

Name*
Name::clone () const
{
    Name *name = new Name;
    name->setName (this);

    return name;
}

void
Name::setName (Pointer<Name const> const &name)
{
    abortIf (name->parent_namespace != NULL);
    parent_namespace = NULL;
    parent_namespace_link = NameMap::Entry ();

    name_str = name->name_str;

#if 0
// Deprecated

    global_namespace = name->global_namespace;
    {
	List< Ref<NestedNameSpecifier> >::DataIterator iter (name->nested_name_specifiers);
	while (!iter.done ()) {
	    Ref<NestedNameSpecifier> &nested_name_specifier = iter.next ();
	    nested_name_specifiers.append (nested_name_specifier);
	}
    }
#endif

    is_destructor = name->is_destructor;

    is_operator = name->is_operator;
    operator_type = name->operator_type;

    is_conversion_operator = name->is_conversion_operator;
    conversion_type = name->conversion_type;

    is_template = name->is_template;
    {
	List< Ref<TemplateArgument> >::DataIterator iter (name->template_arguments);
	while (!iter.done ()) {
	    Ref<TemplateArgument> &template_argument = iter.next ();
	    template_arguments.append (template_argument);
	}
    }
}

Ref<String>
Name::OperatorType::toString () const
{
    return String::forData ("(OperatorType)");
}

Ref<String>
Name::toString () const
{
    // TODO

    abortIf (!validate ());

    Ref<String> str =
	    String::forPrintTask (
		    Pr << name_str <<
// Deprecated			  (global_namespace ? ":: " : "") <<
			  (is_destructor ? "~ " : "") <<
			  (is_operator ? "operator " : "") <<
			  (is_operator && !is_conversion_operator ?
				  operator_type.toString () : String::nullString ()) << "" /* TODO */ // <<
//			  (is_conversion_operator ?
//				  conversion_type->toString () : String::nullString ()) << "" /* TODO */ <<
//			  (is_template ? "template " : ""));
			  );

    if (is_template) {
	str = String::forPrintTask (Pr << str << " < ");

	List< Ref<TemplateArgument> >::DataIterator arg_iter (template_arguments);
	while (!arg_iter.done ()) {
	    Ref<TemplateArgument> &template_argument = arg_iter.next ();

	    switch (template_argument->getType ()) {
		case TemplateArgument::t_Type: {
		    TemplateArgument_Type * const template_argument__type =
			    static_cast <TemplateArgument_Type*> (template_argument.ptr ());

		    // TODO Dump type

		    str = String::forPrintTask (Pr << str << "TYPE");
		} break;
		case TemplateArgument::t_Expression: {
		    TemplateArgument_Expression * const template_argument__expression =
			    static_cast <TemplateArgument_Expression*> (template_argument.ptr ());

		    // TODO Dump expression

		    str = String::forPrintTask (Pr << str << "EXPRESSION");
		} break;
		default:
		    abortIfReached ();
	    }

	    str = String::forPrintTask (Pr << str << (arg_iter.done () ? "" : ", "));
	}

	str = String::forPrintTask (Pr << str << " > ");
    }

    return str;
}

bool
Name::validate () const
{
    // TODO

    return true;
}

TypeDesc_Function::TypeDesc_Function (TypeDesc_Function const &type_desc__function)
    : M::Referenced (),
      TypeDesc (type_desc__function)
{
    if (!type_desc__function.return_type.isNull ())
	return_type = grab (type_desc__function.return_type->clone ());

    {
	List< Ref<Member> >::DataIterator iter (type_desc__function.parameters);
	while (!iter.done ()) {
	    Ref<Member> &member = iter.next ();
	    abortIf (member.isNull ());

	    parameters.append (grab (member->clone ()));
	}
    }
}

#if 0
// Unused
Ref<String>
TypeDesc::toString () const
{
    // TODO
    return String::forData ("(TypeDesc)");
}
#endif

bool
Member::isObject () const
{
    if (getType () == t_Object ||
	getType () == t_DependentObject)
    {
	return true;
    }

    return false;
}

bool
Member::isClass () const
{
    if (getType () == t_Type) {
	// TODO
    }

    // TODO
    abortIfReached ();
    return false;
}

bool
Member::isTemplate () const
{
    if (getType () == Member::t_Function) {
	Member_Function const * const member__function =
		static_cast <Member_Function const *> (this);

	if (member__function->function->is_template)
	    return true;
    } else
    if (getType () == Member::t_Object) {
	if (type_desc->getType () == TypeDesc::t_Class) {
	    TypeDesc_Class const * const type_desc__class =
		    static_cast <TypeDesc_Class const *> (type_desc.ptr ());

	    if (type_desc__class->is_template   &&
		!type_desc__class->is_reference &&
		type_desc__class->type_atoms.isEmpty ())
	    {
		return true;
	    }
	}
    }

    return false;
}

} // namespace Cpp

} // namespace Scruffy

