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


#ifndef __MOMENT__TEST_STREAM_GENERATOR__H__
#define __MOMENT__TEST_STREAM_GENERATOR__H__


#include <libmary/libmary.h>

#include <moment/video_stream.h>


namespace Moment {

using namespace M;

class TestStreamGenerator : public Object
{
private:
    Mutex tick_mutex;

public:
    class Options
    {
    public:
        Uint64 frame_duration;
        Uint64 frame_size;
        Uint64 prechunk_size;
        Uint64 keyframe_interval;
        Uint64 start_timestamp;
        Uint64 burst_width;
        bool use_same_pages;

        Options ();
    };

private:
    mt_const Options opts;

    mt_const DataDepRef<PagePool> page_pool;
    mt_const DataDepRef<Timers>   timers;
    mt_const Ref<VideoStream>     video_stream;

    mt_mutex (tick_mutex) Uint64 keyframe_counter;

    mt_mutex (tick_mutex) PagePool::PageListHead page_list;

    mt_mutex (tick_mutex) bool first_frame;
    mt_mutex (tick_mutex) Time timestamp_offset;

    mt_mutex (tick_mutex) Uint32 page_fill_counter;

    void doFrameTimerTick ();

    static void frameTimerTick (void *_self);

public:
    void init (PagePool    * mt_nonnull page_pool,
               Timers      * mt_nonnull timers,
               VideoStream * mt_nonnull video_stream,
               Options  *opts);

    void start ();

    TestStreamGenerator ();

    ~TestStreamGenerator ();
};

}


#endif /* __MOMENT__TEST_STREAM_GENERATOR__H__ */

