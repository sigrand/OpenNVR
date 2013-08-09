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


#ifndef SCRUFFY__PREPROCESSOR_TYPES__H__
#define SCRUFFY__PREPROCESSOR_TYPES__H__


#include <libmary/libmary.h>

#include <pargen/file_position.h>


namespace Scruffy {

using namespace M;

enum PpTokenType
{
    PpTokenSingleChar,
    PpTokenHeaderName,
    PpTokenPpOpOrPunc,
    PpTokenIdentifier,
    PpTokenPpNumber,
    PpTokenCharacterLiteral,
    PpTokenStringLiteral,
    PpTokenInvalid
};

enum PpItemType
{
    PpItemPpToken,
    PpItemWhitespace
};

class MacroDefinition;

class MacroBan : public StReferenced
{
public:
    StRef<MacroBan> outer_ban;
    StRef<MacroDefinition> mdef;
    bool active;
};

class PpItem : public StReferenced
{
public:
    const PpItemType type;

    const Pargen::FilePosition fpos;

    StRef<String> str;

    PpItem (PpItemType type,
	    const Pargen::FilePosition &fpos)
	: type (type),
	  fpos (fpos)
    {
    }
};

class Whitespace : public PpItem
{
public:
    bool has_newline;

    Whitespace (String *str,
		bool    has_newline,
		const Pargen::FilePosition &fpos)
	: PpItem (PpItemWhitespace, fpos)
    {
	this->str = str;
	this->has_newline = has_newline;
    }
};

class PpToken : public PpItem
{
public:
    PpTokenType pp_token_type;
    StRef<MacroBan> macro_ban;

    PpToken (PpTokenType  pp_token_type,
	     String      *str,
	     MacroBan    *macro_ban,
	     const Pargen::FilePosition &fpos)
	: PpItem (PpItemPpToken, fpos)
    {
	this->pp_token_type = pp_token_type;
	this->str = str;
	this->macro_ban = macro_ban;
    }
};

class PpToken_HeaderName : public PpToken
{
public:
    enum HeaderNameType {
	HeaderNameType_H,
	HeaderNameType_Q
    };

    HeaderNameType header_name_type;

    PpToken_HeaderName (HeaderNameType header_name_type,
			String *str,
			MacroBan *macro_ban,
			const Pargen::FilePosition &fpos)
	: PpToken (PpTokenHeaderName,
		   str,
		   macro_ban,
		   fpos)
    {
	this->header_name_type = header_name_type;
    }
};

enum TokenType
{
    TokenIdentifier = 0,
    TokenKeyword,
    TokenLiteral,
    TokenOperator,
    TokenPunctuator
};

enum LiteralType
{
    LiteralInteger = 0,
    LiteralCharacter,
    LiteralFloating,
    LiteralString,
    LiteralBoolean
};

class Token : public StReferenced
{
public:
    TokenType token_type;
    StRef<String> str;

    const Pargen::FilePosition fpos;

    Token (TokenType token_type,
	   String *str,
	   const Pargen::FilePosition &fpos)
	: fpos (fpos)
    {
	this->token_type = token_type;
	this->str = str;
    }
};

class Literal : public Token
{
public:
    LiteralType literal_type;

    Literal (LiteralType literal_type,
	     String *str,
	     const Pargen::FilePosition &fpos)
	: Token (TokenLiteral, str, fpos)
    {
	this->literal_type = literal_type;
    }
};

}


#include <scruffy/macro_definition.h>


#endif /* SCRUFFY__PREPROCESSOR_TYPES__H__ */

