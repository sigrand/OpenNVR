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


#include <libmary/libmary.h>

#include <pargen/grammar.h>


using namespace M;

namespace Pargen {

VSlab<CompoundGrammarEntry::Acceptor> CompoundGrammarEntry::acceptor_slab;

SwitchGrammarEntry::~SwitchGrammarEntry ()
{
    TranzitionEntryHash::iter iter (tranzition_entries);
    while (!tranzition_entries.iter_done (iter)) {
	TranzitionEntry * const tranzition_entry = tranzition_entries.iter_next (iter);
	delete tranzition_entry;
    }
}

StRef<String>
Grammar_Immediate_SingleToken::toString ()
{
    return st_makeString ("[", token, "]");
}

StRef<String>
Grammar_Compound::toString ()
{
    return name;
}

StRef<String>
Grammar_Switch::toString ()
{
    return name;
}

}

