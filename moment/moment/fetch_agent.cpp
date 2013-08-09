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


#include <moment/fetch_agent.h>


using namespace M;

namespace Moment {

FetchConnection::Frontend const FetchAgent::fetch_conn_frontend = {
    fetchConnDisconnected
};

void
FetchAgent::fetchConnDisconnected (bool             * const mt_nonnull ret_reconnect,
                                   Time             * const mt_nonnull ret_reconnect_after_millisec,
                                   Ref<VideoStream> * const mt_nonnull ret_new_stream,
                                   void             * const _self)
{
    FetchAgent * const self = static_cast <FetchAgent*> (_self);

    *ret_reconnect = true;
    *ret_reconnect_after_millisec = self->reconnect_interval_millisec;

    self->mutex.lock ();
    self->fetch_stream = grab (new (std::nothrow) VideoStream);
    *ret_new_stream = self->fetch_stream;
    self->bound_stream->bindToStream (self->fetch_stream, self->fetch_stream, true, true);
    self->mutex.unlock ();
}

void
FetchAgent::init (MomentServer  * const mt_nonnull moment,
                  FetchProtocol * const mt_nonnull fetch_protocol,
                  ConstMemory     const stream_name,
                  ConstMemory     const uri,
                  Time            const reconnect_interval_millisec)
{
    this->reconnect_interval_millisec = reconnect_interval_millisec;

    bound_stream = grab (new (std::nothrow) VideoStream);
    fetch_stream = grab (new (std::nothrow) VideoStream);
    bound_stream->bindToStream (fetch_stream, fetch_stream, true, true);

    moment->addVideoStream (bound_stream, stream_name);

    mutex.lock ();
    fetch_conn = fetch_protocol->connect (fetch_stream,
                                          uri,
                                          CbDesc<FetchConnection::Frontend> (&fetch_conn_frontend, this, this));
    mutex.unlock ();
}

FetchAgent::FetchAgent ()
    : reconnect_interval_millisec (0)
{}

}

