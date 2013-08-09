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

#include <scruffy/unichar_pp_item_stream.h>
#include <scruffy/preprocessor_util.h>


// Internal
#define DEBUG_INT(a) ;


using namespace M;

namespace Scruffy {
    
PpItemStream::PpItemResult
UnicharPpItemStream::getNextItem (StRef<PpItem> *pp_item)
    throw (InternalException,
	   ParsingException)
{
    unichar_stream->setPosition (cur_pmark);
    const Pargen::FilePosition fpos = unichar_stream->getFpos (cur_pmark);

  DEBUG_INT (
    errf->print ("Scruffy.UnicharPpItemStream.getNextItem: "
		 "calling matchWhitespace ()")
	 .pendl ();
  );

    bool newline;
    unsigned long whsp_len = matchWhitespace (unichar_stream, &newline);
    if (whsp_len > 0) {
	unichar_stream->setPosition (cur_pmark);
	StRef<String> whsp_str = extractString (unichar_stream, whsp_len);
	cur_pmark = unichar_stream->getPosition ();

      DEBUG_INT (
	errf->print ("Scruffy.UnicharPpItemStream.getNextItem: "
		     "whitespace: ")
	     .print (whsp_len)
	     .print (" bytes, ")
	     .print (newline ? "with newline" : "without newline")
	     .pendl ();
      );

	if (pp_item != NULL)
	    *pp_item = st_grab (new (std::nothrow) Whitespace (whsp_str, newline, fpos));

	return PpItemNormal;
    }

  DEBUG_INT (
    errf->print ("Scruffy.UnicharPpItemStream.getNextItem: "
		 "restoring stream position")
	 .pendl ();
  );

    unichar_stream->setPosition (cur_pmark);

  DEBUG_INT (
    errf->print ("Scruffy.UnicharPpItemStream.getNextItem: "
		 "no whitespace, calling matchPreprocessingToken ()")
	 .pendl ();
  );

    PpTokenType pp_token_type;
//    unsigned long pp_token_len = matchPreprocessingToken (unichar_stream, &pp_token_type);
    unsigned long pp_token_len = pp_token_match_func (unichar_stream, &pp_token_type);
    if (pp_token_len == 0) {
	if (pp_item != NULL)
	    *pp_item = NULL;

      /* Determining if an EOF is reached */

	unichar_stream->setPosition (cur_pmark);
	UnicharStream::UnicharResult ures;
	ures = unichar_stream->getNextUnichar (NULL);
	if (ures == UnicharStream::UnicharEof) {
	  DEBUG_INT (
	    errf->print ("Scruffy.UnicharPpItemStream.getNextItem: "
			 "returning PpItemEof")
		 .pendl ();
	  );

	    return PpItemEof;
	}

	assert (ures == UnicharStream::UnicharNormal);

      /* (End of determining if an EOF is reached) */

      DEBUG_INT (
	errf->print ("Scruffy.UnicharPpItemStream.getNextItem: "
		     "pp-token expected")
	     .pendl ();
      );

	throw ParsingException (fpos, NULL);
    }

    unichar_stream->setPosition (cur_pmark);
    StRef<String> pp_token_str = extractString (unichar_stream, pp_token_len);
    cur_pmark = unichar_stream->getPosition ();

  DEBUG_INT (
    errf->print ("Scruffy.UnicharPpItemStream.getNextItem: "
		 "pp-token: \"")
	 .print (pp_token_str)
	 .print ("\"")
	 .pendl ();
  );

    if (pp_token_type == PpTokenStringLiteral)
	pp_token_str = unescapeStringLiteral (pp_token_str);

    if (pp_item != NULL)
	*pp_item = st_grab (new (std::nothrow) PpToken (pp_token_type, pp_token_str, NULL, fpos));

    return PpItemNormal;
}

PpItemStream::PpItemResult
UnicharPpItemStream::getHeaderName (StRef<PpToken_HeaderName> *ret_hn_token)
    throw (ParsingException,
	   InternalException)
{
    if (ret_hn_token != NULL)
	*ret_hn_token = NULL;

    unichar_stream->setPosition (cur_pmark);
    const Pargen::FilePosition fpos = unichar_stream->getFpos (cur_pmark);

    PpToken_HeaderName::HeaderNameType hn_type;
    StRef<String> header_name;
    Size header_name_len = matchHeaderName (unichar_stream, &hn_type, &header_name);
    if (header_name_len > 0) {
	unichar_stream->setPosition (cur_pmark);
	unichar_stream->skip (header_name_len);
	cur_pmark = unichar_stream->getPosition ();

	if (ret_hn_token != NULL)
	    *ret_hn_token = st_grab (new (std::nothrow) PpToken_HeaderName (hn_type, header_name, NULL, fpos));

	return PpItemNormal;
    }

    unichar_stream->setPosition (cur_pmark);
    UnicharStream::UnicharResult res = unichar_stream->getNextUnichar (NULL);
    if (res == UnicharStream::UnicharEof)
	return PpItemEof;

    assert (res == UnicharStream::UnicharNormal);

    return PpItemNormal;
}

StRef<PpItemStream::PositionMarker>
UnicharPpItemStream::getPosition ()
    throw (InternalException)
{
    StRef<PpItemStream::PositionMarker> ret_pmark = st_grab (static_cast <PpItemStream::PositionMarker*> (new (std::nothrow) PositionMarker));

    PositionMarker *pmark = static_cast <PositionMarker*> (ret_pmark.ptr ());
    pmark->unichar_pmark = cur_pmark;

    return ret_pmark;
}

void
UnicharPpItemStream::setPosition (PpItemStream::PositionMarker *_pmark)
    throw (InternalException)
{
    assert (_pmark);

    PositionMarker *pmark = static_cast <PositionMarker*> (_pmark);
    cur_pmark = pmark->unichar_pmark;
}

Pargen::FilePosition
UnicharPpItemStream::getFpos (PpItemStream::PositionMarker *_pmark)
{
    assert (_pmark);

    PositionMarker *pmark = static_cast <PositionMarker*> (_pmark);
    return unichar_stream->getFpos (pmark->unichar_pmark);
}

Pargen::FilePosition
UnicharPpItemStream::getFpos ()
{
    return unichar_stream->getFpos (cur_pmark);
}

UnicharPpItemStream::UnicharPpItemStream (UnicharStream * const unichar_stream,
					  CppPreprocessor::PpTokenMatchFunc const pp_token_match_func)
    throw (InternalException)
    : pp_token_match_func (pp_token_match_func)
{
    assert (unichar_stream);

    this->unichar_stream = unichar_stream;
    this->cur_pmark = unichar_stream->getPosition ();
}

}

