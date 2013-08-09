/*  Moment Video Server - High performance media server
    Copyright (C) 2012 Dmitry Shatrov
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


#include <moment/libmoment.h>


using namespace M;

namespace Moment {

void
dumpGstBufferFlags (GstBuffer * const buffer)
{
    Uint32 flags = (Uint32) GST_BUFFER_FLAGS (buffer);
    if (flags != GST_BUFFER_FLAGS (buffer))
	log__ (_func, "flags do not fit into Uint32");

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_READONLY)) {
	log__ (_func, "GST_BUFFER_FLAG_READONLY");
	flags ^= GST_BUFFER_FLAG_READONLY;
    }

#if 0
    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_MEDIA4)) {
	log__ (_func, "GST_BUFFER_FLAG_MEDIA4");
	flags ^= GST_BUFFER_FLAG_MEDIA4;
    }
#endif

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_PREROLL)) {
	log__ (_func, "GST_BUFFER_FLAG_PREROLL");
	flags ^= GST_BUFFER_FLAG_PREROLL;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DISCONT)) {
	log__ (_func, "GST_BUFFER_FLAG_DISCONT");
	flags ^= GST_BUFFER_FLAG_DISCONT;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_IN_CAPS)) {
	log__ (_func, "GST_BUFFER_FLAG_IN_CAPS");
	flags ^= GST_BUFFER_FLAG_IN_CAPS;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_GAP)) {
	log__ (_func, "GST_BUFFER_FLAG_GAP");
	flags ^= GST_BUFFER_FLAG_GAP;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT)) {
	log__ (_func, "GST_BUFFER_FLAG_DELTA_UNIT");
	flags ^= GST_BUFFER_FLAG_DELTA_UNIT;
    }

#if 0
    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_MEDIA1)) {
	log__ (_func, "GST_BUFFER_FLAG_MEDIA1");
	flags ^= GST_BUFFER_FLAG_MEDIA1;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_MEDIA2)) {
	log__ (_func, "GST_BUFFER_FLAG_MEDIA2");
	flags ^= GST_BUFFER_FLAG_MEDIA2;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_MEDIA3)) {
	log__ (_func, "GST_BUFFER_FLAG_MEDIA3");
	flags ^= GST_BUFFER_FLAG_MEDIA3;
    }
#endif

    if (flags)
	log__ (_func, "Extra flags: 0x", fmt_hex, flags);
}

void
libMomentGstInit (ConstMemory const gst_debug_str)
{
// gst_is_initialized() is new API.
//    if (!gst_is_initialized())

    if (gst_debug_str.len() > 0) {
        String const str (gst_debug_str);

        int argc = 2;
        char* argv [] = {
            (char*) "moment",
            (char*) str.cstr(),
            NULL
        };

        char **argv_ = argv;
        gst_init (&argc, &argv_);
    } else {
        gst_init (NULL /* args */, NULL /* argv */);
    }
}

}

