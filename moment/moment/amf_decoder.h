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


#ifndef LIBMOMENT__AMF_DECODER__H__
#define LIBMOMENT__AMF_DECODER__H__


#include <libmary/libmary.h>

#include <moment/amf_encoder.h>


namespace Moment {

using namespace M;

mt_unsafe class AmfDecoder
{
private:
    AmfEncoding encoding;
    Array *array;
    Size msg_len;

    Size cur_offset;

    List< StRef<String> > string_table;

    StRef<String> getString (Uint32 index);

    Result decodeU29 (Uint32 *ret_number);

    Result decodeDouble (double *ret_number);

    Result doDecodeStringData (Memory  mem,
                               Size   *ret_len,
                               Size   *ret_full_len,
                               bool    is_long_string);

    Result doDecodeStringData_AMF3 (Memory  mem,
                                    Size   *ret_len,
                                    Size   *ret_full_len);

    Result skipValue_AMF3 ();

    Result doSkipValue (bool  dump,
                        bool *ret_object_end);

    Result skipObject_AMF3 ();

public:
    Result decodeNumber (double *ret_number);

    Result decodeBoolean (bool *ret_boolean);

    Result decodeString (Memory  mem,
			 Size   *ret_len,
			 Size   *ret_full_len);

    Result decodeFieldName (Memory  mem,
			    Size   *ret_len,
			    Size   *ret_full_len);

    Result beginObject ();

    Result skipValue () { return doSkipValue (false /* dump */, NULL /* ret_object_end */); }
    Result dumpValue () { return doSkipValue (true  /* dump */, NULL /* ret_object_end */); }

    Result skipObject (bool dump = false);

    Result skipObjectProperty (bool  dump = false,
                               bool *ret_object_end = NULL);

    Result skipObjectTail (bool dump = false);

    bool isObjectEnd ();

    Size getCurOffset () { return cur_offset; }

    void dump ();

    void reset (AmfEncoding   const encoding,
		Array       * const array,
		Size          const msg_len)
    {
	this->encoding = encoding;
	this->array = array;
	this->msg_len = msg_len;

	cur_offset = 0;
    }

    AmfDecoder (AmfEncoding   const encoding,
		Array       * const array,
		Size          const msg_len)
	: encoding (encoding),
	  array (array),
	  msg_len (msg_len),
	  cur_offset (0)
    {}
};

}


#endif /* LIBMOMENT__AMF_DECODER__H__ */

