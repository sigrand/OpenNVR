/*  Moment Video Server - High performance media server
    Copyright (C) 2012-2013 Dmitry Shatrov
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


#ifndef MOMENT__FETCH_AGENT__H__
#define MOMENT__FETCH_AGENT__H__


#include <libmary/libmary.h>
#include <moment/fetch_protocol.h>
#include <moment/moment_server.h>


namespace Moment {

using namespace M;

class FetchAgent : public Object
{
private:
    StateMutex mutex;

    mt_const Time reconnect_interval_millisec;

    mt_const Ref<FetchConnection> fetch_conn;

    mt_mutex (mutex) Ref<VideoStream> bound_stream;
    mt_mutex (mutex) Ref<VideoStream> fetch_stream;

  mt_iface (FetchConnection::Frontned)
    static FetchConnection::Frontend const fetch_conn_frontend;

    static void fetchConnDisconnected (bool             * mt_nonnull ret_reconnect,
                                       Time             * mt_nonnull ret_reconnect_after_millisec,
                                       Ref<VideoStream> * mt_nonnull ret_new_stream,
                                       void             *_self);
  mt_iface_end

public:
    void init (MomentServer  * mt_nonnull moment,
               FetchProtocol * mt_nonnull fetch_protocol,
               ConstMemory    stream_name,
               ConstMemory    uri,
               Time           reconnect_interval_millisec);

    FetchAgent ();
};

}


#endif /* MOMENT__FETCH_AGENT__H__ */

