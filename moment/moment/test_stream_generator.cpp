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


#include <moment/test_stream_generator.h>


using namespace M;

namespace Moment {

TestStreamGenerator::Options::Options ()
    : frame_duration    (40),
      frame_size        (2500),
      prechunk_size     (65536),
      keyframe_interval (10),
      start_timestamp   (0),
      burst_width       (1),
      use_same_pages    (true)
{
}

void
TestStreamGenerator::init (PagePool    * const mt_nonnull page_pool,
                           Timers      * const mt_nonnull timers,
                           VideoStream * const mt_nonnull video_stream,
                           Options     * const init_opts)
{
    this->page_pool    = page_pool;
    this->timers       = timers;
    this->video_stream = video_stream;

    if (init_opts)
        opts = *init_opts;

    {
        Byte *frame_buf = NULL;
        if (opts.frame_size > 0) {
            frame_buf = new Byte [opts.frame_size];
            memset (frame_buf, 0, opts.frame_size);
        }

        if (opts.prechunk_size > 0) {
#warning Deal with prechunk offsets here
            RtmpConnection::PrechunkContext prechunk_ctx;
            RtmpConnection::fillPrechunkedPages (&prechunk_ctx,
                                                 ConstMemory (frame_buf, opts.frame_size),
                                                 page_pool,
                                                 &page_list,
                                                 RtmpConnection::DefaultVideoChunkStreamId,
                                                 opts.start_timestamp,
                                                 true /* first_chunk */);
        } else {
            page_pool->getFillPages (&page_list, ConstMemory (frame_buf, opts.frame_size));
        }

        delete frame_buf;
    }
}

void
TestStreamGenerator::start ()
{
    timers->addTimer_microseconds (
            CbDesc<Timers::TimerCallback> (
                    frameTimerTick,
                    this,
                    this),
            (Time) (opts.frame_duration * 1000 * opts.burst_width),
            true /* periodical */);
}

void
TestStreamGenerator::doFrameTimerTick ()
{
    tick_mutex.lock ();

    for (Uint64 i = 0; i < opts.burst_width; ++i) {
	VideoStream::VideoMessage video_msg;

	if (keyframe_counter == 0) {
	    video_msg.frame_type = VideoStream::VideoFrameType::KeyFrame;
	    keyframe_counter = opts.keyframe_interval;
	} else {
	    video_msg.frame_type = VideoStream::VideoFrameType::InterFrame;
	    --keyframe_counter;
	}

	if (first_frame) {
	    timestamp_offset = getTimeMilliseconds();
	    video_msg.timestamp_nanosec = opts.start_timestamp * 1000000;
	    first_frame = false;
	} else {
	    Time timestamp = getTimeMilliseconds();
	    if (timestamp >= timestamp_offset)
		timestamp -= timestamp_offset;
	    else
		timestamp = 0;

	    timestamp += opts.start_timestamp;

	    video_msg.timestamp_nanosec = timestamp * 1000000;
	}

	video_msg.prechunk_size = opts.prechunk_size;
	video_msg.codec_id = VideoStream::VideoCodecId::/* Unknown */ SorensonH263;

	PagePool::PageListHead *page_list_ptr = &page_list;
	PagePool::PageListHead tmp_page_list;
	if (!opts.use_same_pages) {
	    page_pool->getPages (&tmp_page_list, opts.frame_size);

	    {
		PagePool::Page *page = tmp_page_list.first;
		while (page) {
		    memset (page->getData(), (int) page_fill_counter, page->data_len);
		    page = page->getNextMsgPage();
		}
	    }

	    if (page_fill_counter < 255)
		++page_fill_counter;
	    else
		page_fill_counter = 0;

	    page_list_ptr = &tmp_page_list;
	}

	video_msg.page_pool  = page_pool;
	video_msg.page_list  = *page_list_ptr;
	video_msg.msg_len    = opts.frame_size;
	video_msg.msg_offset = 0;

	video_stream->fireVideoMessage (&video_msg);

	if (!opts.use_same_pages)
	    page_pool->msgUnref (tmp_page_list.first);
    }

    tick_mutex.unlock ();
}

void
TestStreamGenerator::frameTimerTick (void * const _self)
{
    TestStreamGenerator * const self = static_cast <TestStreamGenerator*> (_self);
    self->doFrameTimerTick ();
}

TestStreamGenerator::TestStreamGenerator ()
    : page_pool (this /* coderef_container */),
      timers    (this /* coderef_container */),
      keyframe_counter  (0),
      first_frame       (true),
      timestamp_offset  (0),
      page_fill_counter (0)
{
}

TestStreamGenerator::~TestStreamGenerator ()
{
    logD_ (_this_func);
}

} // namespace Moment

