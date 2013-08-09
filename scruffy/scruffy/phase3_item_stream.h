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


#ifndef SCRUFFY__PHASE3_ITEM_STREAM__H__
#define SCRUFFY__PHASE3_ITEM_STREAM__H__


#include <libmary/libmary.h>

#include <scruffy/pp_item_stream.h>


namespace Scruffy {

using namespace M;

class Phase3ItemStream : public PpItemStream
{
protected:
    class PositionMarker : public PpItemStream::PositionMarker
    {
    public:
	List< StRef<PpItem> >::Element *pp_el;
	StRef<PpItemStream::PositionMarker> stream_pmark;
    };

    List< StRef<PpItem> > pp_items;
    StRef<PpItemStream> pp_stream;

    List< StRef<PpItem> >::Element *cur_pp;
    StRef<PpItemStream::PositionMarker> cur_pmark;

public:
  /* PpItemStream interface */

    PpItemResult getNextItem (StRef<PpItem> *pp_item)
			    throw (InternalException,
				   ParsingException);

    PpItemResult getHeaderName (StRef<PpToken_HeaderName> *ret_hn_token)
			 throw (ParsingException,
				InternalException);

    StRef<PpItemStream::PositionMarker> getPosition ()
			    throw (InternalException);

    void setPosition (PpItemStream::PositionMarker *pmark)
			    throw (InternalException);

    Pargen::FilePosition getFpos (PpItemStream::PositionMarker *pmark);

    Pargen::FilePosition getFpos ();

  /* (End of PpItemStream interface) */

    void prependItems (List< StRef<PpItem> > *items);

    void trim ();

    bool hasPrependedItems ();

    Phase3ItemStream (PpItemStream *pp_stream)
	       throw (InternalException);
};

}


#endif /* SCRUFFY__PHASE3_ITEM_STREAM__H__ */

