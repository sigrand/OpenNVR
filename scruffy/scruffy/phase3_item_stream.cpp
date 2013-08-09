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


#include <scruffy/phase3_item_stream.h>


using namespace M;
using namespace Pargen;

namespace Scruffy {
    
PpItemStream::PpItemResult
Phase3ItemStream::getNextItem (StRef<PpItem> *pp_item)
    throw (InternalException,
	   ParsingException)
{
    if (cur_pp != NULL) {
	List< StRef<PpItem> >::Element *next_pp = cur_pp->next;
	if (pp_item != NULL)
	    *pp_item = cur_pp->data;

	cur_pp = next_pp;
    } else {
	pp_stream->setPosition (cur_pmark);
	PpItemResult pres;
	pres = pp_stream->getNextItem (pp_item);
	if (pres != PpItemNormal)
	    return pres;

	cur_pmark = pp_stream->getPosition ();
    }

    return PpItemNormal;
}

PpItemStream::PpItemResult
Phase3ItemStream::getHeaderName (StRef<PpToken_HeaderName> * /* ret_hn_token */)
    throw (ParsingException,
	   InternalException)
{
    // TODO Is this necessary?

    // No-op

    unreachable ();

    return PpItemNormal;
}

StRef<PpItemStream::PositionMarker>
Phase3ItemStream::getPosition ()
    throw (InternalException)
{
    StRef<PpItemStream::PositionMarker> ret_pmark = st_grab (static_cast <PpItemStream::PositionMarker*> (new (std::nothrow) PositionMarker));

    PositionMarker *pmark = static_cast <PositionMarker*> (ret_pmark.ptr ());
    pmark->pp_el = cur_pp;
    if (cur_pp == NULL)
	pmark->stream_pmark = cur_pmark;

    return ret_pmark;
}

void
Phase3ItemStream::setPosition (PpItemStream::PositionMarker *_pmark)
    throw (InternalException)
{
    assert (_pmark);

    PositionMarker *pmark = static_cast <PositionMarker*> (_pmark);

    if (pmark->pp_el != NULL) {
	assert (!pmark->stream_pmark);
	assert (pp_items.first);

      /* DEBUG
       * Check that there is such an element in 'pp_items' list.
       */
	bool match = false;
	List< StRef<PpItem> >::Element *cur = pp_items.first;
	while (cur != NULL) {
	    if (cur == pmark->pp_el) {
		match = true;
		break;
	    }

	    cur = cur->next;
	}
	assert (match);
      /* (DEBUG) */

	cur_pp = pmark->pp_el;
    } else {
	assert (pmark->stream_pmark);

	cur_pmark = pmark->stream_pmark;
    }
}

FilePosition
Phase3ItemStream::getFpos (PpItemStream::PositionMarker *_pmark)
{
    assert (_pmark);

    PositionMarker *pmark = static_cast <PositionMarker*> (_pmark);
    if (pmark->pp_el != NULL)
	return pmark->pp_el->data->fpos;

    return pp_stream->getFpos (pmark->stream_pmark);
}

FilePosition
Phase3ItemStream::getFpos ()
{
    if (cur_pp != NULL)
	return cur_pp->data->fpos;

    return pp_stream->getFpos (cur_pmark);
}

void
Phase3ItemStream::prependItems (List< StRef<PpItem> > *items)
{
    if (cur_pp == NULL &&
	pp_items.first != NULL)
    {
        unreachable ();
    }

    if (items == NULL)
	return;

    List< StRef<PpItem> >::Element *cur_item = items->last;
    while (cur_item != NULL) {
	pp_items.prepend (cur_item->data, pp_items.first);
	cur_item = cur_item->previous;
    }

    cur_pp = pp_items.first;
}

void
Phase3ItemStream::trim ()
{
    if (cur_pp == NULL) {
	pp_items.clear ();
	return;
    }

  // Removing all elements except the current one.

    List< StRef<PpItem> >::Element *cur = cur_pp->previous;
    while (cur != NULL) {
	List< StRef<PpItem> >::Element *prv = cur->previous;
	pp_items.remove (cur);
	cur = prv;
    }
}

bool
Phase3ItemStream::hasPrependedItems ()
{
    if (cur_pp == NULL)
	return false;

    return true;
}

Phase3ItemStream::Phase3ItemStream (PpItemStream *pp_stream)
    throw (InternalException)
{
    assert (pp_stream);

    this->pp_stream = pp_stream;

    cur_pp = NULL;
    cur_pmark = pp_stream->getPosition ();
}

}

