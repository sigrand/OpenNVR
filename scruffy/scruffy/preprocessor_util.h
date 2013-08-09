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


#ifndef SCRUFFY_PREPROCESSOR_UTIL__H__
#define SCRUFFY_PREPROCESSOR_UTIL__H__


#include <libmary/libmary.h>

#include <scruffy/unichar_stream.h>

#include <scruffy/preprocessor_types.h>
#include <scruffy/parsing_exception.h>


namespace Scruffy {

using namespace M;

StRef<String> extractString (UnicharStream *unichar_stream,
                             unsigned long len)
                      throw (InternalException,
                             ParsingException);

unsigned long matchDigit (UnicharStream *unichar_stream)
		   throw (InternalException,
			  ParsingException);

unsigned long matchOctalDigit (UnicharStream *unichar_stream)
			throw (InternalException,
			       ParsingException);

unsigned long matchHexadecimalDigit (UnicharStream *unichar_stream)
			      throw (InternalException,
				     ParsingException);

unsigned long matchNondigit (UnicharStream *unichar_stream)
		      throw (InternalException,
			     ParsingException);

unsigned long matchWhitespace (UnicharStream *unichar_stream,
			       bool *contains_newline)
			throw (InternalException,
			       ParsingException);

unsigned long matchHeaderName (UnicharStream *unichar_stream)
			throw (InternalException,
			       ParsingException);

unsigned long matchIdentifier (UnicharStream *unichar_stream)
			throw (InternalException,
			       ParsingException);

unsigned long matchPpNumber (UnicharStream *unichar_stream)
		      throw (InternalException,
			     ParsingException);

unsigned long matchSimpleEscapeSequence (UnicharStream *unichar_stream)
				  throw (InternalException,
					 ParsingException);

unsigned long matchOctalEscapeSequence (UnicharStream *unichar_stream)
				 throw (InternalException,
					ParsingException);

unsigned long matchHexadecimalEscapeSequence (UnicharStream *unichar_stream)
				       throw (InternalException,
					      ParsingException);

unsigned long matchEscapeSequence (UnicharStream *unichar_stream)
			    throw (InternalException,
				   ParsingException);

unsigned long matchUniversalCharacterName (UnicharStream *unichar_stream)
				    throw (InternalException,
					   ParsingException);

unsigned long matchCChar (UnicharStream *unichar_stream)
		   throw (InternalException,
			  ParsingException);

unsigned long matchCCharSequence (UnicharStream *unichar_stream)
			   throw (InternalException,
				  ParsingException);

unsigned long matchCharacterLiteral (UnicharStream *unichar_stream)
			      throw (InternalException,
				     ParsingException);

unsigned long matchSChar (UnicharStream *unichar_stream)
		   throw (InternalException,
			  ParsingException);

unsigned long matchSCharSequence (UnicharStream *unichar_stream)
			   throw (InternalException,
				  ParsingException);

unsigned long matchStringLiteral (UnicharStream *unichar_stream)
			   throw (InternalException,
				  ParsingException);

unsigned long matchPreprocessingOpOrPunc (UnicharStream *unichar_stream)
				   throw (InternalException,
					  ParsingException);

Size matchHeaderName (UnicharStream *unichar_stream,
		      PpToken_HeaderName::HeaderNameType *ret_hn_type,
		      StRef<String> *ret_header_name)
	       throw (InternalException,
		      ParsingException);

// mt_throws ((InternalException, ParsingException))
unsigned long matchPreprocessingToken (UnicharStream *unichar_stream,
				       PpTokenType *pp_token_type);

bool compareReplacementLists (List< StRef<PpItem> > *left,
			      List< StRef<PpItem> > *right);

StRef<String> ppItemsToString (List< StRef<PpItem> > *pp_items)
                        throw (ParsingException,
                               InternalException);

StRef<String> spellPpItems (List< StRef<PpItem> > *pp_items)
                     throw (InternalException,
                            ParsingException);

StRef<String> unescapeStringLiteral (String * mt_nonnull string);

StRef<Token> ppTokenToToken (PpToken *pp_token)
                      throw (ParsingException);

void ppTokensToTokens (List< StRef<PpToken> > *pp_tokens  /* non-null */,
		       List< StRef<Token> >   *ret_tokens /* non-null */);

void ppItemsToTokens (List< StRef<PpItem> > *pp_items   /* non-null */,
		      List< StRef<Token> >  *ret_tokens /* non-null */);

}


#endif /* SCRUFFY_PREPROCESSOR_UTIL__H__ */

