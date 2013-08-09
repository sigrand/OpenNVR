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


#include <scruffy/list_pp_item_stream.h>


using namespace M;
using namespace Pargen;

namespace Scruffy {
    
PpItemStream::PpItemResult
ListPpItemStream::getNextItem (StRef<PpItem> *pp_item)
    throw (InternalException,
	   ParsingException)
{
    if (cur_pp != NULL) {
	if (pp_item != NULL)
	    *pp_item = cur_pp->data;

	cur_pp = cur_pp->next;
	return PpItemNormal;
    }

    if (pp_item != NULL)
	*pp_item = NULL;

    return PpItemEof;
}

PpItemStream::PpItemResult
ListPpItemStream::getHeaderName (StRef<PpToken_HeaderName> *ret_hn_token)
    throw (ParsingException,
	   InternalException)
{
    // TODO Is this necessary?

    // No-op

    unreachable ();

    return PpItemNormal;
}

StRef<PpItemStream::PositionMarker>
ListPpItemStream::getPosition ()
    throw (InternalException)
{
    StRef<PpItemStream::PositionMarker> ret_pmark = st_grab (static_cast <PpItemStream::PositionMarker*> (new (std::nothrow) PositionMarker));

    PositionMarker *pmark = static_cast <PositionMarker*> (ret_pmark.ptr ());
    pmark->pp_el = cur_pp;

    return ret_pmark;
}

void
ListPpItemStream::setPosition (PpItemStream::PositionMarker *_pmark)
    throw (InternalException)
{
    assert (_pmark);

    PositionMarker *pmark = static_cast <PositionMarker*> (_pmark);
    cur_pp = pmark->pp_el;
}

FilePosition
ListPpItemStream::getFpos (PpItemStream::PositionMarker *_pmark)
{
    assert (_pmark == NULL);

    PositionMarker *pmark = static_cast <PositionMarker*> (_pmark);
    if (pmark->pp_el != NULL)
	return pmark->pp_el->data->fpos;

    return start_fpos;
}

FilePosition
ListPpItemStream::getFpos ()
{
    if (cur_pp != NULL)
	return cur_pp->data->fpos;

    return start_fpos;
}

ListPpItemStream::ListPpItemStream (List< StRef<PpItem> >::Element *cur_pp,
				    const FilePosition &start_fpos)
    : start_fpos (start_fpos)
{
    this->cur_pp = cur_pp;
}

}

