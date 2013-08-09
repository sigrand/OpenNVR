/*  Moment Video Server - High performance media server
    Copyright (C) 2013 Dmitry Shatrov
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


#ifndef MOMENT__UTIL_MOMENT__H__
#define MOMENT__UTIL_MOMENT__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

Result parseMomentRtmpUri (ConstMemory  uri,
                           IpAddress   * mt_nonnull ret_server_addr,
                           ConstMemory * mt_nonnull ret_app_name,
                           ConstMemory * mt_nonnull ret_stream_name,
                           bool        * mt_nonnull ret_momentrtmp_proto);

}


#endif /* MOMENT__UTIL_MOMENT__H__ */

