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

#include <scruffy/pp_item_stream.h>


// Internal
#define DEBUG_INT(a)


using namespace M;

namespace Scruffy {
    
PpItemStream::PpItemResult
PpItemStream::getWhitespace (StRef<Whitespace> *whsp)
    throw (InternalException,
	   ParsingException)
{
    StRef<PositionMarker> pmark = getPosition ();

  DEBUG_INT (
    errf->print ("Scruffy.PpItemStream.getWhitespace: "
		 "calling getNextItem()")
	 .pendl ();
  );

    StRef<PpItem> pp_item;
    PpItemResult pres;
    pres = getNextItem (&pp_item);
    if (pres != PpItemNormal) {
	if (whsp != NULL)
	    *whsp = NULL;

	setPosition (pmark);
	return pres;
    }

    if (pp_item->type == PpItemWhitespace) {
      DEBUG_INT (
	errf->print ("Scruffy.PpItemStream.getWhitespace: "
		     "got whitespace")
	     .pendl ();
      );

	if (whsp != NULL)
	    *whsp = static_cast <Whitespace*> (pp_item.ptr ());

	return PpItemNormal;
    }

  DEBUG_INT (
    errf->print ("Scruffy.PpItemStream.getWhitespace: "
		 "got something which is not whitespace")
	 .pendl ();
  );

    if (whsp != NULL)
	*whsp = NULL;

    setPosition (pmark);
    return PpItemNormal;
}

PpItemStream::PpItemResult
PpItemStream::getPpToken (StRef<PpToken> *pp_token)
    throw (InternalException,
	   ParsingException)
{
    StRef<PositionMarker> pmark = getPosition ();

    StRef<PpItem> pp_item;
    PpItemResult pres;
    pres = getNextItem (&pp_item);
    if (pres != PpItemNormal) {
	if (pp_token != NULL)
	    *pp_token = NULL;

	setPosition (pmark);
	return pres;
    }

    if (pp_item->type == PpItemPpToken) {
	if (pp_token != NULL)
	    *pp_token = static_cast <PpToken*> (pp_item.ptr ());

	return PpItemNormal;
    }

    if (pp_token != NULL)
	*pp_token = NULL;

    setPosition (pmark);
    return PpItemNormal;
}

PpItemStream::PpItemResult
PpItemStream::getNextPpToken (StRef<PpToken> *pp_token)
    throw (InternalException,
	   ParsingException)
{
    StRef<PositionMarker> pmark = getPosition ();

    for (;;) {
	StRef<PpItem> pp_item;
	PpItemResult pres;
	pres = getNextItem (&pp_item);
	if (pres != PpItemNormal) {
	    if (pp_token != NULL)
		*pp_token = NULL;

	    setPosition (pmark);
	    return pres;
	}

	if (pp_item->type == PpItemPpToken) {
	    if (pp_token != NULL)
		*pp_token = static_cast <PpToken*> (pp_item.ptr ());

	    return PpItemNormal;
	}
    }

    unreachable ();

    if (pp_token != NULL)
	*pp_token = NULL;

    setPosition (pmark);
    return PpItemNormal;
}

}

