/*  Moment Video Server - High performance media server
    Copyright (C) 2011-2013 Dmitry Shatrov
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


#ifndef LIBMOMENT__AMF_ENCODER__H__
#define LIBMOMENT__AMF_ENCODER__H__


#include <libmary/types.h>
#include <cstdlib>

#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class AmfMarker
{
public:
    enum Value {
	Number        = 0x00,
	Boolean       = 0x01,
	String        = 0x02,
	Object        = 0x03,
	MovieClip     = 0x04, // reserved
	Null          = 0x05,
	Undefined     = 0x06,
	Reference     = 0x07,
	EcmaArray     = 0x08,
	ObjectEnd     = 0x09,
	StrictArray   = 0x0a,
	Date          = 0x0b,
	LongString    = 0x0c,
	Unsupported   = 0x0d,
	RecordSet     = 0x0e, // reserved
	XmlDocument   = 0x0f,
	TypedObject   = 0x10,
        // "Switch to AMF3" marker
	AvmPlusObject = 0x11
    };
    AmfMarker (Value const value) : value (value) {}
    AmfMarker () {}
private:
    Value value;
};

class Amf3Marker
{
public:
    enum Value {
        Undefined    = 0x00,
        Null         = 0x01,
        False        = 0x02,
        True         = 0x03,
        Integer      = 0x04,
        Double       = 0x05,
        String       = 0x06,
        XmlDoc       = 0x07,
        Date         = 0x08,
        Array        = 0x09,
        Object       = 0x0a,
        Xml          = 0x0b,
        ByteArray    = 0x0c,
        VectorInt    = 0x0d,
        VectorUint   = 0x0e,
        VectorDouble = 0x0f,
        VectorObject = 0x10,
        Dictionary   = 0x11
    };
    Amf3Marker (Value const value) : value (value) {}
    Amf3Marker () {}
private:
    Value value;
};

class AmfEncoding
{
public:
    enum Value {
	AMF0,
	AMF3,
	Unknown
    };
    operator Value () const { return value; }
    AmfEncoding (Value const value) : value (value) {}
    AmfEncoding () {}
    Size toString_ (Memory const &mem, Format const & /* fmt */) const
    {
        switch (value) {
            case AMF0:    return toString (mem, "AmfEncoding::AMF0");
            case AMF3:    return toString (mem, "AmfEncoding::AMF3");
            case Unknown: return toString (mem, "AmfEncoding::Unknown");
        }
        unreachable ();
        return 0;
    }
private:
    Value value;
};

class AmfEncoder;

class AmfAtom
{
    friend class AmfEncoder;

public:
    enum Type {
	Number,
	Boolean,
	String,
	NullObject,
	BeginObject,
	EndObject,
	FieldName,
	EcmaArray,
	Null
    };

private:
    Type type;

    union {
	double number;
	bool boolean;
	Uint32 integer;

	struct {
	    Byte const *data;
	    Size len;
	} string;
    };

public:
    void setEcmaArraySize (Uint32 const num_entries)
    {
	integer = num_entries;
    }

    AmfAtom (double const number)
	: type (Number),
	  number (number)
    {
    }

    AmfAtom (bool const boolean)
	: type (Boolean),
	  boolean (boolean)
    {
    }

    AmfAtom (ConstMemory const &mem)
	: type (String)
    {
	string.data = mem.mem();
	string.len = mem.len();
    }

    AmfAtom (Type const type,
	     ConstMemory const &mem)
	: type (type)
    {
	string.data = mem.mem();
	string.len = mem.len();
    }

    AmfAtom (Type const type)
	: type (type)
    {
    }

    AmfAtom ()
    {
    }
};

mt_unsafe class AmfEncoder
{
private:
    AmfAtom *atoms;
    Count num_atoms;

    Count num_encoded;

    bool own_atoms;

    void reserveAtom ()
    {
	if (!own_atoms) {
	    assert (num_encoded < num_atoms);
	} else {
	    assert (num_encoded <= num_atoms);
	    if (num_encoded == num_atoms) {
		atoms = (AmfAtom*) realloc (atoms, sizeof (AmfAtom) * num_atoms * 2);
		assert (atoms);
	    }
	}
    }

public:
    AmfAtom* getLastAtom ()
    {
	return &atoms [num_encoded - 1];
    }

    void addNumber (double number);

    void addBoolean (bool boolean);

    void addString (ConstMemory const &mem);

    void addNullObject ();

    void beginObject ();

    void endObject ();

    void beginEcmaArray (Uint32 const num_entries);

    void endEcmaArray ()
    {
	endObject ();
    }

    void addFieldName (ConstMemory const &field_name);

    void addNull ();

    static Result encode (Memory const  &mem,
			  AmfEncoding    encoding,
			  Size          *ret_len,
			  AmfAtom const *atoms,
			  Count          num_atoms);

    template <Size N>
    static Result encode (Memory         const &mem,
			  AmfEncoding    const  encoding,
			  Size         * const  ret_len,
			  AmfAtom const (&atoms) [N])
    {
	return encode (mem, encoding, ret_len, atoms, sizeof (atoms) / sizeof (AmfAtom));
    }

    Result encode (Memory         const &mem,
		   AmfEncoding    const  encoding,
		   Size         * const  ret_len)
    {
	return encode (mem, encoding, ret_len, atoms, num_encoded);
    }

    void reset ()
    {
	num_encoded = 0;
    }

    AmfEncoder (AmfAtom * const atoms,
		Count const num_atoms)
	: atoms (atoms),
	  num_atoms (num_atoms),
	  num_encoded (0)
    {
	if (atoms) {
	    own_atoms = false;
	} else {
	    this->atoms = (AmfAtom*) malloc (sizeof (AmfAtom) * 64);
	    assert (this->atoms);
	    this->num_atoms = 64;
	    own_atoms = true;
	}
    }

    template <Size N>
    AmfEncoder (AmfAtom (&atoms) [N])
	: atoms (atoms),
	  num_atoms (N),
	  num_encoded (0),
	  own_atoms (false)
    {
    }

    AmfEncoder ()
	: num_encoded (0)
    {
	atoms = (AmfAtom*) malloc (sizeof (AmfAtom) * 64);
	assert (atoms);
	num_atoms = 64;
	own_atoms = true;
    }

    ~AmfEncoder ()
    {
	if (own_atoms)
	    free (atoms);
    }
};

}


#endif /* LIBMOMENT__AMF_ENCODER__H__ */

