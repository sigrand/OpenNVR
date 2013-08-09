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


#ifndef MOMENT__MEDIA_SOURCE_PROVIDER__H__
#define MOMENT__MEDIA_SOURCE_PROVIDER__H__


#include <moment/media_source.h>
#include <moment/channel_options.h>


namespace Moment {

using namespace M;

class MediaSourceProvider : public virtual CodeReferenced
{
public:
    virtual Ref<MediaSource> createMediaSource (CbDesc<MediaSource::Frontend> const &frontend,
                                                Timers            *timers,
                                                DeferredProcessor *deferred_processor,
                                                PagePool          *page_pool,
                                                VideoStream       *video_stream,
                                                VideoStream       *mix_video_stream,
                                                Time               initial_seek,
                                                ChannelOptions    *channel_opts,
                                                PlaybackItem      *playback_item) = 0;
};

}


#endif /* MOMENT__MEDIA_SOURCE_PROVIDER__H__ */

