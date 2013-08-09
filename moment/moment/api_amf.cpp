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


#include <libmary/libmary.h>
#include <moment/libmoment.h>

#include <moment/api.h>


using namespace M;
using namespace Moment;


extern "C" {


// Duplicated in api.cpp
struct MomentMessage {
    PagePool::PageListHead *page_list;
    Size msg_offset;
    Size msg_len;
    Size prechunk_size;

    PagePool::PageListArray *pl_array;
};


// ________________________________ AMF Decoder ________________________________

class MomentAmfDecoder
{
public:
    AmfDecoder int_decoder;

    MomentAmfDecoder (AmfEncoding   const encoding,
		      Array       * const array,
		      Size          const msg_len)
	: int_decoder (encoding, array, msg_len)
    {
    }
};

MomentAmfDecoder* moment_amf_decoder_new_AMF0 (MomentMessage * const ext_msg)
{
    return new MomentAmfDecoder (AmfEncoding::AMF0, ext_msg->pl_array, ext_msg->msg_len);
}

void moment_amf_decoder_delete (MomentAmfDecoder * const ext_decoder)
{
    delete ext_decoder;
}

void moment_amf_decoder_reset (MomentAmfDecoder * const ext_decoder,
			       MomentMessage    * const ext_msg)
{
    ext_decoder->int_decoder.reset (AmfEncoding::AMF0, ext_msg->pl_array, ext_msg->msg_len);
}

int moment_amf_decode_number (MomentAmfDecoder * const ext_decoder,
			      double           * const ret_number)
{
    return ext_decoder->int_decoder.decodeNumber (ret_number) ? 0 : -1;
}

int moment_amf_decode_boolean (MomentAmfDecoder * const ext_decoder,
			       int              * const ret_boolean)
{
    bool value;
    if (!ext_decoder->int_decoder.decodeBoolean (&value)) {
	if (ret_boolean)
	    *ret_boolean = 0;

	return -1;
    }

    if (ret_boolean)
	*ret_boolean = value ? 1 : 0;

    return 0;
}

int moment_amf_decode_string (MomentAmfDecoder * const ext_decoder,
			      char             * const buf,
			      size_t             const buf_len,
			      size_t           * const ret_len,
			      size_t           * const ret_full_len)
{
    Size len;
    Size full_len;
    if (!ext_decoder->int_decoder.decodeString (Memory (buf, buf_len), &len, &full_len)) {
	if (ret_len)
	    *ret_len = 0;

	if (ret_full_len)
	    *ret_full_len = 0;

	return -1;
    }

    if (ret_len)
	*ret_len = len;

    if (ret_full_len)
	*ret_full_len = full_len;

    return 0;
}

int moment_amf_decode_field_name (MomentAmfDecoder * const ext_decoder,
				  char             * const buf,
				  size_t             const buf_len,
				  size_t           * const ret_len,
				  size_t           * const ret_full_len)
{
    Size len;
    Size full_len;
    if (!ext_decoder->int_decoder.decodeFieldName (Memory (buf, buf_len), &len, &full_len)) {
	if (ret_len)
	    *ret_len = 0;

	if (ret_full_len)
	    *ret_full_len = 0;

	return -1;
    }

    if (ret_len)
	*ret_len = len;

    if (ret_full_len)
	*ret_full_len = full_len;

    return 0;
}

int moment_amf_decoder_begin_object (MomentAmfDecoder * const ext_decoder)
{
    return ext_decoder->int_decoder.beginObject () ? 0 : -1;
}

int moment_amf_decoder_skip_value (MomentAmfDecoder * const ext_decoder)
{
    return ext_decoder->int_decoder.skipValue () ? 0 : -1;
}

int moment_amf_decoder_skip_object (MomentAmfDecoder * const ext_decoder)
{
    return ext_decoder->int_decoder.skipObject () ? 0 : -1;
}


// ________________________________ AMF Encoder ________________________________

struct MomentAmfEncoder {
    AmfEncoder int_encoder;
};

MomentAmfEncoder* moment_amf_encoder_new_AMF0 (void)
{
    MomentAmfEncoder * const encoder = new MomentAmfEncoder;
    assert (encoder);
    return encoder;
}

void moment_amf_encoder_delete (MomentAmfEncoder * const ext_encoder)
{
    delete ext_encoder;
}

void moment_amf_encoder_reset (MomentAmfEncoder * const ext_encoder)
{
    ext_encoder->int_encoder.reset ();
}

void moment_amf_encoder_add_number (MomentAmfEncoder * const ext_encoder,
				    double             const number)
{
    ext_encoder->int_encoder.addNumber (number);
}

void moment_amf_encoder_add_boolean (MomentAmfEncoder * const ext_encoder,
				     int                const boolean)
{
    ext_encoder->int_encoder.addBoolean (boolean);
}

void moment_amf_encoder_add_string (MomentAmfEncoder * const ext_encoder,
				    char const       * const str,
				    size_t             const str_len)
{
    ext_encoder->int_encoder.addString (ConstMemory (str, str_len));
}

void moment_amf_encoder_add_null_object (MomentAmfEncoder * const ext_encoder)
{
    ext_encoder->int_encoder.addNullObject ();
}

void moment_amf_encoder_begin_object (MomentAmfEncoder * const ext_encoder)
{
    ext_encoder->int_encoder.beginObject ();
}

void moment_amf_encoder_end_object (MomentAmfEncoder * const ext_encoder)
{
    ext_encoder->int_encoder.endObject ();
}

void moment_amf_encoder_begin_ecma_array (MomentAmfEncoder * const ext_encoder,
					  unsigned long      const num_entries)
{
    ext_encoder->int_encoder.beginEcmaArray (num_entries);
}

void moment_amf_encoder_end_ecma_array (MomentAmfEncoder * const ext_encoder)
{
    ext_encoder->int_encoder.endEcmaArray ();
}

void moment_amf_encoder_add_field_name (MomentAmfEncoder * const ext_encoder,
					char const       * const name,
					size_t             const name_len)
{
    ext_encoder->int_encoder.addFieldName (ConstMemory (name, name_len));
}

void moment_amf_encoder_add_null (MomentAmfEncoder * const ext_encoder)
{
    ext_encoder->int_encoder.addNull ();
}

int moment_amf_encoder_encode (MomentAmfEncoder * const ext_encoder,
			       unsigned char    * const buf,
			       size_t             const buf_len,
			       size_t           * const ret_len)
{
    Size len = 0;
    if (!ext_encoder->int_encoder.encode (Memory (buf, buf_len), AmfEncoding::AMF0, ret_len ? &len : NULL)) {
	if (ret_len)
	    *ret_len = 0;

	return -1;
    }

    if (ret_len)
	*ret_len = len;

    return 0;
}


} // extern "C"

