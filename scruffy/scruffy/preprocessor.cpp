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


#include <libmary/libmary.h>

#include <pargen/parser.h>

#include <scruffy/file_byte_stream.h>
#include <scruffy/utf8_unichar_stream.h>

#include <scruffy/preprocessor.h>
#include <scruffy/preprocessor_util.h>
#include <scruffy/list_token_stream.h>
//#include <scruffy/pp_item_stream_token_stream.h>
#include <scruffy/cpp_cond_pargen.h>

#include <scruffy/unichar_pp_item_stream.h>
#include <scruffy/list_pp_item_stream.h>
#include <scruffy/phase3_item_stream.h>


#define DEBUG(a) ;


using namespace M;
using namespace Pargen;

namespace Scruffy {

bool
cpp_cond_Literal_match_func (CppCond_Literal *literal,
			     ParserControl * /* parser_control */,
			     void * /* _data */)
{
    DEBUG (
	errf->print ("Scruffy.CppParser.cpp_cond_Literal_match_func").pendl ();
    )

    Token *token = static_cast <Token*> (literal->any_token->user_obj);
    assert (token);

    if (token->token_type == TokenLiteral) {
	DEBUG (
	    errf->print ("Scruffy.CppParser.cpp_cond_Literal_match_func: literal").pendl ();
	)

	return true;
    }

    DEBUG (
	errf->print ("Scruffy.CppParser.cpp_cond_Literal_match_func: not a literal").pendl ();
    )

    return false;
}

bool
cpp_cond_Identifier_match_func (CppCond_Identifier *identifier,
				ParserControl * /* parser_control */,
				void * /* _data */)
{
    DEBUG (
	errf->print ("Scruffy.CppParser.cpp_cond_Identifier_match_func").pendl ();
    )

    ConstMemory mem_ = identifier->any_token->token;
    Byte const * const mem = mem_.mem();
    Size const len = mem_.len();
    for (Size i = 0; i < len; i++) {
	if ((mem [i] >= 0x41 /* 'A' */ && mem [i] <= 0x5a /* 'Z' */) ||
	    (mem [i] >= 0x61 /* 'a' */ && mem [i] <= 0x7a /* 'z' */) ||
	    (mem [i] == 0x5f /* '_' */))
	{
	    continue;
	}

	if (i > 0 &&
	    (mem [i] >= 0x30 /* '0' */ && mem [i] <= 0x39 /* '9' */))
	{
	    continue;
	} 

	return false;
    }

    return true;
}

/* TODO identifier-list? */
StRef< List_< StRef<String>, StReferenced > >
CppPreprocessor::extractMacroParameters (PpItemStream *pp_stream)
    throw (InternalException,
	   ParsingException)
{
    assert (pp_stream);

    StRef< List_< StRef<String>, StReferenced > > params = st_grab (new (std::nothrow) List_< StRef<String>, StReferenced >);

    for (;;) {
	StRef<PpToken> pp_token;
	PpItemStream::PpItemResult pres = pp_stream->getNextPpToken (&pp_token);
	if (pres != PpItemStream::PpItemNormal)
	    throw ParsingException (pp_stream->getFpos (),
				    st_grab (new (std::nothrow) String ("pp-token expected")));

	if (pp_token->pp_token_type != PpTokenIdentifier)
	    throw ParsingException (pp_token->fpos,
				    st_grab (new (std::nothrow) String ("identifier expected")));

	params->append (pp_token->str, params->last);

	pres = pp_stream->getNextPpToken (&pp_token);
	if (pres != PpItemStream::PpItemNormal)
	    throw ParsingException (pp_stream->getFpos (),
				    st_grab (new (std::nothrow) String ("pp-token expected")));

	if (equal (pp_token->str->mem(), ",")) {
	    /* No-op */
	} else
	if (equal (pp_token->str->mem(), ")")) {
	    break;
	} else {
	    throw ParsingException (pp_token->fpos,
				    st_grab (new (std::nothrow) String ("identifier, ',', or ')' expected")));
	}
    }

    return params;
}

StRef< List_< StRef<PpItem>, StReferenced > >
CppPreprocessor::extractReplacementList (PpItemStream *pp_stream)
    throw (InternalException,
	   ParsingException)
{
    assert (pp_stream);

    StRef<Whitespace> whsp;
    pp_stream->getWhitespace (&whsp);
    if (whsp) {
	if (whsp->has_newline)
	    return NULL;
    }

    StRef< List_< StRef<PpItem>, StReferenced > > replacement_list = st_grab (new (std::nothrow) List_< StRef<PpItem>, StReferenced >);

    for (;;) {
	StRef<PpToken> pp_token;
	PpItemStream::PpItemResult pres = pp_stream->getPpToken (&pp_token);
	if (pres != PpItemStream::PpItemNormal)
	    throw ParsingException (pp_stream->getFpos (),
				    st_grab (new (std::nothrow) String ("pp-token expected")));

	replacement_list->append (pp_token.ptr (), replacement_list->last);

	pp_stream->getWhitespace (&whsp);
	if (whsp) {
	    if (whsp->has_newline)
		break;

	    replacement_list->append (whsp.ptr (), replacement_list->last);
	}
    }

    return replacement_list;
}

class CondValue
{
public:
    enum Type {
	Long,
	UnsignedLong
    };

    Type value_type;

    union {
	long long_value;
	unsigned long ulong_value;
    };

    bool isTrue () const
    {
	switch (value_type) {
	    case Long:
		return long_value != 0;
	    case UnsignedLong:
		return ulong_value != 0;
	}

        unreachable ();
	return false;
    }

    template <CondValue (*long_func) (long),
	      CondValue (*ulong_func) (unsigned long)>
    static CondValue unaryOperation (CondValue const &val)
    {
	switch (val.value_type) {
	    case Long:
		return long_func (val.long_value);
	    case UnsignedLong:
		return ulong_func (val.ulong_value);
	}

        unreachable ();
	return CondValue ((unsigned long) 0);
    }

    template <CondValue (*long_func) (long, long),
	      CondValue (*ulong_func) (unsigned long, unsigned long)>
    static CondValue operation (CondValue const &left,
				CondValue const &right)
    {
	switch (left.value_type) {
	    case Long:
		switch (right.value_type) {
		    case Long:
			return long_func (left.long_value, right.long_value);
		    case UnsignedLong:
			return long_func (left.long_value, (long) right.ulong_value);
		}
	    case UnsignedLong:
		switch (right.value_type) {
		    case Long:
			return long_func ((long) left.ulong_value, right.long_value);
		    case UnsignedLong:
			return ulong_func (left.ulong_value, right.ulong_value);
		}
	}

        unreachable ();
	return CondValue ((unsigned long) 0);
    }

  // unaryPlus

    static CondValue unaryPlus (CondValue const &val)
    {
	return val;
    }

  // unaryMinus

    static CondValue unaryMinus_long (long val)
    {
	return -val;
    }

    static CondValue unaryMinus_ulong (unsigned long val)
    {
	return -val;
    }

    static CondValue unaryMinus (CondValue const &val)
    {
	return unaryOperation <unaryMinus_long, unaryMinus_ulong> (val);
    }

  // unaryNot

    static CondValue unaryNot_long (long val)
    {
	return CondValue ((unsigned long) (val == 0 ? 1 : 0));
    }

    static CondValue unaryNot_ulong (unsigned long val)
    {
	return CondValue ((unsigned long) (val == 0 ? 1 : 0));
    }

    static CondValue unaryNot (CondValue const &val)
    {
	return unaryOperation <unaryNot_long, unaryNot_ulong> (val);
    }

  // unaryInversion

    static CondValue unaryInversion_long (long val)
    {
	return CondValue ((long) ~val);
    }

    static CondValue unaryInversion_ulong (unsigned long val)
    {
	return CondValue ((unsigned long) ~val);
    }

    static CondValue unaryInversion (CondValue const &val)
    {
	return unaryOperation <unaryInversion_long, unaryInversion_ulong> (val);
    }

  // multiplicativeMultiply

    static CondValue multiplicativeMultiply_long (long left,
						  long right)
    {
	return CondValue ((long) (left * right));
    }

    static CondValue multiplicativeMultiply_ulong (unsigned long left,
						   unsigned long right)
    {
	return CondValue ((unsigned long) (left * right));
    }

    static CondValue multiplicativeMultiply (CondValue const &left,
					     CondValue const &right)
    {
	return operation <multiplicativeMultiply_long, multiplicativeMultiply_ulong> (left, right);
    }

  // multiplicativeDivide

    static CondValue multiplicativeDivide_long (long left,
						long right)
    {
	return CondValue ((long) (left / right));
    }

    static CondValue multiplicativeDivide_ulong (unsigned long left,
						 unsigned long right)
    {
	return CondValue ((unsigned long) (left / right));
    }

    static CondValue multiplicativeDivide (CondValue const &left,
					   CondValue const &right)
    {
	return operation <multiplicativeDivide_long, multiplicativeDivide_ulong> (left, right);
    }

  // multiplicativeRemainder

    static CondValue multiplicativeRemainder_long (long left,
						   long right)
    {
	return CondValue ((long) (left % right));
    }

    static CondValue multiplicativeRemainder_ulong (unsigned long left,
						    unsigned long right)
    {
	return CondValue ((unsigned long) (left % right));
    }

    static CondValue multiplicativeRemainder (CondValue const &left,
					      CondValue const &right)
    {
	return operation <multiplicativeRemainder_long, multiplicativeRemainder_ulong> (left, right);
    }

  // additivePlus

    static CondValue additivePlus_long (long left,
					long right)
    {
	return CondValue ((long) (left + right));
    }

    static CondValue additivePlus_ulong (unsigned long left,
					 unsigned long right)
    {
	return CondValue ((unsigned long) (left + right));
    }

    static CondValue additivePlus (CondValue const &left,
				   CondValue const &right)
    {
	return operation <additivePlus_long, additivePlus_ulong> (left, right);
    }

  // additiveMinus

    static CondValue additiveMinus_long (long left,
					 long right)
    {
	return CondValue ((long) (left - right));
    }

    static CondValue additiveMinus_ulong (unsigned long left,
					  unsigned long right)
    {
	return CondValue ((unsigned long) (left - right));
    }

    static CondValue additiveMinus (CondValue const &left,
				    CondValue const &right)
    {
	return operation <additiveMinus_long, additiveMinus_ulong> (left, right);
    }

  // shiftLeft

    static CondValue shiftLeft_long (long left,
				     long right)
    {
	return CondValue ((long) (left << right));
    }

    static CondValue shiftLeft_ulong (unsigned long left,
				      unsigned long right)
    {
	return CondValue ((unsigned long) (left << right));
    }

    static CondValue shiftLeft (CondValue const &left,
				CondValue const &right)
    {
	return operation <shiftLeft_long, shiftLeft_ulong> (left, right);
    }

  // shiftRight

    static CondValue shiftRight_long (long left,
				     long right)
    {
	return CondValue ((long) (left >> right));
    }

    static CondValue shiftRight_ulong (unsigned long left,
				      unsigned long right)
    {
	return CondValue ((unsigned long) (left >> right));
    }

    static CondValue shiftRight (CondValue const &left,
				 CondValue const &right)
    {
	return operation <shiftRight_long, shiftRight_ulong> (left, right);
    }

  // relationalLess

    static CondValue relationalLess_long (long left,
					  long right)
    {
	return CondValue ((unsigned long) (left < right ? 1 : 0));
    }

    static CondValue relationalLess_ulong (unsigned long left,
					   unsigned long right)
    {
	return CondValue ((unsigned long) (left < right ? 1 : 0));
    }

    static CondValue relationalLess (CondValue const &left,
				     CondValue const &right)
    {
	return operation <relationalLess_long, relationalLess_ulong> (left, right);
    }

  // relationalGreater

    static CondValue relationalGreater_long (long left,
					     long right)
    {
	return CondValue ((unsigned long) (left > right ? 1 : 0));
    }

    static CondValue relationalGreater_ulong (unsigned long left,
					      unsigned long right)
    {
	return CondValue ((unsigned long) (left > right ? 1 : 0));
    }

    static CondValue relationalGreater (CondValue const &left,
					CondValue const &right)
    {
	return operation <relationalGreater_long, relationalGreater_ulong> (left, right);
    }

  // relationalLessOrEqual

    static CondValue relationalLessOrEqual_long (long left,
						 long right)
    {
	return CondValue ((unsigned long) (left <= right ? 1 : 0));
    }

    static CondValue relationalLessOrEqual_ulong (unsigned long left,
						  unsigned long right)
    {
	return CondValue ((unsigned long) (left <= right ? 1 : 0));
    }

    static CondValue relationalLessOrEqual (CondValue const &left,
					    CondValue const &right)
    {
	return operation <relationalLessOrEqual_long, relationalLessOrEqual_ulong> (left, right);
    }

  // relationalGreaterOrEqual

    static CondValue relationalGreaterOrEqual_long (long left,
						    long right)
    {
	return CondValue ((unsigned long) (left >= right ? 1 : 0));
    }

    static CondValue relationalGreaterOrEqual_ulong (unsigned long left,
						     unsigned long right)
    {
	return CondValue ((unsigned long) (left >= right ? 1 : 0));
    }

    static CondValue relationalGreaterOrEqual (CondValue const &left,
					       CondValue const &right)
    {
	return operation <relationalGreaterOrEqual_long, relationalGreaterOrEqual_ulong> (left, right);
    }

  // equalityEqual

    static CondValue equalityEqual_long (long left,
					 long right)
    {
	return CondValue ((unsigned long) (left == right ? 1 : 0));
    }

    static CondValue equalityEqual_ulong (unsigned long left,
					  unsigned long right)
    {
	return CondValue ((unsigned long) (left == right ? 1 : 0));
    }

    static CondValue equalityEqual (CondValue const &left,
				    CondValue const &right)
    {
	return operation <equalityEqual_long, equalityEqual_ulong> (left, right);
    }

  // equalityNotEqual

    static CondValue equalityNotEqual_long (long left,
					    long right)
    {
	return CondValue ((unsigned long) (left != right ? 1 : 0));
    }

    static CondValue equalityNotEqual_ulong (unsigned long left,
					     unsigned long right)
    {
	return CondValue ((unsigned long) (left != right ? 1 : 0));
    }

    static CondValue equalityNotEqual (CondValue const &left,
				       CondValue const &right)
    {
	return operation <equalityNotEqual_long, equalityNotEqual_ulong> (left, right);
    }

  // bitwiseAnd

    static CondValue bitwiseAnd_long (long left,
				      long right)
    {
	return CondValue ((long) (left & right));
    }

    static CondValue bitwiseAnd_ulong (unsigned long left,
				       unsigned long right)
    {
	return CondValue ((unsigned long) (left & right));
    }

    static CondValue bitwiseAnd (CondValue const &left,
				 CondValue const &right)
    {
	return operation <bitwiseAnd_long, bitwiseAnd_ulong> (left, right);
    }

  // exclusiveOr

    static CondValue exclusiveOr_long (long left,
				       long right)
    {
	return CondValue ((long) (left ^ right));
    }

    static CondValue exclusiveOr_ulong (unsigned long left,
					unsigned long right)
    {
	return CondValue ((unsigned long) (left ^ right));
    }

    static CondValue exclusiveOr (CondValue const &left,
				  CondValue const &right)
    {
	return operation <exclusiveOr_long, exclusiveOr_ulong> (left, right);
    }

  // inclusiveOr

    static CondValue inclusiveOr_long (long left,
				       long right)
    {
	return CondValue ((long) (left | right));
    }

    static CondValue inclusiveOr_ulong (unsigned long left,
					unsigned long right)
    {
	return CondValue ((unsigned long) (left | right));
    }

    static CondValue inclusiveOr (CondValue const &left,
				  CondValue const &right)
    {
	return operation <inclusiveOr_long, inclusiveOr_ulong> (left, right);
    }

  // logicalAnd

    static CondValue logicalAnd_long (long left,
				      long right)
    {
	return CondValue ((unsigned long) (left && right ? 1 : 0));
    }

    static CondValue logicalAnd_ulong (unsigned long left,
				       unsigned long right)
    {
	return CondValue ((unsigned long) (left && right ? 1 : 0));
    }

    static CondValue logicalAnd (CondValue const &left,
				 CondValue const &right)
    {
	return operation <logicalAnd_long, logicalAnd_ulong> (left, right);
    }

  // logicalOr

    static CondValue logicalOr_long (long left,
				     long right)
    {
	return CondValue ((unsigned long) (left || right ? 1 : 0));
    }

    static CondValue logicalOr_ulong (unsigned long left,
				      unsigned long right)
    {
	return CondValue ((unsigned long) (left || right ? 1 : 0));
    }

    static CondValue logicalOr (CondValue const &left,
				CondValue const &right)
    {
	return operation <logicalOr_long, logicalOr_ulong> (left, right);
    }


    CondValue (long long_value)
    {
	value_type = Long;
	this->long_value = long_value;
    }

    CondValue (unsigned long ulong_value)
    {
	value_type = UnsignedLong;
	this->ulong_value = ulong_value;
    }
};

static long strToLong (char const *str)
{
    Int64 val = 0;
    strToInt64 (str, &val, NULL);
    return (long) val;
}

static unsigned long strToUlong (char const *str)
{
    Uint64 val = 0;
    strToUint64 (str, &val, NULL);
    return (unsigned long) val;
}

static CondValue
evaluateCond_expression (CppPreprocessor * const self,
			 CppCondElement  * const element)
    throw (ParsingException,
	   InternalException)
{
    switch (element->cpp_cond_element_type) {
	case CppCondElement::t_PrimaryExpression: {
//	    errf->print ("--- primary-expression").pendl ();
	    CppCond_PrimaryExpression *_expr = static_cast <CppCond_PrimaryExpression*> (element);
	    switch (_expr->primary_expression_type) {
		case CppCond_PrimaryExpression::t_Literal: {
		    CppCond_PrimaryExpression_Literal *expr =
			    static_cast <CppCond_PrimaryExpression_Literal*> (_expr);
		    Literal *literal = static_cast <Literal*> (expr->literal->any_token->user_obj);
 		    switch (literal->literal_type) {
			case LiteralInteger: {
			    long long_value = strToLong (literal->str->cstr());
			    assert (long_value >= 0);
			    unsigned long ulong_value = strToUlong (literal->str->cstr());
			    if (ulong_value > (unsigned long) long_value)
				return CondValue (ulong_value);
			    else
				return CondValue (long_value);
			} break;
			case LiteralCharacter: {
			    // TODO
			    unreachable ();
			} break;
			case LiteralFloating: {
			    throw ParsingException (literal->fpos,
						    st_grab (new (std::nothrow) String (
                                                            "floating literals are not allowed"
                                                            "in preprocessor controlling expressions")));
			} break;
			case LiteralString: {
			    throw ParsingException (literal->fpos,
						    st_grab (new (std::nothrow) String (
                                                            "string literals are not allowed "
                                                            "in preprocessor controlling expressions")));
			} break;
			case LiteralBoolean: {
			    if (equal (literal->str->mem(), "true"))
				return CondValue ((unsigned long) 1);
			    else
			    if (equal (literal->str->mem(), "false"))
				return CondValue ((unsigned long) 0);
			    else
				unreachable ();
			} break;
		    }

		    unreachable ();
		    return CondValue ((unsigned long) 0);
		} break;
		case CppCond_PrimaryExpression::t_Braces: {
		    CppCond_PrimaryExpression_Braces *expr =
			    static_cast <CppCond_PrimaryExpression_Braces*> (_expr);
		    return evaluateCond_expression (self, expr->expression);
		} break;
		default:
		    unreachable ();
	    }
	} break;
	case CppCondElement::t_UnaryExpression: {
//	    errf->print ("--- unary-expression").pendl ();
	    CppCond_UnaryExpression *_expr = static_cast <CppCond_UnaryExpression*> (element);
	    switch (_expr->unary_expression_type) {
		case CppCond_UnaryExpression::t_Expression: {
		    CppCond_UnaryExpression_Expression *expr =
			    static_cast <CppCond_UnaryExpression_Expression*> (_expr);
		    return evaluateCond_expression (self, expr->expression);
		} break;
		case CppCond_UnaryExpression::t_UnaryOperator: {
		    CppCond_UnaryExpression_UnaryOperator *expr =
			    static_cast <CppCond_UnaryExpression_UnaryOperator*> (_expr);
		    switch (expr->unaryOperator->unary_operator_type) {
			case CppCond_UnaryOperator::t_Plus: {
			    return CondValue::unaryPlus (evaluateCond_expression (self, expr->expression));
			} break;
			case CppCond_UnaryOperator::t_Minus: {
			    return CondValue::unaryMinus (evaluateCond_expression (self, expr->expression));
			} break;
			case CppCond_UnaryOperator::t_Not: {
			    return CondValue::unaryNot (evaluateCond_expression (self, expr->expression));
			} break;
			case CppCond_UnaryOperator::t_Inversion: {
			    return CondValue::unaryInversion (evaluateCond_expression (self, expr->expression));
			} break;
			default:
			    unreachable ();
		    }
		} break;
		case CppCond_UnaryExpression::t_Defined: {
		    CppCond_UnaryExpression_Defined * const expr =
			    static_cast <CppCond_UnaryExpression_Defined*> (_expr);

		    MapBase< StRef<MacroDefinition> >::Entry mdef_entry =
			    self->macro_definitions.lookup (expr->identifier->any_token->token);
		    if (!mdef_entry.isNull ()) {
			errs->println (_func, ": Defined: true");
			return 1ul;
		    }

		    return 0ul;
		} break;
		case CppCond_UnaryExpression::t_DefinedBraces: {
		    CppCond_UnaryExpression_DefinedBraces * const expr =
			    static_cast <CppCond_UnaryExpression_DefinedBraces*> (_expr);

		    MapBase< StRef<MacroDefinition> >::Entry mdef_entry =
			    self->macro_definitions.lookup (expr->identifier->any_token->token);
		    if (!mdef_entry.isNull ()) {
			errs->println (_func, ": DefinedBraces: true");
			return 1ul;
		    }

		    return 0ul;
		} break;
		default:
		    unreachable ();
	    }
	} break;
	case CppCondElement::t_MultiplicativeExpression: {
//	    errf->print ("--- multiplicative-expression").pendl ();
	    CppCond_MultiplicativeExpression *_expr = static_cast <CppCond_MultiplicativeExpression*> (element);
	    switch (_expr->multiplicative_expression_type) {
		case CppCond_MultiplicativeExpression::t_Expression: {
		    CppCond_MultiplicativeExpression_Expression *expr =
			    static_cast <CppCond_MultiplicativeExpression_Expression*> (_expr);
		    return evaluateCond_expression (self, expr->expression);
		} break;
		case CppCond_MultiplicativeExpression::t_Multiply: {
		    CppCond_MultiplicativeExpression_Multiply *expr =
			    static_cast <CppCond_MultiplicativeExpression_Multiply*> (_expr);
		    return CondValue::multiplicativeMultiply (evaluateCond_expression (self, expr->left),
							      evaluateCond_expression (self, expr->right));
		} break;
		case CppCond_MultiplicativeExpression::t_Divide: {
		    CppCond_MultiplicativeExpression_Divide *expr =
			    static_cast <CppCond_MultiplicativeExpression_Divide*> (_expr);
		    return CondValue::multiplicativeDivide (evaluateCond_expression (self, expr->left),
							    evaluateCond_expression (self, expr->right));
		} break;
		case CppCond_MultiplicativeExpression::t_Remainder: {
		    CppCond_MultiplicativeExpression_Remainder *expr =
			    static_cast <CppCond_MultiplicativeExpression_Remainder*> (_expr);
		    return CondValue::multiplicativeRemainder (evaluateCond_expression (self, expr->left),
							       evaluateCond_expression (self, expr->right));
		} break;
		default:
		    unreachable ();
	    }
	} break;
	case CppCondElement::t_AdditiveExpression: {
//	    errf->print ("--- additive-expression").pendl ();
	    CppCond_AdditiveExpression *_expr = static_cast <CppCond_AdditiveExpression*> (element);
	    switch (_expr->additive_expression_type) {
		case CppCond_AdditiveExpression::t_Expression: {
		    CppCond_AdditiveExpression_Expression *expr =
			    static_cast <CppCond_AdditiveExpression_Expression*> (_expr);
		    return evaluateCond_expression (self, expr->expression);
		} break;
		case CppCond_AdditiveExpression::t_Plus: {
		    CppCond_AdditiveExpression_Plus *expr =
			    static_cast <CppCond_AdditiveExpression_Plus*> (_expr);
		    return CondValue::additivePlus (evaluateCond_expression (self, expr->left),
						    evaluateCond_expression (self,expr->right));
		} break;
		case CppCond_AdditiveExpression::t_Minus: {
		    CppCond_AdditiveExpression_Minus *expr =
			    static_cast <CppCond_AdditiveExpression_Minus*> (_expr);
		    return CondValue::additiveMinus (evaluateCond_expression (self, expr->left),
						     evaluateCond_expression (self, expr->right));
		} break;
		default:
		    unreachable ();
	    }
	} break;
	case CppCondElement::t_ShiftExpression: {
//	    errf->print ("--- shift-expression").pendl ();
	    CppCond_ShiftExpression *_expr = static_cast <CppCond_ShiftExpression*> (element);
	    switch (_expr->shift_expression_type) {
		case CppCond_ShiftExpression::t_Expression: {
		    CppCond_ShiftExpression_Expression *expr =
			    static_cast <CppCond_ShiftExpression_Expression*> (_expr);
		    return evaluateCond_expression (self, expr->expression);
		} break;
		case CppCond_ShiftExpression::t_LeftShift: {
		    CppCond_ShiftExpression_LeftShift *expr =
			    static_cast <CppCond_ShiftExpression_LeftShift*> (_expr);
		    return CondValue::shiftLeft (evaluateCond_expression (self, expr->left),
						 evaluateCond_expression (self, expr->right));
		} break;
		case CppCond_ShiftExpression::t_RightShift: {
		    CppCond_ShiftExpression_RightShift *expr =
			    static_cast <CppCond_ShiftExpression_RightShift*> (_expr);
		    return CondValue::shiftRight (evaluateCond_expression (self, expr->left),
						  evaluateCond_expression (self, expr->right));
		} break;
		default:
		    unreachable ();
	    }
	} break;
	case CppCondElement::t_RelationalExpression: {
//	    errf->print ("--- relational-expression").pendl ();
	    CppCond_RelationalExpression *_expr = static_cast <CppCond_RelationalExpression*> (element);
	    switch (_expr->relational_expression_type) {
		case CppCond_RelationalExpression::t_Expression: {
		    CppCond_RelationalExpression_Expression *expr =
			    static_cast <CppCond_RelationalExpression_Expression*> (_expr);
		    return evaluateCond_expression (self, expr->expression);
		} break;
		case CppCond_RelationalExpression::t_Less: {
		    CppCond_RelationalExpression_Less *expr =
			    static_cast <CppCond_RelationalExpression_Less*> (_expr);
		    return CondValue::relationalLess (evaluateCond_expression (self, expr->left),
						      evaluateCond_expression (self, expr->right));
		} break;
		case CppCond_RelationalExpression::t_Greater: {
		    CppCond_RelationalExpression_Greater *expr =
			    static_cast <CppCond_RelationalExpression_Greater*> (_expr);
		    return CondValue::relationalGreater (evaluateCond_expression (self, expr->left),
							 evaluateCond_expression (self, expr->right));
		} break;
		case CppCond_RelationalExpression::t_LessOrEqual: {
		    CppCond_RelationalExpression_LessOrEqual *expr =
			    static_cast <CppCond_RelationalExpression_LessOrEqual*> (_expr);
		    return CondValue::relationalLessOrEqual (evaluateCond_expression (self, expr->left),
							     evaluateCond_expression (self, expr->right));
		} break;
		case CppCond_RelationalExpression::t_GreaterOrEqual: {
		    CppCond_RelationalExpression_GreaterOrEqual *expr =
			    static_cast <CppCond_RelationalExpression_GreaterOrEqual*> (_expr);
		    return CondValue::relationalGreaterOrEqual (evaluateCond_expression (self, expr->left),
								evaluateCond_expression (self, expr->right));
		} break;
		default:
		    unreachable ();
	    }
	} break;
	case CppCondElement::t_EqualityExpression: {
//	    errf->print ("--- equality-expression").pendl ();
	    CppCond_EqualityExpression *_expr = static_cast <CppCond_EqualityExpression*> (element);
	    switch (_expr->equality_expression_type) {
		case CppCond_EqualityExpression::t_Expression: {
		    CppCond_EqualityExpression_Expression *expr =
			    static_cast <CppCond_EqualityExpression_Expression*> (_expr);
		    return evaluateCond_expression (self, expr->expression);
		} break;
		case CppCond_EqualityExpression::t_Equal: {
		    CppCond_EqualityExpression_Equal *expr =
			    static_cast <CppCond_EqualityExpression_Equal*> (_expr);
		    return CondValue::equalityEqual (evaluateCond_expression (self, expr->left),
						     evaluateCond_expression (self, expr->right));
		} break;
		case CppCond_EqualityExpression::t_NotEqual: {
		    CppCond_EqualityExpression_NotEqual *expr =
			    static_cast <CppCond_EqualityExpression_NotEqual*> (_expr);
		    return CondValue::equalityNotEqual (evaluateCond_expression (self, expr->left),
							evaluateCond_expression (self, expr->right));
		} break;
		default:
		    unreachable ();
	    }
	} break;
	case CppCondElement::t_AndExpression: {
//	    errf->print ("--- and-expression").pendl ();
	    CppCond_AndExpression *_expr = static_cast <CppCond_AndExpression*> (element);
	    switch (_expr->and_expression_type) {
		case CppCond_AndExpression::t_Expression: {
		    CppCond_AndExpression_Expression *expr =
			    static_cast <CppCond_AndExpression_Expression*> (_expr);
		    return evaluateCond_expression (self, expr->expression);
		} break;
		case CppCond_AndExpression::t_And: {
		    CppCond_AndExpression_And *expr =
			    static_cast <CppCond_AndExpression_And*> (_expr);
		    return CondValue::bitwiseAnd (evaluateCond_expression (self, expr->left),
						  evaluateCond_expression (self, expr->right));
		} break;
		default:
		    unreachable ();
	    }
	} break;
	case CppCondElement::t_ExclusiveOrExpression: {
//	    errf->print ("--- exclusive-or-expression").pendl ();
	    CppCond_ExclusiveOrExpression *_expr = static_cast <CppCond_ExclusiveOrExpression*> (element);
	    switch (_expr->exclusive_or_expression_type) {
		case CppCond_ExclusiveOrExpression::t_Expression: {
		    CppCond_ExclusiveOrExpression_Expression *expr =
			    static_cast <CppCond_ExclusiveOrExpression_Expression*> (_expr);
		    return evaluateCond_expression (self, expr->expression);
		} break;
		case CppCond_ExclusiveOrExpression::t_ExclusiveOr: {
		    CppCond_ExclusiveOrExpression_ExclusiveOr *expr =
			    static_cast <CppCond_ExclusiveOrExpression_ExclusiveOr*> (_expr);
		    return CondValue::exclusiveOr (evaluateCond_expression (self, expr->left),
						   evaluateCond_expression (self, expr->right));
		} break;
		default:
		    unreachable ();
	    }
	} break;
	case CppCondElement::t_InclusiveOrExpression: {
//	    errf->print ("--- inclusive-or-expression").pendl ();
	    CppCond_InclusiveOrExpression *_expr = static_cast <CppCond_InclusiveOrExpression*> (element);
	    switch (_expr->inclusive_or_expression_type) {
		case CppCond_InclusiveOrExpression::t_Expression: {
		    CppCond_InclusiveOrExpression_Expression *expr =
			    static_cast <CppCond_InclusiveOrExpression_Expression*> (_expr);
		    return evaluateCond_expression (self, expr->expression);
		} break;
		case CppCond_InclusiveOrExpression::t_InclusiveOr: {
		    CppCond_InclusiveOrExpression_InclusiveOr *expr =
			    static_cast <CppCond_InclusiveOrExpression_InclusiveOr*> (_expr);
		    return CondValue::inclusiveOr (evaluateCond_expression (self, expr->left),
						   evaluateCond_expression (self, expr->right));
		} break;
		default:
		    unreachable ();
	    }
	} break;
	case CppCondElement::t_LogicalAndExpression: {
//	    errf->print ("--- logical-and-expression").pendl ();
	    CppCond_LogicalAndExpression *_expr = static_cast <CppCond_LogicalAndExpression*> (element);
	    switch (_expr->logical_and_expression_type) {
		case CppCond_LogicalAndExpression::t_Expression: {
		    CppCond_LogicalAndExpression_Expression *expr =
			    static_cast <CppCond_LogicalAndExpression_Expression*> (_expr);
		    return evaluateCond_expression (self, expr->expression);
		} break;
		case CppCond_LogicalAndExpression::t_LogicalAnd: {
		    CppCond_LogicalAndExpression_LogicalAnd *expr =
			    static_cast <CppCond_LogicalAndExpression_LogicalAnd*> (_expr);
		    return CondValue::logicalAnd (evaluateCond_expression (self, expr->left),
						  evaluateCond_expression (self, expr->right));
		} break;
		default:
		    unreachable ();
	    }
	} break;
	case CppCondElement::t_LogicalOrExpression: {
//	    errf->print ("--- logical-or-expression").pendl ();
	    CppCond_LogicalOrExpression *_expr = static_cast <CppCond_LogicalOrExpression*> (element);
	    switch (_expr->logical_or_expression_type) {
		case CppCond_LogicalOrExpression::t_Expression: {
		    CppCond_LogicalOrExpression_Expression *expr =
			    static_cast <CppCond_LogicalOrExpression_Expression*> (_expr);
		    return evaluateCond_expression (self, expr->expression);
		} break;
		case CppCond_LogicalOrExpression::t_LogicalOr: {
		    CppCond_LogicalOrExpression_LogicalOr *expr =
			    static_cast <CppCond_LogicalOrExpression_LogicalOr*> (_expr);
		    return CondValue::logicalOr (evaluateCond_expression (self, expr->left),
						 evaluateCond_expression (self, expr->right));
		} break;
		default:
		    unreachable ();
	    }
	} break;
	case CppCondElement::t_ConditionalExpression: {
//	    errf->print ("--- conditional-expression").pendl ();
	    CppCond_ConditionalExpression *_expr = static_cast <CppCond_ConditionalExpression*> (element);
	    switch (_expr->conditional_expression_type) {
		case CppCond_ConditionalExpression::t_Expression: {
		    CppCond_ConditionalExpression_Expression *expr =
			    static_cast <CppCond_ConditionalExpression_Expression*> (_expr);
		    return evaluateCond_expression (self, expr->expression);
		} break;
		case CppCond_ConditionalExpression::t_Conditional: {
		    CppCond_ConditionalExpression_Conditional *expr =
			    static_cast <CppCond_ConditionalExpression_Conditional*> (_expr);
		    if (evaluateCond_expression (self, expr->conditionExpression).isTrue ())
			return evaluateCond_expression (self, expr->ifExpression);
		    else
			return evaluateCond_expression (self, expr->elseExpression);
		} break;
		default:
		    unreachable ();
	    }
	} break;
	default:
	    unreachable ();
    }

    unreachable ();
    return CondValue ((unsigned long) 0);
}

static bool
evaluateCond (CppPreprocessor * const self,
	      CppCond_Grammar * const grammar)
    throw (ParsingException,
	   InternalException)
{
    CondValue val = evaluateCond_expression (self, grammar->expression);
    switch (val.value_type) {
	case CondValue::Long: {
//	    errf->print ("--- evaluateCond: long: ").print (val.long_value).pendl ();
	    return val.long_value != 0;
	} break;
	case CondValue::UnsignedLong: {
//	    errf->print ("--- evaluateCond: ulong: ").print (val.ulong_value).pendl ();
	    return val.ulong_value != 0;
	} break;
	default:
            unreachable ();
    }

    // Unreachable
    unreachable ();
    return true;
}

void
CppPreprocessor::do_translateIfDirective (PpItemStream *pp_stream,
					  bool elif)
    throw (ParsingException,
	   InternalException)
{
    assert (pp_stream);

  // Translating all tokens up to a newline.

    FilePosition if_fpos = pp_stream->getFpos ();

    if (elif) {
	if (if_stack.last == NULL)
	    throw ParsingException (if_fpos, st_grab (new (std::nothrow) String ("misplaced #elif")));

	if (if_stack.last->data.state == IfStackEntry::Else)
	    throw ParsingException (if_fpos, st_grab (new (std::nothrow) String ("#elif after #else")));
    }

    List< StRef<PpItem> > pp_items;
    {
	bool defined_guard = false;
	for (;;) {
	    StRef<Whitespace> whsp;

	    pp_stream->getWhitespace (&whsp);
	    if (whsp) {
		if (whsp->has_newline)
		    break;

		pp_items.append (whsp.ptr ());
	    }

	    StRef<PpToken> pp_token;
	    pp_stream->getPpToken (&pp_token);
	    if (!pp_token)
		break;

	    // Parameter for 'defined' should be protected from macro expansion.
	    if (equal (pp_token->str->mem(), "defined")) {
		defined_guard = true;
		pp_items.append (pp_token);
	    } else {
		if (!defined_guard)
		    translatePpToken (pp_stream, pp_token, &pp_items);
		else
		    pp_items.append (pp_token);

		if (!equal (pp_token->str->mem(), "(")) {
		    defined_guard = false;
		}
	    }
	}
    }

    {
      // Converting identifiers into zeroes.
	List< StRef<PpItem> >::Iterator pp_item_iter (pp_items);
	bool defined_guard = false;
	while (!pp_item_iter.done ()) {
	    List< StRef<PpItem> >::Element &cur_el = pp_item_iter.next ();
	    // Note that we hold a reference to pp_item because we should
	    // be able to replace 'cur_el.data' with a different PpItem.
	    StRef<PpItem> pp_item = cur_el.data;

	    if (pp_item->type == PpItemPpToken) {
		PpToken *pp_token = static_cast <PpToken*> (pp_item.ptr ());

		bool defined_keyword = false;
		switch (pp_token->pp_token_type) {
		    case PpTokenIdentifier: {
			// Handling the 'defined' operator.
			// The macro name which is to be checked should be protected as well,
			// so a more complex check is necessary: we should protect the next
			// token from substitution, or, if the next token is an opening brace,
			// then we should protect the one after the brace.
			if (equal (pp_token->str->mem(), "defined")) {
			    defined_guard = true;
			    defined_keyword = true;
			} else {
			    if (!defined_guard) {
				cur_el.data =
                                        st_grab (new (std::nothrow) PpToken (
                                                PpTokenPpNumber,
                                                st_grab (new (std::nothrow) String ("0")),
                                                NULL /* macro_ban */,
                                                pp_token->fpos));
                            }
			}
		    } break;
		    case PpTokenStringLiteral: {
			throw ParsingException (if_fpos,
						st_grab (new (std::nothrow) String (
                                                        "string literals are not allowed "
                                                        "in preprocessor control expressions")));
		    } break;
		    default: {
		      // No-op
		    }
		}

		if (!defined_keyword) {
		    if (!equal (pp_token->str->mem(), "("))
			defined_guard = false;
		}
	    }
	}
    }

#if 0
    Ref<PpItemStream> tmp_pp_stream = grab (static_cast <PpItemStream*> (
						new ListPpItemStream (pp_items.first, FilePosition ())));
    Ref<TokenStream> token_stream = grab (static_cast <TokenStream*> (
						new PpItemStreamTokenStream (tmp_pp_stream)));
#endif

    List< StRef<Token> > token_list;
    ppItemsToTokens (&pp_items, &token_list);

    StRef<ListTokenStream> token_stream =
	    st_grab (new (std::nothrow) ListTokenStream (&token_list));

    StRef<Grammar> grammar = create_cpp_cond_grammar ();
    optimizeGrammar (grammar);

    StRef<StReferenced> cpp_element_container;
    ParserElement *cpp_element = NULL;
    parse (token_stream,
	   NULL,
	   NULL /* user_data */,
	   grammar,
	   &cpp_element,
           &cpp_element_container,
	   "default",
	   Pargen::createDefaultParserConfig (),
	   false /* debug_dump */);

#if 0
    errf->print ("--- #if:").pendl ();

    List< Ref<ParserElement> >::DataIterator grammar_iter (cpp_list);
    while (!grammar_iter.done ()) {
	Ref<ParserElement> _grammar = grammar_iter.next ();
	CppCond_Grammar * const &grammar = static_cast <CppCond_Grammar*> (_grammar.ptr ());
	dump_cpp_cond_grammar (grammar);
    }
#endif

    {
        ConstMemory token;
        if (cpp_element) {
            if (!token_stream->getNextToken (&token))
                throw ParsingException (if_fpos, st_grab (new (std::nothrow) String ("condition parsing error (tail)")));
        }

        if (cpp_element == NULL ||
            token.len() > 0)
        {
            throw ParsingException (if_fpos, st_grab (new (std::nothrow) String ("condition parsing error")));
        }
    }

    if (if_skipping) {
	if (!elif) {
	  // #if
	    if_stack.append (IfStackEntry (IfStackEntry::If, IfStackEntry::Skipping));
	    return;
	}
    }

    if (elif) {
      // #elif
	if (if_stack.last->data.match_type == IfStackEntry::Skipping) {
	    assert (if_skipping);
	    if_stack.last->data.state = IfStackEntry::Elif;
	    return;
	}
    }

    bool should_evaluate = true;
    if (elif) {
      // #elif
	if (if_stack.last->data.match_type == IfStackEntry::Match ||
	    if_stack.last->data.match_type == IfStackEntry::PrvMatch)
	{
	    should_evaluate = false;
	}
    }

    bool match = false;
    if (should_evaluate)
	match = evaluateCond (this, static_cast <CppCond_Grammar*> (cpp_element));

    if_skipping = !match;

    if (!elif) {
      // #if
	if (match)
	    if_stack.append (IfStackEntry (IfStackEntry::If, IfStackEntry::Match));
	else
	    if_stack.append (IfStackEntry (IfStackEntry::If, IfStackEntry::NoMatch));
    } else {
      // #elif
	if (match) {
	    assert (if_stack.last->data.match_type == IfStackEntry::NoMatch);
	    if_stack.last->data.match_type = IfStackEntry::Match;
	}

	if_stack.last->data.state = IfStackEntry::Elif;
    }
}

void
CppPreprocessor::translateIfDirective (PpItemStream *pp_stream)
    throw (ParsingException,
	   InternalException)
{
    do_translateIfDirective (pp_stream, false /* elif */);
}

void
CppPreprocessor::translateElifDirective (PpItemStream *pp_stream)
    throw (ParsingException,
	   InternalException)
{
    assert (pp_stream);

    do_translateIfDirective (pp_stream, true /* elif */);
}

void
CppPreprocessor::do_translateIfdefDirective (PpItemStream *pp_stream,
					     bool inverse)
    throw (InternalException,
	   ParsingException)
{
    assert (pp_stream);

    StRef<PpToken> pp_token;
    PpItemStream::PpItemResult pres = pp_stream->getNextPpToken (&pp_token);
    if (pres != PpItemStream::PpItemNormal)
	throw ParsingException (pp_stream->getFpos (),
				st_grab (new (std::nothrow) String ("pp-token expected")));

    StRef<Whitespace> whsp;
    pres = pp_stream->getWhitespace (&whsp);
    if (pres != PpItemStream::PpItemNormal)
	throw ParsingException (pp_stream->getFpos (),
				st_grab (new (std::nothrow) String ("newline expected")));

    if (!whsp->has_newline)
	throw ParsingException (whsp->fpos,
				st_grab (new (std::nothrow) String ("newline expected")));

    if (if_skipping) {
	if_stack.append (IfStackEntry (IfStackEntry::If, IfStackEntry::Skipping));
	return;
    }

    bool match = !macro_definitions.lookup (pp_token->str->mem()).isNull ();
    if (inverse)
	match = !match;

    if (match)
	if_stack.append (IfStackEntry (IfStackEntry::If, IfStackEntry::Match));
    else {
	if_stack.append (IfStackEntry (IfStackEntry::If, IfStackEntry::NoMatch));
	if_skipping = true;
    }
}

void
CppPreprocessor::translateIfdefDirective (PpItemStream *pp_stream)
    throw (InternalException,
	   ParsingException)
{
    do_translateIfdefDirective (pp_stream, false);
}

void
CppPreprocessor::translateIfndefDirective (PpItemStream *pp_stream)
    throw (InternalException,
	   ParsingException)
{
    do_translateIfdefDirective (pp_stream, true);
}

void
CppPreprocessor::translateElseDirective (PpItemStream *pp_stream)
    throw (InternalException,
	   ParsingException)
{
    assert (pp_stream);

    FilePosition else_fpos = pp_stream->getFpos ();

    StRef<Whitespace> whsp;
    pp_stream->getWhitespace (&whsp);
    if (!whsp)
	throw ParsingException (pp_stream->getFpos (),
				st_grab (new (std::nothrow) String ("newline expected")));

    if (!whsp->has_newline)
	throw ParsingException (whsp->fpos,
				st_grab (new (std::nothrow) String ("newline expected")));

    if (if_stack.isEmpty ())
	throw ParsingException (else_fpos,
				st_grab (new (std::nothrow) String ("misplaced #else")));

    if (if_stack.last->data.state == IfStackEntry::Else)
	throw ParsingException (else_fpos,
				st_grab (new (std::nothrow) String ("#else after #else")));

    if_stack.last->data.state = IfStackEntry::Else;

    if (if_stack.last->data.match_type == IfStackEntry::Skipping) {
	assert (if_skipping);
	return;
    }

    if (if_stack.last->data.match_type == IfStackEntry::Match ||
	if_stack.last->data.match_type == IfStackEntry::PrvMatch)
    {
	if_stack.last->data.match_type = IfStackEntry::PrvMatch;
	if_skipping = true;
	return;
    }

    assert (if_stack.last->data.match_type == IfStackEntry::NoMatch);

    if_stack.last->data.match_type = IfStackEntry::Match;
    if_skipping = false;
}
    
void
CppPreprocessor::translateEndifDirective (PpItemStream *pp_stream)
    throw (InternalException,
	   ParsingException)
{
    assert (pp_stream);

    FilePosition endif_fpos = pp_stream->getFpos ();

    StRef<Whitespace> whsp;
    pp_stream->getWhitespace (&whsp);
    if (!whsp ||
	!whsp->has_newline)
    {
	throw ParsingException (whsp->fpos,
				st_grab (new (std::nothrow) String ("newline expected")));
    }

    if (if_stack.isEmpty ()) {
	throw ParsingException (endif_fpos,
				st_grab (new (std::nothrow) String ("misplaced #endif")));
    }

    if_stack.remove (if_stack.last);

    if (if_stack.last != NULL) {
	if (if_stack.last->data.match_type == IfStackEntry::Match)
	    if_skipping = false;
	else
	    if_skipping = true;
    } else
	if_skipping = false;
}

static bool
checkIdentifierUniqueness (List< StRef<String> > *identifiers)
{
    if (identifiers == NULL)
	return true;

    List< StRef<String> >::Element *cur_to_check = identifiers->first;
    while (cur_to_check != NULL) {
	List< StRef<String> >::Element *next_to_check = cur_to_check->next;

	List< StRef<String> >::Element *cur = cur_to_check->next;
	while (cur != NULL) {
	    if (!cur_to_check->data ||
		cur_to_check->data->isNullString ())
	    {
		if (!cur->data ||
		    cur->data->isNullString ())
		{
		    return false;
		}
	    } else {
		if (cur->data &&
		    !cur->data->isNullString ())
		{
		    if (equal (cur_to_check->data->mem(), cur->data->mem()))
			return false;
		}
	    }

	    cur = cur->next;
	}

	cur_to_check = next_to_check;
    }

    return true;
}

void
CppPreprocessor::translateIncludeDirective (PpItemStream *pp_stream)
    throw (ParsingException,
	   InternalException)
{
    assert (pp_stream);

    DEBUG (
	errf->print ("Scurffy.CppPreprocessor.translateIncludeDirective").pendl ();
    )

    FilePosition fpos = pp_stream->getFpos ();

    const char *no_header_name_exc_str = "header-name expected";

    StRef<Whitespace> whsp;
    pp_stream->getWhitespace (&whsp);
    if (whsp &&
	whsp->has_newline)
    {
	throw ParsingException (fpos, st_grab (new (std::nothrow) String (no_header_name_exc_str)));
    }

    StRef<PpToken_HeaderName> header_name;
    PpItemStream::PpItemResult res = pp_stream->getHeaderName (&header_name);
    if (res != PpItemStream::PpItemNormal)
	throw ParsingException (fpos, st_grab (new (std::nothrow) String (no_header_name_exc_str)));

    if (header_name) {
	pp_stream->getWhitespace (&whsp);
	if (!whsp ||
	    !whsp->has_newline)
	{
	    // FIXME "Garbage after the header name"
	    throw ParsingException (fpos, st_grab (new (std::nothrow) String (no_header_name_exc_str)));
	}
    }

    if (!header_name) {
      // Here we expand all macros up to a newline and check if the resulting
      // preprocessing token sequence matches an h-header or q-header form.

	List< StRef<PpItem> > hn_items;
	for (;;) {
	    pp_stream->getWhitespace (&whsp);
	    if (whsp) {
		if (whsp->has_newline)
		    break;

		hn_items.append (whsp.ptr ());
	    }

	    StRef<PpToken> pp_token;
	    pp_stream->getPpToken (&pp_token);
	    if (!pp_token)
		break;

	    translatePpToken (pp_stream, pp_token, &hn_items);
	}

	StRef<String> hn_str = ppItemsToString (&hn_items);
	DEBUG (
	    errf->print ("Scruffy.CppPreprocessor.translateIncludeDirective: "
			 "spelling: ").print (hn_str).pendl ();
	)

	MemoryFile tmp_file (hn_str->mem());
	StRef<FileByteStream>      tmp_byte_stream    = st_grab (new (std::nothrow) FileByteStream (&tmp_file));
	StRef<Utf8UnicharStream>   tmp_unichar_stream = st_grab (new (std::nothrow) Utf8UnicharStream (tmp_byte_stream));
	StRef<UnicharPpItemStream> tmp_pp_stream      = st_grab (new (std::nothrow) UnicharPpItemStream (tmp_unichar_stream, pp_token_match_func));

	tmp_pp_stream->getHeaderName (&header_name);
	if (!header_name)
	    throw ParsingException (fpos, st_grab (new (std::nothrow) String (no_header_name_exc_str)));

	do { 
	    tmp_pp_stream->getWhitespace (&whsp);
	} while (whsp);

	if (tmp_pp_stream->getNextItem (NULL) != PpItemStream::PpItemEof)
	    // FIXME "Garbage after the header name"
	    throw ParsingException (fpos, st_grab (new (std::nothrow) String (no_header_name_exc_str)));
    }

    DEBUG (
	errf->print ("Scruffy.CppPreprocessor.translateIncludeDirective: "
		     "include: ").print (header_name->str).pendl ();
    )

    // TODO Actually include the file.
}

void
CppPreprocessor::translateDefineDirective (PpItemStream *pp_stream)
    throw (InternalException,
	   ParsingException)
{
    assert (pp_stream);

    DEBUG (
	errf->print ("Scruffy.translateDefineDirective")
	     .pendl ();
    )

    StRef<Whitespace> whsp;
    pp_stream->getWhitespace (&whsp);
    if (whsp) {
	if (whsp->has_newline)
	    throw ParsingException (whsp->fpos,
				    st_grab (new (std::nothrow) String ("no macro name given in #define directive")));
    }

    StRef<PpToken> pp_token;
    PpItemStream::PpItemResult pres = pp_stream->getPpToken (&pp_token);
    if (pres != PpItemStream::PpItemNormal)
	throw ParsingException (pp_stream->getFpos (),
				st_grab (new (std::nothrow) String ("identifier expected")));

    if (pp_token->pp_token_type != PpTokenIdentifier)
	throw ParsingException (pp_token->fpos,
				st_grab (new (std::nothrow) String ("identifier expected")));

    StRef<String> macro_name = pp_token->str;

    DEBUG (
	errf->print ("Scruffy.translateDefineDirective: macro name: ")
	     .print (macro_name)
	     .pendl ();
    )

    StRef< List_< StRef<String>, StReferenced > > params;
    bool lparen = false;

    StRef<PpItemStream::PositionMarker> pmark = pp_stream->getPosition ();
    pp_stream->getPpToken (&pp_token);
    if (pp_token &&
	equal (pp_token->str->mem(), "("))
    {
	params = extractMacroParameters (pp_stream);
	lparen = true;

      DEBUG (
	errf->print ("Scruffy.translateDefineDirective: parameters: ");
	List< Ref<String> >::Element *cur_param = params->first;
	while (cur_param != NULL) {
	    Ref<String> &str = cur_param->data;
	    errf->print ("\"")
		 .print (str)
		 .print ("\"");

	    cur_param = cur_param->next;

	    if (cur_param != NULL)
		errf->print (", ");
	}
	errf->pendl ();
      )
    } else
	pp_stream->setPosition (pmark);

    /* C++98 16.3#5
     * A parameter identifier in a function-like macro shall be uniquely
     * declared within its scope. */
    if (!checkIdentifierUniqueness (params))
	throw ParsingException (pp_stream->getFpos (pmark),
				st_grab (new (std::nothrow) String ("duplicate macro parameters")));

    FilePosition replacement_list_fpos = pp_stream->getFpos ();
    StRef< List_< StRef<PpItem>, StReferenced > > replacement_list = extractReplacementList (pp_stream);

    StRef<MacroDefinition> mdef = st_grab (new (std::nothrow) MacroDefinition);
    mdef->name = macro_name;
    mdef->replacement_list = replacement_list;
    mdef->params = params;
    mdef->lparen = lparen;

    MapBase< StRef<MacroDefinition> >::Entry prv_mdef_entry = macro_definitions.lookupValue (mdef);
    if (!prv_mdef_entry.isNull ()) {
	/* This is a redefinition of a previously defined macro.
	 * Check if we can redefine (C++98 16.3 #1 #2 #3). */

	StRef<MacroDefinition> &prv_mdef = prv_mdef_entry.getData ();

	if (!compareReplacementLists (mdef->replacement_list,
				      prv_mdef->replacement_list))
	{
	    throw ParsingException (replacement_list_fpos,
				    st_grab (new (std::nothrow) String ("invalid macro redefinition")));
	}

	if (mdef->lparen != prv_mdef->lparen)
	    throw ParsingException (replacement_list_fpos,
				    st_grab (new (std::nothrow) String ("invalid macro redefinition (lparen)")));

	if (mdef->params) {
	  /* Implies prv_mdef->params.isNull () */
	    List< StRef<String> >::Element *cur_param = mdef->params->first,
                                           *cur_param_prv = prv_mdef->params->first;
	    while (cur_param     != NULL &&
		   cur_param_prv != NULL)
	    {
		StRef<String> &param = cur_param->data,
                              &param_prv = cur_param_prv->data;

		if (!equal (param->mem(), param_prv->mem()))
		    break;

		cur_param = cur_param->next;
		cur_param_prv = cur_param_prv->next;
	    }

	    if (cur_param     != NULL ||
		cur_param_prv != NULL)
	    {
		throw ParsingException (replacement_list_fpos,
					st_grab (new (std::nothrow) String ("invalid macro redefinition (parameters)")));
	    }
	}
    } else {
	/* This is the first macro definition with such name. */
	macro_definitions.add (mdef);
    }
}

void
CppPreprocessor::translateUndefDirective (PpItemStream *pp_stream)
    throw (InternalException,
	   ParsingException)
{
    assert (pp_stream);

    DEBUG (
	errf->print ("Scruffy.CppParser.translateUndefDirective")
	     .pendl ();
    )

    StRef<Whitespace> whsp;
    pp_stream->getWhitespace (&whsp);
    if (whsp) {
	if (whsp->has_newline)
	    throw ParsingException (whsp->fpos,
				    st_grab (new (std::nothrow) String ("identifier expected #1")));
    }

    StRef<PpToken> pp_token;
    PpItemStream::PpItemResult pres = pp_stream->getPpToken (&pp_token);
    if (pres != PpItemStream::PpItemNormal)
	throw ParsingException (pp_stream->getFpos (),
				st_grab (new (std::nothrow) String ("identifier expected #2")));

    if (pp_token->pp_token_type != PpTokenIdentifier)
	throw ParsingException (pp_token->fpos,
				st_grab (new (std::nothrow) String ("identifier expected #2")));

    StRef<String> macro_name = pp_token->str;

    pp_stream->getWhitespace (&whsp);
    if (!whsp ||
	!whsp->has_newline)
    {
	throw ParsingException (whsp->fpos,
				st_grab (new (std::nothrow) String ("newline expected")));
    }

    MapBase< StRef<MacroDefinition> >::Entry mdef_entry = macro_definitions.lookup (macro_name->mem());
    if (!mdef_entry.isNull ()) {
	DEBUG (
	    errf->print ("Scruffy.CppParser.translateUndefDirecrive: "
			 "undefining macro \"")
		 .print (macro_name)
		 .print ("\"")
		 .pendl ();
	)

	macro_definitions.remove (mdef_entry);
    }
}

void
CppPreprocessor::translatePreprocessingDirective (PpItemStream *pp_stream)
    throw (InternalException,
	   ParsingException)
{
    assert (pp_stream);

    DEBUG (
	errf->print ("Scruffy.translatePreprocessingDirective")
	     .pendl ();
    )

    StRef<Whitespace> whsp;
    pp_stream->getWhitespace (&whsp);
    if (whsp) {
	if (whsp->has_newline) {
	    DEBUG (
		errf->print ("Scruffy.translatePreprocessingDirective: "
			     "null directive")
		     .pendl ();
	    )

	    return;
	}
    }

    StRef<PpToken> pp_token;
    PpItemStream::PpItemResult pres = pp_stream->getPpToken (&pp_token);
    if (pres != PpItemStream::PpItemNormal)
	throw ParsingException (pp_stream->getFpos (),
				st_grab (new (std::nothrow) String ("pp-token expected")));

    StRef<String> pp_directive_name = pp_token->str;

    if (equal (pp_directive_name->mem(), "if")) {
	DEBUG (
	    errf->print ("Scruffy.translatePreprocessingDirective: "
			 "\"if\" directive")
		 .pendl ();
	)

	translateIfDirective (pp_stream);
	return;
    } else
    if (equal (pp_directive_name->mem(), "ifdef")) {
	DEBUG (
	    errf->print ("Scruffy.translatePreprocessingDirective: "
			 "\"ifdef\" directive")
		 .pendl ();
	)

	translateIfdefDirective (pp_stream);
	return;
    } else
    if (equal (pp_directive_name->mem(), "ifndef")) {
	DEBUG (
	    errf->print ("Scruffy.translatePreprocessingDirective: "
			 "\"ifndef\" directive")
		 .pendl ();
	)

	translateIfndefDirective (pp_stream);
	return;
    } else
    if (equal (pp_directive_name->mem(), "elif")) {
	DEBUG (
	    errf->print ("Scruffy.translatePreprocessingDirective: "
			 "\"elif\" directive")
		 .pendl ();
	)

	translateElifDirective (pp_stream);
	return;
    } else
    if (equal (pp_directive_name->mem(), "else")) {
	DEBUG (
	    errf->print ("Scruffy.translatePreprocessingDirective: "
			 "\"else\" directive")
		 .pendl ();
	)

	translateElseDirective (pp_stream);
	return;
    } else
    if (equal (pp_directive_name->mem(), "endif")) {
	DEBUG (
	    errf->print ("Scruffy.translatePreprocessingDirective: "
			 "\"endif\" directive")
		 .pendl ();
	)

	translateEndifDirective (pp_stream);
	return;
    }

    if (if_skipping)
	return;

    if (equal (pp_directive_name->mem(), "include")) {
	DEBUG (
	    errf->print ("Scruffy.translatePreprocessingDirective: "
			 "\"include\" directive")
		 .pendl ();
	)

	translateIncludeDirective (pp_stream);
    } else
    if (equal (pp_directive_name->mem(), "define")) {
	DEBUG (
	    errf->print ("Scruffy.translatePreprocessingDirective: "
			 "\"define\" directive")
		 .pendl ();
	)

	translateDefineDirective (pp_stream);
    } else
    if (equal (pp_directive_name->mem(), "undef")) {
	DEBUG (
	    errf->print ("Scruffy.translatePreprocessingDirective: "
			 "\"undef\" directive")
		 .pendl ();
	)

	translateUndefDirective (pp_stream);
    } else
    if (equal (pp_directive_name->mem(), "line")) {
	DEBUG (
	    errf->print ("Scruffy.translatePreprocessingDirective: "
			 "\"line\" directive")
		 .pendl ();
	)

	// TODO
    } else
    if (equal (pp_directive_name->mem(), "error")) {
	DEBUG (
	    errf->print ("Scruffy.translatePreprocessingDirective: "
			 "\"error\" directive")
		 .pendl ();
	)

	// TODO Parse the error message and push it to the user.
	throw ParsingException (pp_stream->getFpos (), st_grab (new (std::nothrow) String ("#error")));
    } else
    if (equal (pp_directive_name->mem(), "pragma")) {
	DEBUG (
	    errf->print ("Scruffy.translatePreprocessingDirective: "
			 "\"pragma\" directive")
		 .pendl ();
	)

	// TODO
    } else {
	throw ParsingException (pp_token->fpos,
				st_makeString ("invalid preprocessing directive #",
                                               pp_directive_name));
    }
}

StRef< List_< StRef<PpItem>, StReferenced > >
CppPreprocessor::translateMacroInvocation (PpItemStream    *pp_stream,
					   MacroDefinition *mdef,
					   MacroBan        *macro_ban)
    throw (InternalException,
	   ParsingException)
{
    assert (pp_stream && mdef);

    assert (mdef->lparen);

    DEBUG (
	errf->print ("Scruffy.CppParser.translateMacroInvocation: macro name: \"")
	     .print (mdef->name)
	     .print ("\"")
	     .pendl ();
    )

    StRef<MacroBan> new_ban = st_grab (new (std::nothrow) MacroBan);
    new_ban->mdef = mdef;
    new_ban->outer_ban = macro_ban;
    new_ban->active = false;

  /* Preparing the arguments for substitution */

    FilePosition arguments_fpos = pp_stream->getFpos ();

    List< StRef< List_< StRef<PpItem>, StReferenced > > > arguments,
                                                          nonrepl_arguments;
    for (;;) {
      /* Extracting next argument */

	FilePosition start_fpos = pp_stream->getFpos ();
	StRef< List_< StRef<PpItem>, StReferenced > > arg_list = st_grab (new (std::nothrow) List_< StRef<PpItem>, StReferenced >);

	/* TODO Detect potential prepocessing directives (undefined behavior). */

	bool end_of_args = false;
	for (;;) {
	    StRef<PpItem> pp_item;
	    PpItemStream::PpItemResult pres = pp_stream->getNextItem (&pp_item);
	    if (pres != PpItemStream::PpItemNormal)
		throw ParsingException (pp_stream->getFpos (),
					st_grab (new (std::nothrow) String ("unexpected end of file #1")));

	    if (pp_item->type == PpItemWhitespace) {
		arg_list->append (pp_item, arg_list->last);
		continue;
	    }

	    assert (pp_item->type == PpItemPpToken);

	    PpToken *pp_token = static_cast <PpToken*> (pp_item.ptr ());

	    if (equal (pp_token->str->mem(), ")")) {
		end_of_args = true;
		break;
	    }

	    if (equal (pp_token->str->mem(), ","))
		break;

	    StRef<PpToken> new_token = st_grab (new (std::nothrow) PpToken (pp_token->pp_token_type,
                                                                            pp_token->str,
                                                                            new_ban,
                                                                            pp_token->fpos));
	    if (equal (pp_token->str->mem(), "(")) {
		unsigned long nopenings = 0;
		for (;;) {
		    if (equal (pp_token->str->mem(), "("))
			nopenings ++;
		    else
		    if (equal (pp_token->str->mem(), ")"))
			nopenings --;

		    DEBUG (
			errf->print ("Scruffy.CppParser.translateMacroInvocation: arg_list token #1: ")
			     .print (new_token->str)
			     .pendl ();
		    )

		    arg_list->append (new_token.ptr (), arg_list->last);

		    if (nopenings == 0)
			break;

		    for (;;) {
			pres = pp_stream->getNextItem (&pp_item);
			if (pres != PpItemStream::PpItemNormal)
			    throw ParsingException (pp_stream->getFpos (),
						    st_grab (new (std::nothrow) String ("unexpected end of file #2")));

			if (pp_item->type == PpItemWhitespace)
			    arg_list->append (pp_item, arg_list->last);
			else
			    break;
		    }

		    assert (pp_item->type == PpItemPpToken);

		    pp_token = static_cast <PpToken*> (pp_item.ptr ());

		    DEBUG (
			errf->print ("Scruffy.CppParser.translateMacroInvocation: "
				     "token #2: ")
			     .print (pp_token->str)
			     .pendl ();
		    )

		    new_token = st_grab (new (std::nothrow) PpToken (pp_token->pp_token_type,
                                                                     pp_token->str,
                                                                     new_ban,
                                                                     pp_token->fpos));
		}
	    } else {
		DEBUG (
		    errf->print ("Scruffy.CppParser.translateMacroInvocation: arg_list token #2: ")
			 .print (new_token->str)
			 .pendl ();
		)

		arg_list->append (new_token.ptr (), arg_list->last);
	    }
	}

	DEBUG (
	    errf->print ("Scruffy.CppParser.translateMacroInvocation: "
			 "performing macro replacement in the argument")
		 .pendl ();
	)

      /* (End of extracting next argument) */

      /* Performing macro replacement for the extracted argument */

	StRef< List_< StRef<PpItem>, StReferenced > > replaced_arg_list =
                st_grab (new List_< StRef<PpItem>, StReferenced >);
	StRef<PpItemStream> arg_pp_stream =
                st_grab (static_cast <PpItemStream*> (new (std::nothrow) ListPpItemStream (arg_list->first, start_fpos)));
	for (;;) {
	    StRef<PpItem> pp_item;
	    arg_pp_stream->getNextItem (&pp_item);
	    if (!pp_item)
		break;

	    if (pp_item->type == PpItemWhitespace) {
		replaced_arg_list->append (pp_item, replaced_arg_list->last);
	    } else
	    if (pp_item->type == PpItemPpToken) {
		PpToken *pp_token = static_cast <PpToken*> (pp_item.ptr ());
		translatePpToken (arg_pp_stream, pp_token, replaced_arg_list);
	    } else {
                unreachable ();
            }
	}

      /* (End of performing macro replacement for the extracted argument) */

      /* Checking that the argument is not empty */

	bool arg_is_empty = true;
	List< StRef<PpItem> >::Element *cur_item = replaced_arg_list->first;
	while (cur_item != NULL) {
	    if (cur_item->data->type == PpItemPpToken) {
		arg_is_empty = false;
		break;
	    }

	    cur_item = cur_item->next;
	}

	if (arg_is_empty) {
//	    DEBUG (
		errs->println ("Scruffy.CppParser.translateMacroInvocation: "
                               "argument contains no preprocessing tokens before "
                               "argument substitution (undefined behavior)");
//	    )

	    /* TODO throw ParsingException () ? */
	}

      /* (End of checking that the argument is not empty) */

	arguments.append (replaced_arg_list, arguments.last);
	nonrepl_arguments.append (arg_list, nonrepl_arguments.last);

	if (end_of_args)
	    break;
    }

    assert (arguments.getNumElements () == nonrepl_arguments.getNumElements ());

  /* (End of preparing the arguments for substitution) */

    new_ban->active = true;

    unsigned long nparams = 0;
    if (mdef->params)
	nparams = mdef->params->getNumElements ();

    if (arguments.getNumElements () != nparams) {
	throw ParsingException (arguments_fpos,
                                makeString ("bad number of arguments (",
                                            arguments.getNumElements(),
                                            " instead of ", nparams, ")"));
    }

    DEBUG (
	errf->print ("Scruffy.CppParser.translateMacroInvocation: "
		     "processing the replacement list")
	     .pendl ();
    )

    if (!mdef->replacement_list)
	return NULL;

    StRef< List_< StRef<PpItem>, StReferenced > > ret_list = st_grab (new (std::nothrow) List_< StRef<PpItem>, StReferenced >);

    bool sharp_encountered = false;

    List< StRef<PpItem> >::Element *cur_pp = mdef->replacement_list->first,
                                   *prv_pp_token_el = NULL;
    while (cur_pp != NULL) {
	StRef<PpItem> &rpl_item = cur_pp->data;
	if (rpl_item->type == PpItemWhitespace) {
	    if (!sharp_encountered)
		ret_list->append (rpl_item, ret_list->last);

	    cur_pp = cur_pp->next;
	    continue;
	}

	assert (rpl_item->type == PpItemPpToken);

	PpToken *rpl_token = static_cast <PpToken*> (rpl_item.ptr ());

	bool param_match = false;
	if (mdef->params) {
	    List< StRef<String> >::Element *param_el = mdef->params->first;
	    List< StRef< List_< StRef<PpItem>, StReferenced > > >::Element
		    *cur_arg_list = arguments.first,
		    *cur_nonrepl_arg_list = nonrepl_arguments.first;
	    while (param_el != NULL) {
		assert (cur_arg_list && cur_nonrepl_arg_list);

		if (equal (rpl_token->str->mem(), param_el->data->mem())) {
		  /* Checking for a neighboring sharp or double-sharp */

		    bool sharp = false,
			 double_sharp = false;

		    if (prv_pp_token_el != NULL) {
			PpToken *prv_pp_token = static_cast <PpToken*> (prv_pp_token_el->data.ptr ());
			if (equal (prv_pp_token->str->mem(), "##"))
			    double_sharp = true;
			else
			if (equal (prv_pp_token->str->mem(), "#"))
			    sharp = true;
		    }

		    List< StRef<PpItem> >::Element *cur_ff = cur_pp->next;
		    while (cur_ff != NULL) {
			StRef<PpItem> &pp_item_ff = cur_ff->data;
			if (pp_item_ff->type == PpItemPpToken) {
			    PpToken *pp_token_ff = static_cast <PpToken*> (pp_item_ff.ptr ());

			    if (equal (pp_token_ff->str->mem(), "##"))
				double_sharp = true;

			    break;
			}

			cur_ff = cur_ff->next;
		    }

		  /* (End of checking for a neighboring sharp or double-sharp) */

		    if (!sharp) {
			List< StRef<PpItem> >::Element *cur_arg_pp;
			if (!double_sharp)
			    cur_arg_pp = cur_arg_list->data->first;
			else
			    cur_arg_pp = cur_nonrepl_arg_list->data->first;

			while (cur_arg_pp != NULL) {
			    ret_list->append (cur_arg_pp->data, ret_list->last);
			    cur_arg_pp = cur_arg_pp->next;
			}
		    } else {
		      // A new valid character string literal should have been formed.

			DEBUG (
			    errf->print ("Scruffy.Preprocessor.translateMacroInvocation: "
					 "rematching new character string literal").pendl ();
			)

			sharp_encountered = false;

			StRef<String> spelling = spellPpItems (cur_nonrepl_arg_list->data);

			MemoryFile sp_file = MemoryFile (spelling->mem());
			StRef<ByteStream> sp_byte_stream = st_grab (static_cast <ByteStream*> (new (std::nothrow) FileByteStream (&sp_file)));
			StRef<UnicharStream> sp_unichar_stream = st_grab (static_cast <UnicharStream*> (new (std::nothrow) Utf8UnicharStream (sp_byte_stream)));

			PpTokenType sp_pp_token_type;
			unsigned long sp_pp_token_len = matchPreprocessingToken (sp_unichar_stream,
										 &sp_pp_token_type);
			if (sp_pp_token_len != spelling->len() ||
			    sp_pp_token_type != PpTokenStringLiteral)
			{
			    throw ParsingException (
				    arguments_fpos,
                                    st_makeString ("argument spelling is not a valid character string literal "
                                                   "(undefined behavior): ", spelling));
			}

			StRef<PpToken> pp_token = st_grab (new (std::nothrow) PpToken (PpTokenStringLiteral,
                                                                                       spelling,
                                                                                       new_ban,
                                                                                       rpl_token->fpos));
			ret_list->append (pp_token.ptr (), ret_list->last);
		    }

		    param_match = true;
		    break;
		}

		param_el = param_el->next;
		cur_arg_list = cur_arg_list->next;
		cur_nonrepl_arg_list = cur_nonrepl_arg_list->next;
	    }
	}

	if (!param_match) {
	    if (sharp_encountered)
		throw ParsingException (arguments_fpos,
					st_grab (new (std::nothrow) String ("# is not followed by a parameter in the replacement list")));

	    if (equal (rpl_token->str->mem(), "#")) {
		sharp_encountered = true;
	    } else {
		StRef<PpToken> new_token = st_grab (new (std::nothrow) PpToken (rpl_token->pp_token_type,
                                                                                rpl_token->str,
                                                                                new_ban,
                                                                                rpl_token->fpos));
		ret_list->append (new_token.ptr (), ret_list->last);
	    }
	}

	prv_pp_token_el = cur_pp;
	cur_pp = cur_pp->next;
    }

    if (sharp_encountered)
	throw ParsingException (arguments_fpos,
				st_grab (new (std::nothrow) String ("# is not followed by a parameter in the replacement list")));

    evaluateDoubleSharps (ret_list, new_ban);

    return ret_list;
}

/* This is a fictive translation method. All it does is
 * duplicating 'mdef->replacement_list' and updating
 * 'macro_ban' fields in the new list.
 */
StRef< List_< StRef<PpItem>, StReferenced > >
CppPreprocessor::translateObjectMacro (MacroDefinition *mdef,
				       MacroBan        *macro_ban)
    throw (InternalException,
	   ParsingException)
{
    DEBUG (
	errf->print ("Scruffy.CppParser.translateObjectMacro")
	     .pendl ();
    )

    assert (mdef);

    StRef<MacroBan> new_ban = st_grab (new (std::nothrow) MacroBan);
    new_ban->mdef = mdef;
    new_ban->outer_ban = macro_ban;
    new_ban->active = true;

    StRef< List_< StRef<PpItem>, StReferenced > > new_items;
    if (mdef->replacement_list) {
	/* We have to create a new list of tokens with
	 * '.macro_ban' set to 'new_ban'. */

	new_items = st_grab (new (std::nothrow) List_< StRef<PpItem>, StReferenced >);
	List< StRef<PpItem> >::Element *rpl_el = mdef->replacement_list->first;
	while (rpl_el != NULL) {
	    StRef<PpItem> &rpl_item = rpl_el->data;
	    if (rpl_item->type == PpItemWhitespace) {
		new_items->append (rpl_item, new_items->last);
	    } else
	    if (rpl_item->type == PpItemPpToken) {
		PpToken *rpl_token = static_cast <PpToken*> (rpl_item.ptr ());

		StRef<PpToken> new_token = st_grab (new (std::nothrow) PpToken (rpl_token->pp_token_type,
                                                                                rpl_token->str,
                                                                                new_ban,
                                                                                rpl_token->fpos));

		new_items->append (new_token.ptr (), new_items->last);
	    } else {
                unreachable ();
            }

	    rpl_el = rpl_el->next;
	}
    }

    evaluateDoubleSharps (new_items, new_ban);

    return new_items;
}

void
CppPreprocessor::evaluateDoubleSharps (List< StRef<PpItem> > *pp_items,
				       MacroBan *macro_ban)
    throw (InternalException,
	   ParsingException)
{
    if (pp_items == NULL)
	return;

    List< StRef<PpItem> >::Element *cur = pp_items->first,
                                   *prv_pp_token_el = NULL;
    while (cur) {
	StRef<PpItem> &pp_item = cur->data;
	if (pp_item->type != PpItemPpToken) {
	    cur = cur->next;
	    continue;
	}

	PpToken *pp_token = static_cast <PpToken*> (pp_item.ptr ());

	if (equal (pp_token->str->mem(), "##")) {
	    DEBUG (
		errf->print ("Scruffy.CppParser.evaluateDoubleSharps: "
			     "## preprocessing token")
		     .pendl ();
	    )

	    if (prv_pp_token_el == NULL)
		throw ParsingException (pp_token->fpos,
					st_grab (new (std::nothrow) String ("## at the beginning of the replacement list")));

	    List< StRef<PpItem> >::Element *cur_ff = cur->next,
                                           *next_pp_token_el = NULL;
	    while (cur_ff != NULL) {
		StRef<PpItem> &pp_item_ff = cur_ff->data;
		if (pp_item_ff->type == PpItemPpToken) {
		    next_pp_token_el = cur_ff;
		    break;
		}

		cur_ff = cur_ff->next;
	    }

	    if (next_pp_token_el == NULL)
		throw ParsingException (pp_token->fpos,
					st_grab (new (std::nothrow) String ("## at the end of the replacement list")));

	    PpToken *prv_pp_token  = static_cast <PpToken*> (prv_pp_token_el->data.ptr ());
	    PpToken *next_pp_token = static_cast <PpToken*> (next_pp_token_el->data.ptr ());

	    StRef<String> new_token_str = st_makeString (prv_pp_token->str, next_pp_token->str);
	    DEBUG (
		errf->print ("Scruffy.CppParser.evaluateDoubleSharps: "
			     "concatenation result: ")
		     .print (new_token_str)
		     .pendl ();
	    )

	    MemoryFile ns_file = MemoryFile (new_token_str->mem());
	    StRef<ByteStream> ns_byte_stream = st_grab (static_cast <ByteStream*> (new (std::nothrow) FileByteStream (&ns_file)));
	    StRef<UnicharStream> ns_unichar_stream = st_grab (static_cast <UnicharStream*> (new (std::nothrow) Utf8UnicharStream (ns_byte_stream)));

	    DEBUG (
		errf->print ("Scruffy.Preprocessor.evaluateDoubleSharps: "
			     "rematching concatenation result: ").print (new_token_str).pendl ();
	    )

	    PpTokenType new_pp_token_type;
	    unsigned long new_pp_token_len = matchPreprocessingToken (ns_unichar_stream,
								      &new_pp_token_type);
	    if (new_pp_token_len != new_token_str->len()) {
		throw ParsingException (prv_pp_token->fpos,
					st_grab (new (std::nothrow) String ("the result of concatenation is not a valid "
                                                                            "preprocessing token (undefined behavior)")));
	    }

	    StRef<PpToken> new_token = st_grab (new (std::nothrow) PpToken (new_pp_token_type,
                                                                            new_token_str,
                                                                            macro_ban,
                                                                            prv_pp_token->fpos));

	    List< StRef<PpItem> >::Element *new_pp_token_el =
		    pp_items->prepend (new_token.ptr (), prv_pp_token_el);

	    List< StRef<PpItem> >::Element *cur_rm = prv_pp_token_el;
	    for (;;) {
		List< StRef<PpItem> >::Element *next_rm = cur_rm->next;
		pp_items->remove (cur_rm);
		if (cur_rm == next_pp_token_el)
		    break;

		cur_rm = next_rm;
	    }

	    prv_pp_token_el = new_pp_token_el;
	    cur = new_pp_token_el->next;
	} else {
	    prv_pp_token_el = cur;
	    cur = cur->next;
	}
    }
}

void
CppPreprocessor::translatePpToken (PpItemStream *pp_stream,
				   PpToken      *_pp_token,
				   List< StRef<PpItem> > *receiving_pp_list)
    throw (InternalException,
	   ParsingException)
{
    assert (pp_stream && _pp_token && receiving_pp_list);

    DEBUG (
	errf->print ("Scruffy.CppParser.translatePpToken: \"")
	     .print (_pp_token->str)
	     .print ("\"")
	     .pendl ();
    )

    StRef<Phase3ItemStream> phase3_stream = st_grab (new (std::nothrow) Phase3ItemStream (pp_stream));

    StRef<PpToken> pp_token = _pp_token;

    /* TODO "...nest level up to an implementation-defined limit":
     * introduce a limit for nest level. */
    for (;;) {
	DEBUG (
	    errf->print ("Scruffy.CppParser.translatePpToken: iteration: \"")
		 .print (pp_token->str)
		 .print ("\"")
		 .pendl ();
	)

	bool no_macro = true;
	MapBase< StRef<MacroDefinition> >::Entry mdef_entry = macro_definitions.lookup (pp_token->str->mem());
	if (!mdef_entry.isNull ()) {
	    StRef<MacroDefinition> &mdef = mdef_entry.getData ();

	    bool banned = false;
	    StRef<MacroBan> macro_ban = pp_token->macro_ban;
	    while (macro_ban) {
		if (macro_ban->active &&
		    equal (macro_ban->mdef->name->mem(), mdef->name->mem()))
		{
		    banned = true;
		    break;
		}

		macro_ban = macro_ban->outer_ban;
	    }

	    if (!banned) {
		if (mdef->lparen) {
		    StRef<PpItemStream::PositionMarker> pmark = phase3_stream->getPosition ();

		    StRef<PpToken> ppt;
		    phase3_stream->getNextPpToken (&ppt);
		    if (ppt &&
			equal (ppt->str->mem(), "("))
		    {
			no_macro = false;

			StRef< List_< StRef<PpItem>, StReferenced > >
				minv_items = translateMacroInvocation (phase3_stream,
								       mdef,
								       pp_token->macro_ban);

			phase3_stream->trim ();
			if (minv_items)
			    phase3_stream->prependItems (minv_items);
		    } else
			phase3_stream->setPosition (pmark);
		} else {
		    no_macro = false;

		    StRef< List_< StRef<PpItem>, StReferenced > >
				omacro_items = translateObjectMacro (mdef,
								     pp_token->macro_ban);

		    phase3_stream->trim ();
		    if (omacro_items)
			phase3_stream->prependItems (omacro_items);
		}
	    } else {
		DEBUG (
		    errf->print ("Scruffy.CppParser.translatePpToken: banned")
			 .pendl ();
		)
	    }
	}

	if (no_macro) {
	    receiving_pp_list->append (pp_token.ptr ());
	}

	if (!phase3_stream->hasPrependedItems ())
	    break;

	for (;;) {
	    StRef<PpItem> pp_item;
	    phase3_stream->getNextItem (&pp_item);
	    assert (pp_item);

	    if (pp_item->type == PpItemWhitespace) {
		receiving_pp_list->append (pp_item);
	    } else
	    if (pp_item->type == PpItemPpToken) {
		pp_token = static_cast <PpToken*> (pp_item.ptr ());
		break;
	    } else {
                unreachable ();
            }
	}

	phase3_stream->trim ();
    }

    DEBUG (
	errf->print ("Scruffy.CppParser.translatePpToken: done")
	     .pendl ();
    )
}

void
CppPreprocessor::predefineMacros ()
{
    /* TODO
     * __LINE__
     * __FILE__
     * __DATE__
     * __TIME__
     */

    StRef<MacroDefinition> mdef;
   
    mdef = st_grab (new (std::nothrow) MacroDefinition);
    mdef->name = st_grab (new (std::nothrow) String ("__cplusplus"));
    mdef->lparen = false;
    mdef->replacement_list = st_grab (new (std::nothrow) List_< StRef<PpItem>, StReferenced >);
    mdef->replacement_list->append (
	    StRef<PpItem> (
		st_grab (new (std::nothrow) PpToken (PpTokenPpNumber,
                                                     StRef<String> (st_grab (new (std::nothrow) String ("199711L"))),
                                                     NULL,
                                                     FilePosition ()))),
	    mdef->replacement_list->last);

    macro_definitions.add (mdef);
}

void 
CppPreprocessor::translationPhase3 (File *in_file)
    throw (InternalException,
	   ParsingException)
{
    assert (in_file);

    DEBUG (
	errf->print ("Scruffy.CppParser.translationPhase3")
	     .pendl ();
    )

  /* Initialization */

    macro_definitions.clear ();
    predefineMacros ();

    if_stack.clear ();
    if_skipping = false;

  /* (End of initialization) */

    StRef<ByteStream> byte_stream = st_grab (static_cast <ByteStream*> (new (std::nothrow) FileByteStream (in_file)));
    StRef<UnicharStream> unichar_stream = st_grab (static_cast <UnicharStream*> (new (std::nothrow) Utf8UnicharStream (byte_stream)));
    StRef<PpItemStream> pp_stream = st_grab (static_cast <PpItemStream*> (new (std::nothrow) UnicharPpItemStream (unichar_stream, pp_token_match_func)));

    /* 'true' if we are at a point where a preprocessing directive can start. */
//    bool pp_directive_start = true;
    for (;;) {
	StRef<Whitespace> whsp;
	DEBUG (
	    errf->print ("Scruffy.CppParser.translationPhase3: calling pp_stream->getWhitespace()")
		 .pendl ();
	)
	pp_stream->getWhitespace (&whsp);
	if (whsp) {
//	    if (whsp->has_newline)
//		pp_directive_start = true;

	    pp_items->append (whsp.ptr (), pp_items->last);

	    DEBUG (
		errf->print ("Scruffy.CppParser.translationPhase3: got some whitespace")
		     .pendl ();
	    )
	}

	StRef<PpToken> pp_token;
	PpItemStream::PpItemResult pres;
	DEBUG (
	    errf->print ("Scruffy.CppParser.translationPhase3: calling pp_stream->getPpToken()")
		 .pendl ();
	)
	pres = pp_stream->getPpToken (&pp_token);
	if (pres == PpItemStream::PpItemEof) {
	    if (!if_stack.isEmpty ())
		throw ParsingException (pp_stream->getFpos (),
					st_grab (new (std::nothrow) String ("unterminated #if")));

	    DEBUG (
		errf->print ("Scruffy.CppParser.translationPhase3: "
			     "the file has been parsed successfully")
		     .pendl ();
	    )

	    break;
	}

	assert (pp_token);

	DEBUG (
	    errf->print ("Scruffy.CppParser.translationPhase3: got a PpToken: \"")
		 .print (pp_token->str->getData ())
		 .print ("\"")
		 .pendl ();
	)

	if (equal (pp_token->str->mem(), "#")) {
	    translatePreprocessingDirective (pp_stream);
//	    pp_directive_start = true;
	} else {
	    if (!if_skipping)
		translatePpToken (pp_stream, pp_token, pp_items);

//	    pp_directive_start = false;
	}
    }
}

Result
CppPreprocessor::performPreprocessing ()
{
    /* 4 Kb pages */
//    Ref< DynamicTreeArray<char> > phase3_array = grab (new DynamicTreeArray<char> (12));
//    Ref<File> phase3_in_file = source_file;
//    Ref<File> phase3_out_file = grab (new ArrayFile (phase3_array, true));

//    translationPhase3 (phase3_in_file, phase3_out_file);

    try {
        translationPhase3 (source_file);
    } catch (ParsingException &e) {
        errs->println (_func, "ParsingException: ", e.toString());
        exc_throw (ParsingException, e);
        return Result::Failure;
    } catch (...) {
        errs->println (_func, "exception");
        exc_throw (ParsingException, FilePosition(), st_grab (new (std::nothrow) String ("preprocessing error")));
        return Result::Failure;
    }

    return Result::Success;
}

CppPreprocessor::CppPreprocessor (File             * const source_file,
				  PpTokenMatchFunc   const pp_token_match_func)
    : pp_token_match_func (pp_token_match_func)
{
    assert (source_file);

    this->source_file = source_file;

    if_skipping = false;

    pp_items = st_grab (new (std::nothrow) List_< StRef<PpItem>, StReferenced >);
}

}

