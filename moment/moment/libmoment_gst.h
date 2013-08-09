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


#ifndef __MOMENT__LIBMOMENT_GST__H__
#define __MOMENT__LIBMOMENT_GST__H__


#include <libmary/types.h>
#include <gst/gst.h>

#include <libmary/libmary.h>


namespace Moment {

using namespace M;

void dumpGstBufferFlags (GstBuffer *buffer);

void libMomentGstInit (ConstMemory gst_debug_str = ConstMemory());

}


#endif /* __MOMENT__LIBMOMENT_GST__H__ */

