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


#include <scruffy/cpp.h>

using namespace MyCpp;

namespace Scruffy {
namespace Cpp {

#if 0
int
standardConversionDepth (Cpp::TypeDesc * const from_type,
			 Cpp::TypeDesc * const to_type)
{
    // TODO lvalue to rvalue conversion (store a flag in TypeDesc?)
}
#endif

#if 0
int
standardConversionDepth (Cpp::Member   * const from_member,
			 Cpp::TypeDesc * const to_type)
{
}
#endif

// Returns a cast expression
Ref<Expression>
castToPointer (Value * const from_value,
	       Value * const to_value)
{
    static char const * const _func_name = "castToPointer";

    if (from_value->type_atoms.isEmpty ()) {
	errf->print (_func_name).print ("not a pointer").pendl ();
	abortIfReached ();
    }

    Ref<TypeDesc::TypeAtom> &type_atom = from_value->type_atoms.getFirst ();
    if (type_atom->type_atom_type == TypeDesc::TypeAtom::Pointer) {
	to_value->is_lvalue = from_value->is_lvalue;
	to_value->type_desc = from_value->type_desc->clone ();
	return NULL;
    }

    if (type_atom->type_atom_type == TypeDesc::TypeAtom::Array) {
	to_value->is_value = from_value->is_lvalue;
	to_value->type_desc = from_value->type_desc->clone ();
	to_value->type_desc->type_atoms.remove (to_value->type_desc->type_atoms.getFirstElement ());
	// TODO cv qualifiers
	Ref<TypeDesc::TypeAtom> const type_atom = grab (new TypeDesc::TypeAtom);
	to_value->type_desc->type_atoms.prepend (type_atom);
	// TODO Return a standard conversion.
	return NULL;
    }

    errf->print (_func_name).print ("neither a pointer, nor an array").pendl ();
    abortIfReached ();
    return NULL;
}

// Returns a cast expression
Ref<Expression>
castToType (bool       const lvalue_required,
	    TypeDesc * const to_type,
	    Value    * const from_value,
	    Value    * const to_value)
{
}

void
expandExpression (Expression * const expression)
{
  // The first thing to do is to expand all subexpressions.

    List< Ref<Expression> >::DataIterator expr_iter (expression->expressions);
    while (!expr_iter.done ()) {
	Ref<Expression> &expression = expr_iter.next ();
	expandExpression (expression);
    }

  // Now we can expand the expression.

    abortIf (!expression->value.isNull ());
    expression->value = grab (new Value);

    switch (expression->getOperation ()) {
	case Expression::Operation::Literal: {
	    expression->value->type_desc = /* type of the literal */;
	    expression->value->constant_value = /* value of the literal */;
	} break;
	case Expression::Operation::This: {
	    expression->value->is_lvalue = false;
	    expression->value->type_desc = /* pointer to the current class, possibly with cv-qualifiers */;
	} break;
	case Expression::Operation::Id: {
	    abortIf (expression->getType () != Expression::Type::Id);
	    Expression_Id * const expression__id =
		    static_cast <Expression_Id*> (expression);

	    expression->value->is_lvalue = true;
	    expression->value->type_desc = expression__id->member->type_desc;
	} break;
	case Expression::Operation::Subscripting: {
	    expression->cast_expressions.append (
		    castToPointer (expression->expressions.getFirst ()->value, expression->value));

	    expression->value->is_lvalue = true;
	    expression->value->type_desc = /* type of the array */;
	} break;
	case Expression::Operation::FunctionCall: {
	    expression->value->is_lvalue = /* depends on function's return type */;
	    expression->value->type_desc = /* depends on function's return type */;
	} break;
	case Expression::Operation::TypeConversion: {
	    expression->value->is_lvalue = /* depends on the argument */;
	    expression->value->type_desc = expression->type_argument;
	} break;
	case Expression::Operation::DotMemberAccess: {
	    expression->value->is_lvalue = true;
	    expression->value->type_desc = /* depends on the type of the member */;
	} break;
	case Expression::Operation::ArrowMemberAccess: {
	    expression->value->is_lvalue = true;
	    expression->value->type_desc = /* depends on the type of the member */;
	} break;
	case Expression::Operation::PostfixIncrement: {
	    expression->value->is_lvalue = /* depends */;
	    expression->value->type_desc = /* depends */;
	} break;
	case Expression::Operation::PostfixDecrement: {
	} break;
	case Expression::Operation::DynamicCast: {
	    expression->value->is_lvalue = /* depends on the argument */;
	    expression->value->type_desc = expression->type_argument;
	} break;
	case Expression::Operation::StaticCast: {
	    expression->value->is_lvalue = /* depends on the argument */;
	    expression->value->type_desc = expression->type_argument;
	} break;
	case Expression::Operation::ReinterpretCast: {
	    expression->value->is_lvalue = /* depends on the argument */;
	    expression->value->type_desc = expression->type_argument;
	} break;
	case Expression::Operation::ConstCast: {
	    expression->value->is_lvalue = /* depends on the argument */;
	    expression->value->type_desc = expression->type_argument;
	} break;
	case Expression::Operation::TypeidExpression: {
	    expression->value->type_desc = /* typeid */;
	} break;
	case Expression::Operation::TypeidType: {
	    expression->value->type_desc = /* typeid */;
	} break;
	case Expression::Operation::PrefixIncrement: {
	} break;
	case Expression::Operation::PrefixDecrement: {
	} break;
	case Expression::Operation::SizeofExpression: {
	    expression->value->type_desc = /* size_t */;
	    expression->value->constant_value = /* sizeof argument's type */;
	} break;
	case Expression::Operation::SizeofType: {
	    expression->value->type_desc = /* size_t */;
	    expression->value->constant_value = /* sizeof type */;
	} break;
	case Expression::Operation::Indirection: {
	    expression->value->is_lvalue = /* depends */;
	    expression->value->type_desc = /* type of the argument without one pointer */;
	} break;
	case Expression::Operation::PointerTo: {
	    expression->value->is_lvalue = true;
	    expression->value->type_desc = /* type of the argument plus a pointer */;
	} break;
	case Expression::Operation::UnaryPlus: {
	} break;
	case Expression::Operation::UnaryMinus: {
	} break;
	case Expression::Operation::LogicalNegation: {
	} break;
	case Expression::Operation::OnesComplement: {
	} break;
	case Expression::Operation::Delete: {
	} break;
	case Expression::Operation::Cast: {
	} break;
	case Expression::Operation::DotPointerToMember: {
	} break;
	case Expression::Operation::ArrowPointerToMember: {
	} break;
	case Expression::Operation::Multiplication: {
	} break;
	case Expression::Operation::Division: {
	} break;
	case Expression::Operation::Remainder: {
	} break;
	case Expression::Operation::Addition: {
	} break;
	case Expression::Operation::Subtraction: {
	} break;
	case Expression::Operation::LeftShift: {
	} break;
	case Expression::Operation::RightShift: {
	} break;
	case Expression::Operation::Less: {
	} break;
	case Expression::Operation::Greater: {
	} break;
	case Expression::Operation::LessOrEqual: {
	} break;
	case Expression::Operation::GreaterOrEqual: {
	} break;
	case Expression::Operation::Equal: {
	} break;
	case Expression::Operation::NotEqual: {
	} break;
	case Expression::Operation::BitwiseAnd: {
	} break;
	case Expression::Operation::BitwiseExclusiveOr: {
	} break;
	case Expression::Operation::BitwiseInclusiveOr: {
	} break;
	case Expression::Operation::LogicalAnd: {
	} break;
	case Expression::Operation::LogicalOr: {
	} break;
	case Expression::Operation::Conditional: {
	} break;
	case Expression::Operation::Assignment: {
	} break;
	case Expression::Operation::Assignment_Multiplication: {
	} break;
	case Expression::Operation::Assignment_Division: {
	} break;
	case Expression::Operation::Assignment_Remainder: {
	} break;
	case Expression::Operation::Assignment_Addition: {
	} break;
	case Expression::Operation::Assignment_Subtraction: {
	} break;
	case Expression::Operation::Assignment_RightShift: {
	} break;
	case Expression::Operation::Assignment_LeftShift: {
	} break;
	case Expression::Operation::Assignment_BitwiseAnd: {
	} break;
	case Expression::Operation::Assignment_BitwiseExclusiveOr: {
	} break;
	case Expression::Operation::Assignment_BitwiseInclusiveOr: {
	} break;
	case Expression::Operation::Throw: {
	} break;
	case Expression::Operation::Comma: {
	} break;
	case Expression::Operation::ImplicitCast: {
	} break;
	default:
	    abortIfReached ();
    }
}

} // namespace Cpp
} // namespace Scruffy

