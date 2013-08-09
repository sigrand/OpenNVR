/*  Moment-Gst - GStreamer support module for Moment Video Server
    Copyright (C) 2011-2013 Dmitry Shatrov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#ifndef MOMENT__GST_STREAM__H__
#define MOMENT__GST_STREAM__H__


#include <libmary/types.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <moment/libmoment.h>


namespace MomentGst {

using namespace M;
using namespace Moment;

class GstStream : public MediaSource
{
private:
    StateMutex mutex;

    typedef gboolean (*MediaDataCallback) (GstPad    *pad,
                                           GstBuffer *buffer,
                                           gpointer   _self);

    class WorkqueueItem : public Referenced
    {
    public:
        enum ItemType {
            ItemType_CreatePipeline,
            ItemType_ReleasePipeline
        };

        mt_const ItemType item_type;
    };

    mt_const Ref<ChannelOptions> channel_opts;
    mt_const Ref<PlaybackItem>   playback_item;

    mt_const DataDepRef<Timers> timers;
    mt_const DataDepRef<PagePool> page_pool;

    mt_const Ref<VideoStream> video_stream;
    mt_const Ref<VideoStream> mix_video_stream;

    mt_const GstCaps *mix_audio_caps;
    mt_const GstCaps *mix_video_caps;

    mt_const Ref<Thread> workqueue_thread;

    DeferredProcessor::Task deferred_task;
    DeferredProcessor::Registration deferred_reg;
    static bool deferredTask (void *_self);
    void reportStatusEvents ();
    void doReportStatusEvents ();

    mt_mutex (mutex)
    mt_begin

      List< Ref<WorkqueueItem> > workqueue_list;
      Cond workqueue_cond;

      Timers::TimerKey no_video_timer;

      GstElement *playbin;
      gulong audio_probe_id;
      gulong video_probe_id;

      bool got_audio_pad;
      bool got_video_pad;

      GstAppSrc *mix_audio_src;
      GstAppSrc *mix_video_src;

      Time initial_seek;
      bool initial_seek_pending;
      bool initial_seek_complete;
      bool initial_play_pending;

      RtmpServer::MetaData metadata;
      Cond metadata_reported_cond;
      bool metadata_reported;

      Time last_frame_time;

      VideoStream::AudioCodecId audio_codec_id;
      unsigned audio_rate;
      unsigned audio_channels;

      VideoStream::VideoCodecId video_codec_id;

      bool got_in_stats;
      bool got_video;
      bool got_audio;

      bool first_audio_frame;
      bool first_video_frame;

      Count audio_skip_counter;
      Count video_skip_counter;

      bool is_adts_aac_stream;
      bool got_adts_aac_codec_data;
      Byte adts_aac_codec_data [2];

      bool is_h264_stream;
      GstBuffer *avc_codec_data_buffer;

      Uint64 prv_audio_timestamp;

      // This flag helps to prevent concurrent pipeline state transition
      // requests (to NULL and to PLAYING states).
      // TODO FIXME chaning_state_to_playing is not used in all cases where it should be.
      bool changing_state_to_playing;

      bool reporting_status_events;

      // If 'true', then a seek to 'initial_seek' position should be initiated
      // in reportStatusEvents().
      bool seek_pending;
      // If 'true', then the pipeline should be set to PLAYING state
      // in reportStatusEvents().
      bool play_pending;

      // No "no_video" notifications should be made after error or eos
      // notification for the same GstStream instance.
      bool no_video_pending;
      bool got_video_pending;
      bool error_pending;
      bool eos_pending;
      // If 'true', then no further notifications for this GstStream
      // instance should be made.
      // 'close_notified' is set to true after error or eos notification.
      bool close_notified;

      // If 'true', then the stream associated with this GstStream
      // instance has been closed, which means that all associated gstreamer
      // objects should be released.
      bool stream_closed;

      // Bytes received (in_stats_el's sink pad).
      Uint64 rx_bytes;
      // Bytes generated (audio fakesink's sink pad).
      Uint64 rx_audio_bytes;
      // Bytes generated (video fakesink's sink pad).
      Uint64 rx_video_bytes;

      LibMary_ThreadLocal *tlocal;

    mt_end

    mt_const Cb<MediaSource::Frontend> frontend;

    static void workqueueThreadFunc (void *_self);

  // Pipeline manipulation

    void createPipelineForChainSpec ();
    void createPipelineForUri ();
    void createSmartPipelineForUri ();

    void doCreatePipeline ();
    void doReleasePipeline ();

    mt_unlocks (mutex) Result setPipelinePlaying ();

    mt_unlocks (mutex) void pipelineCreationFailed ();

  // Audio/video data handling

    mt_mutex (mutex) void reportMetaData ();

    static gboolean inStatsDataCb (GstPad    *pad,
				   GstBuffer *buffer,
				   gpointer   _self);

  // Audio data handling

    mt_mutex (mutex) void doAudioData (GstBuffer *buffer);

    static gboolean audioDataCb (GstPad    *pad,
				 GstBuffer *buffer,
				 gpointer   _self);

    static void handoffAudioDataCb (GstElement *element,
				    GstBuffer  *buffer,
				    GstPad     *pad,
				    gpointer    _self);

  // Video data handling

    mt_mutex (mutex) void doVideoData (GstBuffer *buffer);

    static gboolean videoDataCb (GstPad    *pad,
				 GstBuffer *buffer,
				 gpointer   _self);

    static void handoffVideoDataCb (GstElement *element,
				    GstBuffer  *buffer,
				    GstPad     *pad,
				    gpointer    _self);

  // State management

    static GstBusSyncReply busSyncHandler (GstBus     *bus,
					   GstMessage *msg,
					   gpointer    _self);

    static gboolean decodebinAutoplugContinue (GstElement *bin,
                                               GstPad     *pad,
                                               GstCaps    *caps,
                                               gpointer    _self);

    static void decodebinPadAdded (GstElement *element,
                                   GstPad     *new_pad,
                                   gpointer    _self);

    mt_mutex (mutex) void doSetPad (GstPad            *pad,
                                    ConstMemory        sink_el_name,
                                    MediaDataCallback  media_data_cb,
                                    ConstMemory        chain);

    void doSetAudioPad (GstPad      *pad,
                        ConstMemory  chain);

    void doSetVideoPad (GstPad      *pad,
                        ConstMemory  chain);

    void setRawAudioPad (GstPad *pad);
    void setRawVideoPad (GstPad *pad);
    void setAudioPad (GstPad *pad);
    void setVideoPad (GstPad *pad);

    static void noVideoTimerTick (void *_self);

  mt_iface (VideoStream::EventHandler)

    static VideoStream::EventHandler mix_stream_handler;

    static void mixStreamAudioMessage (VideoStream::AudioMessage * mt_nonnull audio_msg,
				       void *_self);

    static void mixStreamVideoMessage (VideoStream::VideoMessage * mt_nonnull video_msg,
				       void *_self);

  mt_iface_end

public:
  mt_iface (MediaSource)
    void createPipeline ();
    void releasePipeline ();

    void getTrafficStats (TrafficStats * mt_nonnull ret_traffic_stats);
    void resetTrafficStats ();
  mt_iface_end

    mt_const void init (CbDesc<MediaSource::Frontend> const &frontend,
                        Timers            *timers,
                        DeferredProcessor *deferred_processor,
			PagePool          *page_pool,
			VideoStream       *video_stream,
			VideoStream       *mix_video_stream,
			Time               initial_seek,
                        ChannelOptions    *channel_opts,
                        PlaybackItem      *playback_item);

     GstStream ();
    ~GstStream ();
};

}


#endif /* MOMENT__GST_STREAM__H__ */

