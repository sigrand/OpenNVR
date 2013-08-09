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


#include <moment/amf_decoder.h>


using namespace M;

namespace Moment {

StRef<String>
AmfDecoder::getString (Uint32 const index)
{
    if (index >= string_table.getNumElements())
        return NULL;

    Uint32 i = 0;
    List< StRef<String> >::iterator iter (string_table);
    while (!iter.done()) {
        StRef<String> const &str = iter.next ()->data;
        if (i == index)
            return str;

        ++i;
    }

    unreachable ();
    return NULL;
}

Result
AmfDecoder::decodeU29 (Uint32 * const ret_number)
{
    Uint32 number = 0;
    for (int i = 0; i < 4; ++i) {
        if (msg_len - cur_offset < 1) {
            logD_ (_func, "no U29");
            return Result::Failure;
        }

        Byte byte;
        array->get (cur_offset, Memory::forObject (byte));
        cur_offset += 1;

        number <<= 7;

        if (i < 3) {
            number |= ((Uint32) (byte & 0x7f));
            if (! (byte & 0x80))
                break;
        } else {
            number |= (Uint32) byte;
        }
    }

    if (*ret_number)
        *ret_number = number;

    return Result::Success;
}

Result
AmfDecoder::decodeDouble (double * const ret_number)
{
    if (msg_len - cur_offset < 8) {
        logD_ (_func, "no double");
        return Result::Failure;
    }

    Byte number_buf [8];
    array->get (cur_offset, Memory::forObject (number_buf));
    cur_offset += 8;

    double number;
    ((Byte*) &number) [0] = number_buf [7];
    ((Byte*) &number) [1] = number_buf [6];
    ((Byte*) &number) [2] = number_buf [5];
    ((Byte*) &number) [3] = number_buf [4];
    ((Byte*) &number) [4] = number_buf [3];
    ((Byte*) &number) [5] = number_buf [2];
    ((Byte*) &number) [6] = number_buf [1];
    ((Byte*) &number) [7] = number_buf [0];

    if (ret_number)
        *ret_number = number;

    return Result::Success;
}

Result
AmfDecoder::decodeNumber (double * const ret_number)
{
    if (encoding == AmfEncoding::AMF3) {
        if (msg_len - cur_offset < 1) {
            logD_ (_func, "no number marker");
            return Result::Failure;
        }

        Byte marker;
        array->get (cur_offset, Memory::forObject (marker));
        cur_offset += 1;

        if (marker == Amf3Marker::Integer) {
            Uint32 number;
            if (!decodeU29 (&number))
                return Result::Failure;

            if (ret_number)
                *ret_number = (double) number;
        } else
        if (marker == Amf3Marker::Double) {
            double number;
            if (!decodeDouble (&number))
                return Result::Failure;

            if (ret_number)
                *ret_number = (double) number;
        }
    } else {
        if (msg_len - cur_offset < 9) {
            logD_ (_func, "no number");
            return Result::Failure;
        }

        Byte data [9];
        array->get (cur_offset, Memory::forObject (data));

        if (data [0] != AmfMarker::Number) {
            logD_ (_func, "not a number");
            return Result::Failure;
        }

        if (ret_number) {
            // Aliasing rules are not broken here,
            // since 'Byte' is 'unsigned char'.
            ((Byte*) ret_number) [0] = data [8];
            ((Byte*) ret_number) [1] = data [7];
            ((Byte*) ret_number) [2] = data [6];
            ((Byte*) ret_number) [3] = data [5];
            ((Byte*) ret_number) [4] = data [4];
            ((Byte*) ret_number) [5] = data [3];
            ((Byte*) ret_number) [6] = data [2];
            ((Byte*) ret_number) [7] = data [1];
        }

        cur_offset += 9;
    }

    return Result::Success;
}

Result
AmfDecoder::decodeBoolean (bool * const ret_boolean)
{
    if (encoding == AmfEncoding::AMF3) {
        if (msg_len - cur_offset < 1) {
            logD_ (_func, "no boolean");
            return Result::Failure;
        }

        Byte marker;
        array->get (cur_offset, Memory::forObject (marker));
        cur_offset += 1;

        bool value = false;
        if (marker == Amf3Marker::True) {
            value = true;
        } else
        if (marker == Amf3Marker::False) {
            value = false;
        } else {
            logD_ (_func, "not a boolean");
            return Result::Failure;
        }

        if (ret_boolean)
            *ret_boolean = value;
    } else {
        if (msg_len - cur_offset < 2) {
            logD_ (_func, "no boolean");
            return Result::Failure;
        }

        Byte data [2];
        array->get (cur_offset, Memory::forObject (data));

        if (data [0] != AmfMarker::Boolean) {
            logD_ (_func, "not a boolean");
            return Result::Failure;
        }

        if (ret_boolean)
            *ret_boolean = data [1];

        cur_offset += 2;
    }

    return Result::Success;
}

Result
AmfDecoder::doDecodeStringData (Memory   const mem,
                                Size   * const ret_len,
                                Size   * const ret_full_len,
                                bool     const is_long_string)
{
    Uint32 string_len;
    if (is_long_string) {
        Byte str_len_buf [4];

        if (msg_len - cur_offset < 4) {
            logD_ (_func, "no long string length");
            return Result::Failure;
        }

        array->get (cur_offset, Memory::forObject (str_len_buf));
        cur_offset += 4;

        string_len = ((Uint32) str_len_buf [0] << 24) |
                     ((Uint32) str_len_buf [1] << 16) |
                     ((Uint32) str_len_buf [2] <<  8) |
                     ((Uint32) str_len_buf [3] <<  0);
    } else {
        Byte str_len_buf [2];

        if (msg_len - cur_offset < 2) {
            logD_ (_func, "no string length");
            return Result::Failure;
        }

        array->get (cur_offset, Memory::forObject (str_len_buf));
        cur_offset += 2;

        string_len = ((Uint32) str_len_buf [0] << 8) |
                     ((Uint32) str_len_buf [1] << 0);
    }

    if (msg_len - cur_offset < string_len) {
	logE_ (_func, "message is too short");
	return Result::Failure;
    }

    Size const tocopy = (mem.len() > string_len ? string_len : mem.len());
    array->get (cur_offset, mem.region (0, tocopy));
    cur_offset += string_len;

    if (ret_len)
	*ret_len = tocopy;

    if (ret_full_len)
	*ret_full_len = string_len;

    return Result::Success;
}

Result
AmfDecoder::doDecodeStringData_AMF3 (Memory   const mem,
                                     Size   * const ret_len,
                                     Size   * const ret_full_len)
{
    Uint32 length;
    if (!decodeU29 (&length))
        return Result::Failure;

    bool const is_ref = !(length & 1);
    length >>= 1;

    Size tocopy;
    Size string_len;
    if (is_ref) {
        StRef<String> const str = getString (length /* index */);
        if (!str) {
            logD_ (_func, "unresolved string reference");
            return Result::Failure;
        }

        string_len = str->len();
        tocopy = (mem.len() > string_len ? string_len : mem.len());
        memcpy (mem.mem(), str->mem().mem(), tocopy);
    } else {
        string_len = length;
        if (msg_len - cur_offset < string_len) {
            logE_ (_func, "message is too short");
            return Result::Failure;
        }

        StRef<String> const str = st_grab (new String (string_len));
        array->get (cur_offset, str->mem());
        string_table.append (str);

        tocopy = (mem.len() > string_len ? string_len : mem.len());
        array->get (cur_offset, mem.region (0, tocopy));
        cur_offset += string_len;
    }

    if (ret_len)
        *ret_len = tocopy;

    if (ret_full_len)
        *ret_full_len = string_len;

    return Result::Success;
}

Result
AmfDecoder::decodeString (Memory   const mem,
	 		  Size   * const ret_len,
	 		  Size   * const ret_full_len)
{
    if (msg_len - cur_offset < 1) {
	logD_ (_func, "no string marker");
	return Result::Failure;
    }

    Byte marker;
    array->get (cur_offset, Memory::forObject (marker));
    cur_offset += 1;

    if (encoding == AmfEncoding::AMF3)
        return doDecodeStringData_AMF3 (mem, ret_len, ret_full_len);

    if (marker != AmfMarker::String     &&
        marker != AmfMarker::LongString &&
        marker != AmfMarker::XmlDocument)
    {
        logD_ (_func, "not a string");
        return Result::Failure;
    }
    bool const is_long_string = (marker != AmfMarker::String);

    return doDecodeStringData (mem, ret_len, ret_full_len, is_long_string);
}

Result
AmfDecoder::decodeFieldName (Memory   const mem,
			     Size   * const ret_len,
			     Size   * const ret_full_len)
{
    if (encoding == AmfEncoding::AMF3) {
        logE_ (_func, "Parsing of AMF3 objects is not implemented");
        return Result::Failure;
    }

    if (msg_len - cur_offset < 2) {
	logD_ (_func, "no field name");
	return Result::Failure;
    }

    Byte header_data [2];
    array->get (cur_offset, Memory::forObject (header_data));

    cur_offset += 2;

    Uint32 const string_len = ((Uint32) header_data [0] << 8) |
			      ((Uint32) header_data [1] << 0);

    if (msg_len - cur_offset < string_len) {
	logE_ (_func, "message is too short");
	return Result::Failure;
    }

    Size const tocopy = (mem.len() > string_len ? string_len : mem.len());
    array->get (cur_offset, mem.region (0, tocopy));

    cur_offset += string_len;

    if (ret_len)
	*ret_len = tocopy;

    if (ret_full_len)
	*ret_full_len = string_len;

    return Result::Success;
}

Result
AmfDecoder::beginObject ()
{
    if (encoding == AmfEncoding::AMF3) {
        logE_ (_func, "Parsing of AMF3 objects is not implemented");
        return Result::Failure;
    }

    if (msg_len - cur_offset < 1) {
	logD_ (_func, "no object marker");
	return Result::Failure;
    }

    Byte obj_marker;
    array->get (cur_offset, Memory::forObject (obj_marker));
    cur_offset += 1;

    if (obj_marker != AmfMarker::Object) {
	logE_ (_func, "not an object");
	return Result::Failure;
    }

    return Result::Success;
}

Result
AmfDecoder::skipValue_AMF3 ()
{
    LogLevel const loglevel = /* dump ? LogLevel::None : */ LogLevel::Debug;

    if (msg_len - cur_offset < 1) {
        log_ (loglevel, _func, "no value marker");
        return Result::Failure;
    }

    Byte value_marker;
    array->get (cur_offset, Memory::forObject (value_marker));
    cur_offset += 1;

    switch (value_marker) {
        case Amf3Marker::Undefined: {
          // No-op
        } break;
        case Amf3Marker::Null: {
          // No-op
        } break;
        case Amf3Marker::False: {
          // No-op
        } break;
        case Amf3Marker::True: {
          // No-op
        } break;
        case Amf3Marker::Integer: {
            --cur_offset;
            if (!decodeNumber (NULL))
                return Result::Failure;
        } break;
        case Amf3Marker::Double: {
            --cur_offset;
            if (!decodeNumber (NULL))
                return Result::Failure;
        } break;
        case Amf3Marker::String: {
            --cur_offset;
            if (!decodeString (Memory(), NULL, NULL))
                return Result::Failure;
        } break;
        case Amf3Marker::Xml:
        case Amf3Marker::XmlDoc:
        case Amf3Marker::ByteArray: {
            if (msg_len - cur_offset < 1) {
                logD_ (_func, "no Xml length");
                return Result::Failure;
            }

            Uint32 header;
            array->get (cur_offset, Memory::forObject (header));
            cur_offset += 1;

            if (header & 0x1) {
                Uint32 const length = header >> 1;
                if (msg_len - cur_offset < length) {
                    logD_ (_func, "Xml is too long");
                    return Result::Failure;
                }

                cur_offset += length;
            }
        } break;
        case Amf3Marker::Date: {
            if (msg_len - cur_offset < 1) {
                logD_ (_func, "no Date header");
                return Result::Failure;
            }

            Uint32 header;
            array->get (cur_offset, Memory::forObject (header));
            cur_offset += 1;

            if (header & 0x1) {
                if (!decodeDouble (NULL)) {
                    logD_ (_func, "no date-time");
                    return Result::Failure;
                }
            }
        } break;
        case Amf3Marker::Array: {
            if (msg_len - cur_offset < 1) {
                logD_ (_func, "no Array header");
                return Result::Failure;
            }

            Uint32 header;
            array->get (cur_offset, Memory::forObject (header));
            cur_offset += 1;

            if (header & 0x1) {
                Uint32 const dense_size = (header >> 1);

                for (;;) {
                    Size str_len;
                    if (!doDecodeStringData_AMF3 (Memory(), NULL, &str_len))
                        return Result::Failure;

                    if (str_len == 0)
                        break;

                    if (!skipValue_AMF3 ())
                        return Result::Failure;
                }

                for (Uint32 i = 0; i < dense_size; ++i) {
                    if (!skipValue_AMF3 ())
                        return Result::Failure;
                }
            }
        } break;
        case Amf3Marker::Object: {
            --cur_offset;
            return skipObject_AMF3 ();
        } break;
        case Amf3Marker::VectorInt: {
            logD_ (_func, "VectorInt: unsupported");
            return Result::Failure;
        } break;
        case Amf3Marker::VectorUint: {
            logD_ (_func, "VectorUint: unsupported");
            return Result::Failure;
        } break;
        case Amf3Marker::VectorDouble: {
            logD_ (_func, "VectorDouble: unsupported");
            return Result::Failure;
        } break;
        case Amf3Marker::VectorObject: {
            logD_ (_func, "VectorObject: unsupported");
            return Result::Failure;
        } break;
        case Amf3Marker::Dictionary: {
            logD_ (_func, "Dictionary: unsupported");
            return Result::Failure;
        } break;
        default: {
            log_ (loglevel, _func, "unknown field value type: ", (unsigned) value_marker);
            return Result::Failure;
        } break;
    }

    return Result::Success;
}

Result
AmfDecoder::doSkipValue (bool   const dump,
                         bool * const ret_object_end)
{
    if (ret_object_end)
        *ret_object_end = false;

    LogLevel const loglevel = dump ? LogLevel::None : LogLevel::Debug;

    if (msg_len - cur_offset < 1) {
        log_ (loglevel, _func, "no value marker");
        return Result::Failure;
    }

    Byte value_marker;
    array->get (cur_offset, Memory::forObject (value_marker));
    cur_offset += 1;

    switch (value_marker) {
        case AmfMarker::Number: {
            double number;
            --cur_offset;
            if (!decodeNumber (dump ? &number : NULL))
                return Result::Failure;

            if (dump)
                log_ (loglevel, _func, "Number: ", number);
        } break;
        case AmfMarker::Boolean: {
            bool boolean;
            --cur_offset;
            if (!decodeBoolean (dump ? &boolean : NULL))
                return Result::Failure;

            if (dump)
                log_ (loglevel, _func, "Boolean: ", boolean);
        } break;
        case AmfMarker::String:
        case AmfMarker::LongString:
        case AmfMarker::XmlDocument: {
            Byte str [128];
            Size len;
            Size full_len;

            --cur_offset;
            if (!decodeString ((dump ? Memory::forObject (str) : Memory()),
                               &len,
                               &full_len))
            {
                return Result::Failure;
            }

            if (dump) {
                log_ (loglevel, _func,
                      (value_marker == AmfMarker::String ? ConstMemory ("String")
                                                         : ConstMemory ("LongString")),
                      " [", full_len, "]: ", ConstMemory (str, len));
            }
        } break;
        case AmfMarker::Object: {
            if (dump)
                log_ (loglevel, _func, "Object");

            --cur_offset;
            if (!skipObject (dump))
                return Result::Failure;
        } break;
        case AmfMarker::TypedObject: {
            if (dump)
                log_ (loglevel, _func, "TypedObject");

            --cur_offset;
            if (!skipObject (dump))
                return Result::Failure;
        } break;
        case AmfMarker::MovieClip: {
            if (dump)
                log_ (loglevel, _func, "MovieClip");
        } break;
        case AmfMarker::Null: {
            if (dump)
                log_ (loglevel, _func, "Null");
        } break;
        case AmfMarker::Undefined: {
            if (dump)
                log_ (loglevel, _func, "Undefined");
        } break;
        case AmfMarker::Reference: {
            if (msg_len - cur_offset < 2) {
                log_ (loglevel, _func, "no reference index");
                return Result::Failure;
            }

            if (dump) {
                Byte index_buf [2];
                array->get (cur_offset, Memory::forObject (index_buf));

                Uint32 const index = ((Uint32) index_buf [0] << 8) |
                                     ((Uint32) index_buf [1] << 0);

                log_ (loglevel, _func, "Reference: ", index);
            }

            cur_offset += 2;
        } break;
        case AmfMarker::EcmaArray: {
            if (msg_len - cur_offset < 4) {
                log_ (loglevel, _func, "no ECMA array property count");
                return Result::Failure;
            }

            Byte count_buf [4];
            array->get (cur_offset, Memory::forObject (count_buf));
            cur_offset += 4;

            Uint32 const count = ((Uint32) count_buf [0] << 24) |
                                 ((Uint32) count_buf [1] << 16) |
                                 ((Uint32) count_buf [2] <<  8) |
                                 ((Uint32) count_buf [3] <<  0);
            if (dump)
                log_ (loglevel, _func, "ECMA Array [", count, "]");

            for (Uint32 i = 0; i < count; ++i) {
                if (!skipObjectProperty (dump, NULL /* ret_object_end */))
                    return Result::Failure;
            }
        } break;
        case AmfMarker::ObjectEnd: {
            if (dump)
                log_ (loglevel, _func, "ObjectEnd");

            if (ret_object_end)
                *ret_object_end = true;
        } break;
        case AmfMarker::StrictArray: {
            if (msg_len - cur_offset < 4) {
                log_ (loglevel, _func, "no strict array value count");
                return Result::Failure;
            }

            Byte count_buf [4];
            array->get (cur_offset, Memory::forObject (count_buf));
            cur_offset += 4;

            Uint32 const count = ((Uint32) count_buf [0] << 24) |
                                 ((Uint32) count_buf [1] << 16) |
                                 ((Uint32) count_buf [2] <<  8) |
                                 ((Uint32) count_buf [3] <<  0);
            if (dump)
                log_ (loglevel, _func, "Strict Array [", count, "]");

            for (Uint32 i = 0; i < count; ++i) {
                if (!doSkipValue (dump, NULL /* ret_object_end */))
                    return Result::Failure;
            }
        } break;
        case AmfMarker::Date: {
            if (msg_len - cur_offset< 8) {
                log_ (loglevel, _func, "no date time");
                return Result::Failure;
            }

            double time;
            {
                Byte time_buf [8];
                array->get (cur_offset, Memory::forObject (time_buf));
                cur_offset += 8;

                ((Byte*) &time) [0] = time_buf [7];
                ((Byte*) &time) [1] = time_buf [6];
                ((Byte*) &time) [2] = time_buf [5];
                ((Byte*) &time) [3] = time_buf [4];
                ((Byte*) &time) [4] = time_buf [3];
                ((Byte*) &time) [5] = time_buf [2];
                ((Byte*) &time) [6] = time_buf [1];
                ((Byte*) &time) [7] = time_buf [0];
            }

            if (msg_len - cur_offset < 2) {
                log_ (loglevel, _func, "no date time-zone");
                return Result::Failure;
            }

            if (dump) {
                Byte tz_buf [2];
                array->get (cur_offset, Memory::forObject (tz_buf));
                Uint32 const tz = ((Uint32) tz_buf [0] << 8) |
                                  ((Uint32) tz_buf [1] << 0);

                log_ (loglevel, _func, "Date: time: ", time, "time-zone: ", tz);
            }

            cur_offset += 2;
        } break;
        case AmfMarker::Unsupported: {
            if (dump)
                log_ (loglevel, _func, "Unsupported");
        } break;
        case AmfMarker::RecordSet: {
            if (dump)
                log_ (loglevel, _func, "MovieClip");
        } break;
        case AmfMarker::AvmPlusObject: {
            if (dump)
                log_ (loglevel, _func, "AvmPlusObject");

            encoding = AmfEncoding::AMF3;
            if (!skipValue_AMF3 ()) {
                return Result::Failure;
            }
            encoding = AmfEncoding::AMF0;

            return Result::Success;
        } break;
        default: {
            log_ (loglevel, _func, "unknown field value type: ", (unsigned) value_marker);
            return Result::Failure;
        } break;
    };

    return Result::Success;
}

Result
AmfDecoder::skipObject_AMF3 ()
{
    if (msg_len - cur_offset < 1) {
	logD_ (_func, "no object marker");
	return Result::Failure;
    }

    Byte obj_marker;
    array->get (cur_offset, Memory::forObject (obj_marker));
    cur_offset += 1;

    if (obj_marker != Amf3Marker::Object) {
	logD_ (_func, "not an object");
	return Result::Failure;
    }

    Uint32 header;
    if (!decodeU29 (&header))
        return Result::Failure;

    if (! (header & 0x1)) {
        // object reference
        return Result::Success;
    }

    if (! (header & 0x2)) {
        // traits reference
        return Result::Success;
    }

    if (header & 0x4) {
        // traits-ext
        cur_offset = msg_len;
        return Result::Success;
    }

    bool const dynamic = (header & 0x8);
    Uint32 const num_sealed = (header >> 4);

    if (!doDecodeStringData_AMF3 (Memory(), NULL, NULL))
        return Result::Failure;

    for (Uint32 i = 0; i < num_sealed; ++i) {
        if (!doDecodeStringData_AMF3 (Memory(), NULL, NULL))
            return Result::Failure;
    }

    for (Uint32 i = 0; i < num_sealed; ++i) {
        if (!skipValue_AMF3 ())
            return Result::Failure;
    }

    if (dynamic) {
        for (;;) {
            Size len;
            if (!doDecodeStringData_AMF3 (Memory(), NULL, &len))
                return Result::Failure;

            if (len == 0)
                break;

            if (!skipValue_AMF3 ())
                return Result::Failure;
        }
    }

    return Result::Success;
}

Result
AmfDecoder::skipObject (bool const dump)
{
    if (encoding == AmfEncoding::AMF3)
        return skipObject_AMF3 ();

    if (msg_len - cur_offset < 1) {
	logD_ (_func, "no object marker");
	return Result::Failure;
    }

    Byte obj_marker;
    array->get (cur_offset, Memory::forObject (obj_marker));
    cur_offset += 1;

    if (obj_marker == AmfMarker::Null)
	return Result::Success;

    if (obj_marker != AmfMarker::Object &&
        obj_marker != AmfMarker::TypedObject)
    {
	logD_ (_func, "not an object");
	return Result::Failure;
    }

    if (obj_marker == AmfMarker::TypedObject) {
        Byte str [128];
        Size len;
        Size full_len;
        if (!doDecodeStringData ((dump ? Memory::forObject (str) : Memory()),
                                 &len,
                                 &full_len,
                                 false /* is_long_string */))
        {
            return Result::Failure;
        }

        if (dump)
            logD_ (_func, "TypedObject: class-name: ", ConstMemory (str, len));
    }

    return skipObjectTail (dump);
}

Result
AmfDecoder::skipObjectProperty (bool   const dump,
                                bool * const ret_object_end)
{
    {
        Byte str [128];
        Size len;
        Size full_len;
        if (!decodeFieldName (dump ? Memory::forObject (str) : Memory(),
                              &len,
                              &full_len))
        {
            logD_ (_func, "no field name");
            return Result::Failure;
        }

        if (dump)
            logD_ (_func, "field name [", full_len, "]: ", ConstMemory (str, len));
    }

    if (!doSkipValue (dump, ret_object_end))
        return Result::Failure;

    return Result::Success;
}

Result
AmfDecoder::skipObjectTail (bool const dump)
{
    for (;;) {
        bool object_end = false;
        if (!skipObjectProperty (dump, &object_end))
            return Result::Failure;

        if (object_end)
            break;
    }

    return Result::Success;
}

bool
AmfDecoder::isObjectEnd ()
{
    if (msg_len - cur_offset < 3) {
        logE_ (_func, "no field name length");
        return false;
    }

    Byte buf [3];
    array->get (cur_offset, Memory::forObject (buf));
    if (buf [0] == 0x00 && buf [1] == 0x00 && buf [2] == 0x09)
        return true;

    return false;
}

void
AmfDecoder::dump ()
{
    AmfEncoding const tmp_encoding = encoding;
    Size tmp_offset = cur_offset;
    while (dumpValue ());
    encoding = tmp_encoding;
    cur_offset = tmp_offset;
}

}

