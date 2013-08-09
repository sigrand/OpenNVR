/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov
    e-mail: shatrov@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <moment/amf_encoder.h>


using namespace M;

namespace Moment {

void
AmfEncoder::addNumber (double const number)
{
    reserveAtom ();
    atoms [num_encoded].type = AmfAtom::Number;
    atoms [num_encoded].number = number;
    ++num_encoded;
}

void
AmfEncoder::addBoolean (bool const boolean)
{
    reserveAtom ();
    atoms [num_encoded].type = AmfAtom::Boolean;
    atoms [num_encoded].boolean = boolean;
    ++num_encoded;
}

void
AmfEncoder::addString (ConstMemory const &mem)
{
    reserveAtom ();
    atoms [num_encoded].type = AmfAtom::String;
    atoms [num_encoded].string.data = mem.mem();
    atoms [num_encoded].string.len = mem.len();
    ++num_encoded;
}

void
AmfEncoder::addNullObject ()
{
    reserveAtom ();
    atoms [num_encoded].type = AmfAtom::NullObject;
    ++num_encoded;
}

void
AmfEncoder::beginObject ()
{
    reserveAtom ();
    atoms [num_encoded].type = AmfAtom::BeginObject;
    ++num_encoded;
}

void
AmfEncoder::endObject ()
{
    reserveAtom ();
    atoms [num_encoded].type = AmfAtom::EndObject;
    ++num_encoded;
}

void
AmfEncoder::beginEcmaArray (Uint32 const num_entries)
{
    reserveAtom ();
    atoms [num_encoded].type = AmfAtom::EcmaArray;
    atoms [num_encoded].integer = num_entries;
    ++num_encoded;
}

void
AmfEncoder::addFieldName (ConstMemory const &field_name)
{
    reserveAtom ();
    atoms [num_encoded].type = AmfAtom::FieldName;
    atoms [num_encoded].string.data = field_name.mem();
    atoms [num_encoded].string.len = field_name.len();
    ++num_encoded;
}

void
AmfEncoder::addNull ()
{
    reserveAtom ();
    atoms [num_encoded].type = AmfAtom::Null;
    ++num_encoded;
}

Result
AmfEncoder::encode (Memory          const &mem,
		    AmfEncoding     const  encoding,
		    Size          * const  ret_len,
		    AmfAtom const * const  atoms,
		    Count           const  num_atoms)
{
    assert (encoding == AmfEncoding::AMF0);

    Result result = Result::Success;

    Byte * const buf = mem.mem();
    Size const buf_len = mem.len();

    Size cur_pos = 0;
    for (Count i = 0; i < num_atoms; ++i) {
	AmfAtom const &atom = atoms [i];
	switch (atom.type) {
	    case AmfAtom::Number:
		if (buf_len - cur_pos >= 9) {
		    buf [cur_pos + 0] = AmfMarker::Number;
		    buf [cur_pos + 1] = ((Byte*) &atom.number) [7];
		    buf [cur_pos + 2] = ((Byte*) &atom.number) [6];
		    buf [cur_pos + 3] = ((Byte*) &atom.number) [5];
		    buf [cur_pos + 4] = ((Byte*) &atom.number) [4];
		    buf [cur_pos + 5] = ((Byte*) &atom.number) [3];
		    buf [cur_pos + 6] = ((Byte*) &atom.number) [2];
		    buf [cur_pos + 7] = ((Byte*) &atom.number) [1];
		    buf [cur_pos + 8] = ((Byte*) &atom.number) [0];
		} else {
		    result = Result::Failure;
		}

		cur_pos += 9;
		break;

	    case AmfAtom::Boolean:
		if (buf_len - cur_pos >= 2) {
		    buf [cur_pos + 0] = AmfMarker::Boolean;
		    buf [cur_pos + 1] = atom.boolean ? 1 : 0;
		} else {
		    result = Result::Failure;
		}

		cur_pos += 2;
		break;

	    case AmfAtom::String:
		if (buf_len - cur_pos >= 3 + atom.string.len) {
		    buf [cur_pos + 0] = AmfMarker::String;
		    buf [cur_pos + 1] = (atom.string.len >> 8) & 0xff;
		    buf [cur_pos + 2] = (atom.string.len >> 0) & 0xff;
		    memcpy (buf + cur_pos + 3, atom.string.data, atom.string.len);
		} else {
		    result = Result::Failure;
		}

		cur_pos += 3 + atom.string.len;
		break;

#if 0
// Replaced with AmfAtom::Null

	    case AmfAtom::NullObject:
		if (buf_len - cur_pos >= 4) {
		    buf [cur_pos + 0] = AmfMarker::Object;
		    buf [cur_pos + 1] = 0;
		    buf [cur_pos + 2] = 0;
		    buf [cur_pos + 3] = AmfMarker::ObjectEnd;
		} else {
		    result = Result::Failure;
		}

		cur_pos += 4;
		break;
#endif

	    case AmfAtom::BeginObject:
		if (buf_len - cur_pos >= 1) {
		    buf [cur_pos] = AmfMarker::Object;
		} else {
		    result = Result::Failure;
		}

		cur_pos += 1;
		break;

	    case AmfAtom::EndObject:
		if (buf_len - cur_pos >= 3) {
		    buf [cur_pos + 0] = 0;
		    buf [cur_pos + 1] = 0;
		    buf [cur_pos + 2] = AmfMarker::ObjectEnd;
		} else {
		    result = Result::Failure;
		}

		cur_pos += 3;
		break;

	    case AmfAtom::FieldName:
		if (buf_len - cur_pos >= 2 + atom.string.len) {
		    buf [cur_pos + 0] = (atom.string.len >> 8) & 0xff;
		    buf [cur_pos + 1] = (atom.string.len >> 0) & 0xff;
		    memcpy (buf + cur_pos + 2, atom.string.data, atom.string.len);
		} else {
		    result = Result::Failure;
		}

		cur_pos += 2 + atom.string.len;
		break;

	    case AmfAtom::EcmaArray:
		if (buf_len - cur_pos >= 5) {
		    buf [cur_pos + 0] = AmfMarker::EcmaArray;
		    buf [cur_pos + 1] = (atom.integer >> 24) & 0xff;
		    buf [cur_pos + 2] = (atom.integer >> 16) & 0xff;
		    buf [cur_pos + 3] = (atom.integer >>  8) & 0xff;
		    buf [cur_pos + 4] = (atom.integer >>  0) & 0xff;
		} else {
		    result = Result::Failure;
		}

		cur_pos += 5;
		break;

	    case AmfAtom::NullObject:
	    case AmfAtom::Null:
		if (buf_len - cur_pos >= 1) {
		    buf [cur_pos] = AmfMarker::Null;
		} else {
		    result = Result::Failure;
		}

		cur_pos += 1;
		break;
	}

	if (!result && !ret_len)
	    break;
    }

    if (ret_len)
	*ret_len = cur_pos;

    return result;
}

}

