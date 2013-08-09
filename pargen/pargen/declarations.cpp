/*  Pargen - Flexible parser generator
    Copyright (C) 2011-2013 Dmitry Shatrov

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


#include <pargen/declarations.h>


using namespace M;

namespace Pargen {

StRef<String>
PhrasePart::toString ()
{
    switch (phrase_part_type) {
	case PhrasePart::t_Phrase: {
	    PhrasePart_Phrase * const phrase_part__phrase =
		    static_cast <PhrasePart_Phrase*> (this);

            return st_makeString ("Phrase ", phrase_part__phrase->phrase_name);
	} break;
	case PhrasePart::t_Token: {
	    PhrasePart_Token * const phrase_part__token =
		    static_cast <PhrasePart_Token*> (this);

	    if (!phrase_part__token->token)
		return st_grab (new String ("Token *"));

            return st_makeString ("Token [", phrase_part__token->token, "]");
	} break;
	case PhrasePart::t_AcceptCb: {
	    PhrasePart_AcceptCb * const phrase_part__accept_cb =
		    static_cast <PhrasePart_AcceptCb*> (this);

            return st_makeString ("AcceptCb ", phrase_part__accept_cb->cb_name);
	} break;
	case PhrasePart::t_UniversalAcceptCb: {
	    PhrasePart_UniversalAcceptCb * const phrase_part__universal_accept_cb =
		    static_cast <PhrasePart_UniversalAcceptCb*> (this);

	    return st_makeString ("UniversalAcceptCb ", phrase_part__universal_accept_cb->cb_name);
	} break;
	case PhrasePart::t_UpwardsAnchor: {
	    PhrasePart_UpwardsAnchor * const phrase_part__upwards_anchor =
		    static_cast <PhrasePart_UpwardsAnchor*> (this);

	    return st_makeString ("UpwardsAnchor ",
                                  phrase_part__upwards_anchor->declaration_name,
                                  (!phrase_part__upwards_anchor->phrase_name ? ":" : ""),
                                  phrase_part__upwards_anchor->phrase_name,
                                  "@",
                                  phrase_part__upwards_anchor->label_name);
	} break;
	case PhrasePart::t_Label: {
	    PhrasePart_Label * const phrase_part__label =
		    static_cast <PhrasePart_Label*> (this);

	    return st_makeString ("Label ", phrase_part__label->label_name);
	} break;
	default:
	  // No-op
	    ;
    }

    return st_grab (new String ("Unknown"));
}

}

