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


#ifndef MOMENT__TRANSCODER__H__
#define MOMENT__TRANSCODER__H__


#include <libmary/libmary.h>

#include <moment/video_stream.h>


namespace Moment {

using namespace M;

class Transcoder : public virtual Object
{
public:
    enum TranscodingMode
    {
        TranscodingMode_Off,
        TranscodingMode_On,
        TranscodingMode_Direct
    };

    mt_const virtual void addOutputStream (VideoStream     * mt_nonnull out_stream,
                                           ConstMemory      chain_str,
                                           TranscodingMode  audio_mode,
                                           TranscodingMode  video_mode) = 0;

    // Must be called only once, *after* init() and addOutputSrteam() are called.
    virtual void start (VideoStream * mt_nonnull src_stream) = 0;

    mt_const virtual void init (DeferredProcessor * mt_nonnull deferred_processor,
                                Timers            * mt_nonnull timers,
                                PagePool          * mt_nonnull page_pool,
                                bool               transcode_on_demand,
                                Time               transcode_on_demand_timeout_millisec) = 0;
};

}


#endif /* MOMENT__TRANSCODER__H__ */

