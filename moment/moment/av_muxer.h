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


#ifndef __MOMENT__AV_MUXER__H__
#define __MOMENT__AV_MUXER__H__


#include <libmary/libmary.h>

#include <moment/video_stream.h>


namespace Moment {

using namespace M;

mt_unsafe class AvMuxer
{
protected:
    Sender *sender;

public:
    virtual mt_throws Result beginMuxing () = 0;
    virtual mt_throws Result muxAudioMessage (VideoStream::AudioMessage * mt_nonnull msg) = 0;
    virtual mt_throws Result muxVideoMessage (VideoStream::VideoMessage * mt_nonnull msg) = 0;
    virtual mt_throws Result endMuxing () = 0;
    virtual void reset () = 0;

    void setSender (Sender * const sender) { this->sender = sender; }

    AvMuxer () : sender (NULL) {}

    virtual ~AvMuxer () {}
};

}


#endif /* __MOMENT__AV_MUXER__H__ */

