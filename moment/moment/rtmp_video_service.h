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


#ifndef __LIBMOMENT__RTMP_VIDEO_SERVICE__H__
#define __LIBMOMENT__RTMP_VIDEO_SERVICE__H__


#include <libmary/libmary.h>
#include <moment/rtmp_connection.h>


namespace Moment {

using namespace M;

class RtmpVideoService
{
public:
    struct Frontend {
	Result (*clientConnected) (RtmpConnection  * mt_nonnull rtmp_conn,
				   IpAddress const &client_addr,
				   void            *cb_data);
    };

protected:
    mt_const Cb<Frontend> frontend;

public:
    mt_const void setFrontend (CbDesc<Frontend> const &frontend)
    {
	this->frontend = frontend;
    }
};

}


#endif /* __LIBMOMENT__RTMP_VIDEO_SERVICE__H__ */

