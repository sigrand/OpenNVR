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


#include <moment/push_agent.h>


using namespace M;

namespace Moment {

void
PushAgent::doVideoStreamAdded (VideoStream * const video_stream)
{
    bound_stream->bindToStream (video_stream,
                                video_stream,
                                true /* bind_audio */,
                                true /* bind_video */);
}

MomentServer::VideoStreamHandler PushAgent::moment_stream_handler = {
    videoStreamAdded
};

void
PushAgent::videoStreamAdded (VideoStream * const mt_nonnull video_stream,
                             ConstMemory   const stream_name,
                             void        * const _self)
{
    PushAgent * const self = static_cast <PushAgent*> (_self);

    if (!equal (stream_name, self->stream_name->mem()))
        return;

    self->doVideoStreamAdded (video_stream);
}

mt_const void
PushAgent::init (ConstMemory    const _stream_name,
                 PushProtocol * const mt_nonnull push_protocol,
                 ConstMemory    const uri,
                 ConstMemory    const username,
                 ConstMemory    const password)
{
    MomentServer * const moment = MomentServer::getInstance();
    stream_name = st_grab (new (std::nothrow) String (_stream_name));

    bound_stream = grab (new (std::nothrow) VideoStream);

    {
      // Getting video stream by name, bind the stream, subscribe for video_stream_handler *atomically*.
        moment->lock ();

        moment->addVideoStreamHandler_unlocked (
                CbDesc<MomentServer::VideoStreamHandler> (&moment_stream_handler,
                                                          this /* cb_data */,
                                                          this /* coderef_container */));

        Ref<VideoStream> const video_stream = moment->getVideoStream_unlocked (_stream_name);
        if (video_stream)
            doVideoStreamAdded (video_stream);

        moment->unlock ();
    }

    push_conn = push_protocol->connect (bound_stream, uri, username, password);
}

}

