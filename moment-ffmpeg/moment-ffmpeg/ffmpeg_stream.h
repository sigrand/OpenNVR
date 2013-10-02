/*  Moment-FFmpeg - FFmpeg support module for Moment Video Server
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


#ifndef MOMENT__FFMPEG_STREAM__H__
#define MOMENT__FFMPEG_STREAM__H__


#include <libmary/types.h>
//#include <libmary/util_time.h>

#include <moment/libmoment.h>
#include <moment-ffmpeg/nvr_cleaner.h>

extern "C" {
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
//#include <libavformat/avc.h>
#include <libswscale/swscale.h>
}


namespace MomentFFmpeg {

using namespace M;
using namespace Moment;

class FFmpegStream : public MediaSource
{
private:
    StateMutex mutex;

    class WorkqueueItem : public Referenced
    {
    public:
        enum ItemType {
            ItemType_CreatePipeline,
            ItemType_ReleasePipeline
        };

        mt_const ItemType item_type;
    };

    class MemoryEx : public Memory
    {

    private:

        Size cur_size;

    public:

        Size size() const { return cur_size; }
        void setSize(Size newPos) { cur_size = newPos; }

        MemoryEx ()
            : Memory()
            , cur_size(0)
        {
        }

        MemoryEx (Byte * const mem, Size const len)
            : Memory(mem, len)
            , cur_size(0)
        {
        }
    };

    typedef String MemorySafe;

    class ffmpegStreamData
    {

    public:

        ffmpegStreamData(void);
        ~ffmpegStreamData();

        Result Init(const char * uri, const char * channel_name, const Ref<MConfig::Config> & config, Timers * timers);
        void Deinit();

        // if PushVideoPacket returns 'false' consequently is the end of stream (EOS).
        bool PushVideoPacket(FFmpegStream * pParent);

        static void CloseCodecs(AVFormatContext * pAVFrmtCntxt);

    private /*functions*/:

    private /*variables*/:

        AVFormatContext *   format_ctx;
        int                 video_stream_idx;
        Ref<NvrCleaner>     m_nvr_cleaner;
        static StateMutex   m_mutexFFmpeg;

        class nvrData
        {

        public:

            nvrData(void);
            ~nvrData();

            Result Init(AVFormatContext * format_ctx, const char * channel_name, const char * record_dir,
                        Uint64 file_duration_sec = 3600, Uint64 max_age_minutes = 120);
            void Deinit();
            bool IsInit() const;

            Result WritePacket(const AVFormatContext * inCtx, /*const*/ AVPacket & packet);

        private /*functions*/:

        private /*variables*/:

            mt_const StRef<String> m_record_dir;
            mt_const StRef<String> m_channel_name;
            Uint64 m_file_duration_sec;
            Uint64 m_max_age_minutes;
            AVFormatContext * m_out_format_ctx;

            bool m_bIsInit;

        };

        nvrData m_nvrData;
    };

    ffmpegStreamData m_ffmpegStreamData;
    Ref<MConfig::Config> config;



    mt_const Ref<ChannelOptions> channel_opts;
    mt_const Ref<PlaybackItem>   playback_item;

    mt_const DataDepRef<Timers> timers;
    mt_const DataDepRef<PagePool> page_pool;

    mt_const Ref<VideoStream> video_stream;
    mt_const Ref<VideoStream> mix_video_stream;

    mt_const Ref<Thread> workqueue_thread;
    mt_const Ref<Thread> ffmpeg_thread;


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
      //GstBuffer *avc_codec_data_buffer;
      u_int8_t *avc_codec_data_buffer;
      size_t avc_codec_data_buffer_size;

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
      // notification for the same FFmpegStream instance.
      bool no_video_pending;
      bool got_video_pending;
      bool error_pending;
      bool eos_pending;
      // If 'true', then no further notifications for this FFmpegStream
      // instance should be made.
      // 'close_notified' is set to true after error or eos notification.
      bool close_notified;

      // If 'true', then the stream associated with this FFmpegStream
      // instance has been closed, which means that all associated ffmpeg
      // objects should be released.
      bool stream_closed;

      // Bytes received (in_stats_el's sink pad).
      Uint64 rx_bytes;
      // Bytes generated (audio fakesink's sink pad).
      Uint64 rx_audio_bytes;
      // Bytes generated (video fakesink's sink pad).
      Uint64 rx_video_bytes;

      LibMary_ThreadLocal *tlocal;

      bool m_bIsPlaying;

    mt_end

    mt_const Cb<MediaSource::Frontend> frontend;

    static void workqueueThreadFunc (void *_self);

    // thread for sending of 'ffmpeg' packets (video and/or audio)
    static void ffmpegThreadFunc (void *_self);

  // Pipeline manipulation

    void createSmartPipelineForUri ();

    void ffmpeg_send_packets(void);

    void doCreatePipeline ();
    void doReleasePipeline ();

    mt_unlocks (mutex) void pipelineCreationFailed ();

  // Audio/video data handling

    mt_mutex (mutex) void reportMetaData ();


  // Audio data handling



  // Video data handling

    mt_mutex (mutex) void doVideoData (AVPacket & packet, AVCodecContext * pctx, int time_base_den);

    int WriteB8ToBuffer(Int32 b, MemoryEx & memory);
    int WriteB16ToBuffer(Uint32 val, MemoryEx & memory);
    int WriteB32ToBuffer(Uint32 val, MemoryEx & memory);
    int WriteDataToBuffer(ConstMemory const memory, MemoryEx & memoryOut);
    int AvcParseNalUnits(ConstMemory const mem, MemoryEx * pMemoryOut);
    int IsomWriteAvcc(ConstMemory const memory, MemoryEx & memoryOut);

#if 0
    static gboolean videoDataCb (GstPad    *pad,
                 GstBuffer *buffer,
                 gpointer   _self);

    static void handoffVideoDataCb (GstElement *element,
                    GstBuffer  *buffer,
                    GstPad     *pad,
                    gpointer    _self);
#endif
    static void noVideoTimerTick (void *_self);



  mt_iface (VideoStream::EventHandler)

    static VideoStream::EventHandler mix_stream_handler;

  mt_iface_end

public:
  mt_iface (MediaSource)
    void createPipeline ();
    void releasePipeline ();

    void getTrafficStats (TrafficStats * mt_nonnull ret_traffic_stats);
    void resetTrafficStats ();

    void beginPushPacket();
  mt_iface_end

    mt_const void init (CbDesc<MediaSource::Frontend> const &frontend,
                        Timers            *timers,
                        DeferredProcessor *deferred_processor,
			PagePool          *page_pool,
			VideoStream       *video_stream,
			VideoStream       *mix_video_stream,
			Time               initial_seek,
                        ChannelOptions    *channel_opts,
                        PlaybackItem      *playback_item,
                        const Ref<MConfig::Config> & config);

     FFmpegStream ();
    ~FFmpegStream ();
};

}


#endif /* MOMENT__FFMPEG_STREAM__H__ */

