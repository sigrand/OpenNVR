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


#ifndef MOMENT__FETCH_PROTOCOL__H__
#define MOMENT__FETCH_PROTOCOL__H__


#include <libmary/libmary.h>
#include <moment/video_stream.h>


namespace Moment {

using namespace M;

class FetchConnection : public virtual Object
{
public:
    struct Frontend
    {
        void (*disconnected) (bool             * mt_nonnull ret_reconnect,
                              // Time that should pass since the beginning of the previous
                              // connection attempt.
                              Time             * mt_nonnull ret_reconnect_after_millisec,
                              Ref<VideoStream> * mt_nonnull ret_new_stream,
                              void             *cb_data);
    };
};

class FetchProtocol : public virtual Object
{
public:
    virtual Ref<FetchConnection> connect (VideoStream *video_stream,
                                          ConstMemory  uri,
                                          CbDesc<FetchConnection::Frontend> const &frontend) = 0;
};

}


#endif /* MOMENT__FETCH_PROTOCOL__H__ */

