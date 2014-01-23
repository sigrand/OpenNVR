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


#include <libmary/module_init.h>
#include <moment/libmoment.h>

#include "tsmux/tsmux.h"
#include <moment-hls/inc.h>


// Unnecessary list of frames which is kept around for possible future optimizations.
//#define MOMENT_HLS__FRAME_LIST


#define MOMENT_HLS__HEADERS_DATE \
	Byte date_buf [unixtimeToString_BufSize]; \
	Size const date_len = unixtimeToString (Memory::forObject (date_buf), getUnixtime());

#define MOMENT_HLS__COMMON_HEADERS \
	"Server: Moment/1.0\r\n" \
	"Date: ", ConstMemory (date_buf, date_len), "\r\n" \
	"Connection: Keep-Alive\r\n"

#define MOMENT_HLS__OK_HEADERS(mime_type, content_length) \
	"HTTP/1.1 200 OK\r\n" \
	MOMENT_HLS__COMMON_HEADERS \
	"Content-Type: ", (mime_type), "\r\n" \
	"Content-Length: ", (content_length), "\r\n" \
	"Cache-Control: no-cache\r\n"

#define MOMENT_HLS__404_HEADERS(content_length) \
	"HTTP/1.1 404 Not Found\r\n" \
	MOMENT_HLS__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_HLS__400_HEADERS(content_length) \
	"HTTP/1.1 400 Bad Request\r\n" \
	MOMENT_HLS__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_HLS__500_HEADERS(content_length) \
	"HTTP/1.1 500 Internal Server Error\r\n" \
	MOMENT_HLS__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"


using namespace M;
using namespace Moment;

static LogGroup libMary_logGroup_hls        ("mod_hls",        LogLevel::D);
static LogGroup libMary_logGroup_hls_seg    ("mod_hls.seg",    LogLevel::D);
static LogGroup libMary_logGroup_hls_msg    ("mod_hls.msg",    LogLevel::I);
static LogGroup libMary_logGroup_hls_timers ("mod_hls.timers", LogLevel::I);

static ConstMemory const m3u8_mime_type   = "application/vnd.apple.mpegurl";
static ConstMemory const mpegts_mime_type = "video/mp2t";

namespace {
class HlsServer : public Object
{
private:
    StateMutex mutex;

    class HlsFrame : public Referenced,
                     public IntrusiveListElement<>
    {
    public:
        // 'false' for audio.
        bool is_video_frame;
        Uint64 timestamp_nanosec;
#ifdef MOMENT_HLS__FRAME_LIST
        Uint64 arrival_time_seconds;
#endif

        VideoStream::VideoFrameType video_frame_type;
        VideoStream::AudioFrameType audio_frame_type;

        bool random_access;
        bool is_data_frame;

        CodeDepRef<PagePool> page_pool;
        PagePool::PageListHead page_list;
        Size msg_offset;

        ~HlsFrame ()
        {
            logD (hls_msg, _this_func_);
            if (page_pool)
                page_pool->msgUnref (page_list.first);
        }
    };

#ifdef MOMENT_HLS__FRAME_LIST
    typedef IntrusiveList<HlsFrame> HlsFrameList;
#endif

    class HlsSegment : public Referenced,
                       public IntrusiveListElement<>
    {
    public:
        Uint64 seg_no;
        Uint64 first_timestamp;
        Size seg_len;

        // Excluded from playlist? (i.e. scheduled for deletion)
        bool excluded;
        Uint64 excluded_time;

        PagePool::PageListHead page_list;

        HlsSegment ()
            : excluded (false),
              excluded_time (0)
        {}

        ~HlsSegment ()
        {
            logD (hls_msg, _this_func_);
        }
    };

    typedef IntrusiveList<HlsSegment> HlsSegmentList;

    class StreamSession;

    class SegmentSessionList_name;

    class SegmentSession : public Object,
                           public IntrusiveListElement<SegmentSessionList_name>
    {
    public:
        mt_const WeakRef<StreamSession> weak_stream_session;

        mt_const DataDepRef<Sender> conn_sender;
        mt_const bool        conn_keepalive;
        mt_const IpAddress   client_address;
        mt_const Ref<String> request_line;

        mt_const Uint64 seg_no;

        mt_mutex (StreamSession::mutex) bool in_forming_list;

        mt_mutex (StreamSession::mutex) bool valid;
        mt_mutex (StreamSession::mutex) GenericInformer::SubscriptionKey sender_sbn;

        SegmentSession ()
            : conn_sender (this /* coderef_container */)
        {}

        ~SegmentSession ()
        {
            logD (hls_seg, _this_func_);
        }
    };

    typedef IntrusiveList <SegmentSession, SegmentSessionList_name> SegmentSessionList;

    class HlsStream;

    struct NewPacketCbData
    {
        mt_const StreamSession *stream_session;

        PagePool::Page *first_new_page;
        Size new_page_offs;
    };

    class WatchedStreamSessionList_name;
    class StartedStreamSessionList_name;
    class StreamSessionCleanupList_name;
    class StreamSessionReleaseQueue_name;

public:
    class StreamOptions
    {
    public:
        mt_const bool one_session_per_stream;
        mt_const bool realtime_mode;
        mt_const bool no_audio;
        mt_const bool no_video;
        mt_const bool no_rtmp_audio;
        mt_const bool no_rtmp_video;
        // Meaningful only in real-time mode.
        mt_const Uint64 realtime_target_len;
        mt_const Uint64 segment_duration_millisec;
        mt_const Uint64 max_segment_len;
        mt_const Uint64 num_real_segments;
        mt_const Uint64 num_lead_segments;
    };

private:
    class StreamSession : public Object,
                          public HashEntry<>,
                          public IntrusiveListElement<WatchedStreamSessionList_name>,
                          public IntrusiveListElement<StartedStreamSessionList_name>,
                          public IntrusiveListElement<StreamSessionCleanupList_name>,
                          public IntrusiveListElement<StreamSessionReleaseQueue_name>
    {
    public:
        StateMutex mutex;

        mt_const WeakRef<HlsServer> weak_hls_server;

        mt_const StreamOptions opts;

        mt_mutex (HlsServer::mutex) bool in_release_queue;
        mt_mutex (HlsServer::mutex) Time release_time;

        mt_mutex (HlsServer::mutex) bool watched_now;
        mt_mutex (HlsServer::mutex) Uint64 last_request_time_millisec;

        mt_const Ref<String> session_id;

        mt_const TsMux        *tsmux;
        mt_const TsMuxStream  *audio_ts;
        mt_const TsMuxStream  *video_ts;

        mt_const Ref<HlsStream> hls_stream;
        mt_const DataDepRef<PagePool> page_pool;

        mt_mutex (mutex) bool valid;

        // If 'true', then the session in on 'hls_stream->started_stream_sessions' list.
        mt_mutex (mutex) bool started;

        Uint64 last_frame_timestamp;

        // Used for calling new_packet_cb()
        NewPacketCbData new_packet_cb_data;

        Uint64 oldest_seg_no;
        Uint64 newest_seg_no;
        // First segment to appear in the playlist.
        Uint64 head_seg_no;

        HlsSegmentList segment_list;
        Size num_active_segments;

        Ref<HlsSegment> forming_segment;
        Size forming_seg_no;

        // A session can be either in 'forming_seg_sessions' list or
        // in 'waiting_seg_sessions' list, never in both lists at the same time.
        SegmentSessionList forming_seg_sessions;
        SegmentSessionList waiting_seg_sessions;

        bool got_first_pts;
        guint64 first_pts;

        // TEST
        Uint64 last_pts;

        void trimSegmentList (Time cur_time);

        void newFrameAdded (HlsFrame * mt_nonnull frame,
                            bool      force_finish_segment);

        mt_mutex (mutex) void newFrameAdded_unlocked (HlsFrame * mt_nonnull frame,
                                                      bool      force_finish_segment);

        Result addSegmentSession (SegmentSession * mt_nonnull seg_session);

        mt_mutex (mutex) void destroySegmentSession (SegmentSession * mt_nonnull seg_session);

        mt_mutex (mutex) static gboolean new_packet_cb (guint8 *data,
                                                        guint   len,
                                                        void   *_new_packet_cb_data,
                                                        gint64  /* new_pcr */);

        mt_mutex (mutex) void doNewPacketCb (guint8          *data_buf,
                                             guint            data_len,
                                             NewPacketCbData *new_packet_cb_data);

        mt_mutex (mutex) void finishSegment (PagePool::Page **first_new_page,
                                             Size            *new_page_offs);

      mt_iface (Sender::Frontend)
        static Sender::Frontend const sender_event_handler;

        static void senderClosed (Exception *exc_,
                                  void      *_seg_session);
      mt_iface_end

        StreamSession (StreamOptions * const mt_nonnull opts)
            : opts                 (*opts),
              in_release_queue     (false),
              release_time         (0),
              watched_now          (false),
              last_request_time_millisec (0),
              page_pool            (this /* coderef_container */),
              last_frame_timestamp (0),
              oldest_seg_no        (0),
              newest_seg_no        (0),
              head_seg_no          (0),
              num_active_segments  (0),
              forming_seg_no       (0),
              got_first_pts        (false),
              first_pts            (0),
              last_pts             (0)
        {}

        ~StreamSession ()
        {
            logD (hls, _func_);
        }
    };

    typedef Hash< StreamSession,
		  Memory,
		  MemberExtractor< StreamSession,
				   Ref<String>,
				   &StreamSession::session_id,
				   Memory,
				   AccessorExtractor< String,
						      Memory,
						      &String::mem > >,
		  MemoryComparator<> >
	    StreamSessionHash;

    typedef IntrusiveList< StreamSession, WatchedStreamSessionList_name >  WatchedStreamSessionList;
    typedef IntrusiveList< StreamSession, StartedStreamSessionList_name >  StartedStreamSessionList;
    typedef IntrusiveList< StreamSession, StreamSessionCleanupList_name >  StreamSessionCleanupList;
    typedef IntrusiveList< StreamSession, StreamSessionReleaseQueue_name > StreamSessionReleaseQueue;

    class StreamDeletionQueue_name;

    class HlsStream : public Object,
                      public IntrusiveListElement<StreamDeletionQueue_name>
    {
    public:
        StateMutex mutex;

        mt_const HlsServer *hls_server;
        mt_mutex (HlsServer::mutex) StreamSession *bound_stream_session;

        WeakRef<VideoStream> weak_video_stream;

        mt_const bool is_rtmp_stream;
        mt_const bool no_audio;
        mt_const bool no_video;

        // Used for MOMENT_HLS__FRAME_LIST only.
        mt_const Uint64 frame_window_nanosec;

        mt_const GenericStringHash::EntryKey hash_key;

        mt_mutex (HlsServer::mutex) GenericInformer::SubscriptionKey stream_sbn;

        mt_mutex (HlsServer::mutex) bool is_closed;
        mt_mutex (HlsServer::mutex) Time closed_time;

        mt_mutex (mutex) Ref<HlsFrame> avc_codec_data_frame;
        mt_mutex (mutex) Time last_codec_data_time_millisec;

        mt_mutex (mutex) StartedStreamSessionList started_stream_sessions;

        struct AacCodecData
        {
            DataDepRef<PagePool> page_pool;
            PagePool::PageListHead page_list;
            Size msg_offset;
            Size msg_len;

            AacCodecData (Object * const coderef_container)
                : page_pool (coderef_container)
            {}
        };

        mt_mutex (mutex) AacCodecData aac_codec_data;
        mt_mutex (mutex) bool got_aac_codec_data;
        mt_mutex (mutex) bool got_avc_codec_data;
        mt_mutex (mutex) Size nal_length_size;

#ifdef MOMENT_HLS__FRAME_LIST
        mt_mutex (mutex) HlsFrameList frame_list;
        mt_mutex (mutex) Size frame_list_size;
#endif

        void muxMpegtsSegment (PagePool               * mt_nonnull page_pool,
                               PagePool::PageListHead * mt_nonnull data_page_list,
                               Size                   * mt_nonnull ret_data_len);

        HlsStream ()
            : bound_stream_session (NULL),
              is_rtmp_stream (false),
              no_audio  (false),
              no_video  (false),
              is_closed (false),
              closed_time (0),
              last_codec_data_time_millisec (0),
              aac_codec_data (this /* coderef_container */),
              got_aac_codec_data (false),
              got_avc_codec_data (false),
              nal_length_size (0)
#ifdef MOMENT_HLS__FRAME_LIST
              ,
              frame_list_size (0)
#endif
        {}

        ~HlsStream ()
        {
            logD (hls, _func_);

#ifdef MOMENT_HLS__FRAME_LIST
            mutex.lock ();
            {
                HlsFrameList::iter iter (frame_list);
                while (!frame_list.iter_done (iter)) {
                    HlsFrame * const frame = frame_list.iter_next (iter);
                    frame->unref ();
                }
                frame_list.clear();
                frame_list_size = 0;
            }
            mutex.unlock ();
#endif
        }
    };

    typedef IntrusiveList<HlsStream, StreamDeletionQueue_name> StreamDeletionQueue;
    typedef StringHash< Ref<HlsStream> > HlsStreamHash;

    mt_const StreamOptions default_opts;

    mt_const bool   insert_au_delimiters;
    mt_const bool   send_codec_data;
    mt_const Uint64 codec_data_interval_millisec;
    mt_const Uint64 target_duration_seconds;
    mt_const Uint64 num_dummy_starters;
    mt_const Time   stream_timeout;
    mt_const Time   watcher_timeout_millisec;
    mt_const Uint64 frame_window_seconds;

    mt_const Ref<String> extinf_str;

    mt_const Timers *timers;
    mt_const PagePool *page_pool;

    DeferredProcessor::Registration def_reg;

    // TODO These are unnecessary.
    mt_const Timers::TimerKey segments_cleanup_timer;
    mt_const Timers::TimerKey stream_session_cleanup_timer;
    mt_const Timers::TimerKey watched_streams_timer;

    MOMENT_HLS__DATA

#ifdef MOMENT_HLS_DEMO
    mt_mutex (mutex) bool send_delimiters;
#endif

    mt_mutex (mutex) Uint64 stream_id_counter;

    mt_mutex (mutex) HlsStreamHash hls_stream_hash;

    mt_mutex (mutex) StreamSessionHash stream_sessions;
    mt_mutex (mutex) WatchedStreamSessionList watched_stream_sessions;
    mt_mutex (mutex) StreamSessionCleanupList stream_session_cleanup_list;
    mt_mutex (mutex) StreamSessionReleaseQueue stream_session_release_queue;

    static void segmentsCleanupTimerTick (void *_self);

    static void streamSessionCleanupTimerTick (void *_self);

    static void watchedStreamsTimerTick (void *_self);

#ifdef MOMENT_HLS_DEMO
    static void watchedStreamsTimerTick_ (void *_self);
#endif

    mt_mutex (hls_stream->mutex) Result muxMpegtsH264CodecData (HlsStream              * mt_nonnull hls_stream,
                                                                PagePool::PageListHead * mt_nonnull from_pages,
                                                                Size                    from_offset,
                                                                Size                    from_len,
                                                                HlsFrame               * mt_nonnull dst_frame);

    mt_mutex (hls_stream->mutex) Result muxMpegtsH264Frame (HlsStream              * mt_nonnull hls_stream,
                                                            PagePool::PageListHead * mt_nonnull from_pages,
                                                            Size                    from_offset,
                                                            Size                    from_len,
                                                            HlsFrame               * mt_nonnull dst_frame);

    mt_mutex (hls_stream->mutex) Result muxMpegtsAacFrame (HlsStream              * mt_nonnull hls_stream,
                                                           PagePool::PageListHead * mt_nonnull from_pages,
                                                           Size                    from_offset,
                                                           Size                    from_len,
                                                           HlsFrame               * mt_nonnull dst_frame);

    static mt_mutex (hls_stream->mutex) void doAddFrame (HlsStream *hls_stream,
                                                         HlsFrame  *frame);

  mt_iface (VideoStream::EventHandler)
    static VideoStream::EventHandler const vs_event_handler;

    static void audioMessage (VideoStream::AudioMessage * mt_nonnull msg,
                              void                      *_hls_stream);

    static void videoMessage (VideoStream::VideoMessage * mt_nonnull msg,
                              void                      *_hls_stream);

    static void videoStreamClosed (void *_hls_stream);
  mt_iface_end

  mt_iface (VideoStream::FrameSaver::FrameHandler)
    static VideoStream::FrameSaver::FrameHandler const saved_frame_handler;

    static Result savedAudioFrame (VideoStream::AudioMessage * mt_nonnull audio_msg,
                                   void                      *_hls_stream);

    static Result savedVideoFrame (VideoStream::VideoMessage * mt_nonnull video_msg,
                                   void                      *_hls_stream);
  mt_iface_end

  mt_iface (MomentServer::VideoStreamHandler)
    static MomentServer::VideoStreamHandler const video_stream_handler;

    static void videoStreamAdded (VideoStream * mt_nonnull video_stream,
                                  ConstMemory  stream_name,
                                  void        *cb_data);
  mt_iface_end

    Result sendHttpNotFound (HttpRequest * mt_nonnull req,
                             Sender      * mt_nonnull conn_sender)
    {
        return sendHttpNotFound (req->getClientAddress(),
                                 req->getRequestLine(),
                                 req->getKeepalive(),
                                 conn_sender,
                                 page_pool);
    }

    static Result sendHttpNotFound (IpAddress    client_address,
                                    ConstMemory  request_line,
                                    bool         keepalive,
                                    Sender      * mt_nonnull conn_sender,
                                    PagePool    * mt_nonnull page_pool);

    Ref<StreamSession> createStreamSession (ConstMemory stream_name);

    mt_unlocks (mutex) void markStreamSessionRequest (StreamSession * mt_nonnull stream_session);

    mt_mutex (mutex) Ref<HlsServer::StreamSession> doCreateStreamSession (HlsStream *hls_stream);

    mt_mutex (mutex) bool destroyStreamSession (StreamSession * mt_nonnull stream_session,
                                                bool           force_destroy);

    Result processStreamHttpRequest (HttpRequest * mt_nonnull req,
                                     Sender      * mt_nonnull conn_sender,
                                     ConstMemory  stream_name);

    Result processSegmentListHttpRequest (HttpRequest * mt_nonnull req,
                                          Sender      * mt_nonnull conn_sender,
                                          ConstMemory  stream_session_id);

    mt_unlocks (mutex) Result doProcessSegmentListHttpRequest (HttpRequest   *req,
                                                               Sender        * mt_nonnull conn_sender,
                                                               StreamSession *stream_session,
                                                               ConstMemory    path_prefix);

    Result processSegmentHttpRequest (HttpRequest * mt_nonnull req,
                                      Sender      * mt_nonnull conn_sender,
                                      ConstMemory  stream_session_id,
                                      ConstMemory  seg_no_mem);

  mt_iface (HttpService::HttpHandler)
    static HttpService::HttpHandler const http_handler;

    static Result httpRequest (HttpRequest   * mt_nonnull req,
                               Sender        * mt_nonnull conn_sender,
                               Memory const  &msg_body,
                               void         ** mt_nonnull ret_msg_data,
                               void          *_self);
  mt_iface_end

public:
    mt_const void init (MomentServer  * mt_nonnull moment,
                        StreamOptions * mt_nonnull opts,
                        bool           insert_au_delimiters,
                        bool           send_codec_data,
                        Uint64         codec_data_interval_millisec,
                        Uint64         target_duration_seconds,
                        Uint64         num_dummy_starters,
                        Time           stream_timeout,
                        Time           watcher_timeout_millisec,
                        Uint64         frame_window_seconds);

     HlsServer ();
    ~HlsServer ();
};
} // namespace {}

static HlsServer glob_hls_server;


// TODO mod_hls _must_ be loaded before mod_gst (!)

void
HlsServer::StreamSession::trimSegmentList (Time const cur_time)
{
    mutex.lock ();

    HlsSegmentList::iter iter (segment_list);
    while (!segment_list.iter_done (iter)) {
        HlsSegment * const segment = segment_list.iter_next (iter);
        if (!segment->excluded)
            break;

        if (segment->excluded_time < cur_time
            && cur_time - segment->excluded_time >
                   // TODO Calculate this once and remember
                   (opts.segment_duration_millisec / 1000 + 1) *
                           (opts.num_real_segments + opts.num_lead_segments + 1))
        {
            logD (hls_seg, _this_func, "removing segment ", oldest_seg_no);
            segment_list.remove (segment);
            page_pool->msgUnref (segment->page_list.first);
            segment->unref ();
            ++oldest_seg_no;
        }
    }

    mutex.unlock ();
}

void
HlsServer::segmentsCleanupTimerTick (void * const _self)
{
    HlsServer * const self = static_cast <HlsServer*> (_self);
    Time const cur_time = getTime();

    self->mutex.lock ();

    StreamSessionHash::iter iter (self->stream_sessions);
    while (!self->stream_sessions.iter_done (iter)) {
        StreamSession * const stream_session = self->stream_sessions.iter_next (iter);
        stream_session->trimSegmentList (cur_time);
    }

    self->mutex.unlock ();
}

void
HlsServer::streamSessionCleanupTimerTick (void * const _self)
{
    HlsServer * const self = static_cast <HlsServer*> (_self);

    logD (hls_timers, _self_func);

    Time const cur_time = getTime();
    Time const cur_time_millisec = getTimeMilliseconds();

    self->mutex.lock ();

    {
        StreamSession *stop_at_session = NULL;
        StreamSessionCleanupList::iter iter (self->stream_session_cleanup_list);
        while (!self->stream_session_cleanup_list.iter_done (iter)) {
            StreamSession * const stream_session = self->stream_session_cleanup_list.iter_next (iter);
            if (stream_session == stop_at_session) {
                logD (hls_timers, _self_func, "stop_at_session");
                break;
            }

            logD (hls_timers, _self_func, "cur_time_millisec: ", cur_time_millisec, ", "
                  "last_request_time_millisec: ", stream_session->last_request_time_millisec, ", "
                  "stream_timeout: ", self->stream_timeout);

            if (stream_session->last_request_time_millisec > cur_time_millisec) {
                logD (hls_timers, _self_func, "last_request_time > cur_time");
                break;
            }

            if (cur_time_millisec - stream_session->last_request_time_millisec < self->stream_timeout * 1000) {
                logD (hls_timers, _self_func, "not timeout out yet");
                break;
            }

            if (!self->destroyStreamSession (stream_session, false /* force_destory */)) {
                logD (hls_timers, _self_func, "destruction postponed");

                self->stream_session_cleanup_list.remove (stream_session);
                self->stream_session_cleanup_list.append (stream_session);
                if (!stop_at_session)
                    stop_at_session = stream_session;
            }
        }
    }

    {
        StreamSessionReleaseQueue::iter iter (self->stream_session_release_queue);
        while (!self->stream_session_release_queue.iter_done (iter)) {
            StreamSession * const stream_session = self->stream_session_release_queue.iter_next (iter);

            if (stream_session->release_time > cur_time
                || cur_time - stream_session->release_time < self->stream_timeout)
            {
                break;
            }

            bool const destroyed = self->destroyStreamSession (stream_session, true /* force_destroy */);
            assert (destroyed);
        }
    }

    self->mutex.unlock ();
}

void
HlsServer::watchedStreamsTimerTick (void * const _self)
{
    HlsServer * const self = static_cast <HlsServer*> (_self);

    logD (hls_timers, _self_func);

    Time const cur_time_millisec = getTimeMilliseconds ();

    self->mutex.lock ();

    while (!self->watched_stream_sessions.isEmpty()) {
        StreamSession * const stream_session = self->watched_stream_sessions.getFirst();

        if (stream_session->last_request_time_millisec > cur_time_millisec
            || cur_time_millisec - stream_session->last_request_time_millisec < self->watcher_timeout_millisec)
        {
            break;
        }

        Ref<StreamSession> const stream_session_ref =
                Ref<StreamSession>::createNoRef (stream_session);

        self->watched_stream_sessions.remove (stream_session);
        assert (stream_session->watched_now);
        stream_session->watched_now = false;

        Ref<VideoStream> const video_stream = stream_session->hls_stream->weak_video_stream.getRef();
        if (video_stream) {
            self->mutex.unlock ();
            video_stream->minusOneWatcher ();
            self->mutex.lock ();
        }
    }

    self->mutex.unlock ();
}

#ifdef MOMENT_HLS_DEMO
void
HlsServer::watchedStreamsTimerTick_ (void * const _self)
{
    HlsServer * const self = static_cast <HlsServer*> (_self);

    self->mutex.lock ();
    self->send_delimiters = true;
    self->mutex.unlock ();
}
#endif

#define _GST_GET(__data, __idx, __size, __shift) \
    (((guint##__size) (((const guint8 *) (__data))[__idx])) << (__shift))

#define GST_READ_UINT8(data)            (_GST_GET (data, 0,  8,  0))

#define GST_READ_UINT16_BE(data)        (_GST_GET (data, 0, 16,  8) | \
                                         _GST_GET (data, 1, 16,  0))

#define GST_READ_UINT32_BE(data)        (_GST_GET (data, 0, 32, 24) | \
                                         _GST_GET (data, 1, 32, 16) | \
                                         _GST_GET (data, 2, 32,  8) | \
                                         _GST_GET (data, 3, 32,  0))

mt_mutex (hls_stream->mutex) Result
HlsServer::muxMpegtsH264CodecData (HlsStream              * const mt_nonnull hls_stream,
                                   PagePool::PageListHead * const mt_nonnull from_pages,
                                   Size                     const from_offset,
                                   Size                     const from_len,
                                   HlsFrame               * const mt_nonnull dst_frame)
{
    logD (hls_msg, _func, "from_len: ", from_len, ", from_offset: ", from_offset);

    dst_frame->page_pool = page_pool;
    dst_frame->msg_offset = 0;

    PagePool::PageListArray arr (from_pages->first, from_offset, from_len);
    unsigned char tmp_buf [4];

#if 0
// Unnecessary
    if (hls_stream->is_rtmp_stream || insert_au_delimiters) {
      // This is access_unit_delimiter_rbsp() { primary_pic_type[u(3)] rbsp_trailing_bits() }
        Byte const test_prefix [6] = { 0, 0, 0, 1, 0x09, 0xf0 };
        page_pool->getFillPages (&dst_frame->page_list, ConstMemory::forObject (test_prefix));
    }
#endif

    guint8 startcode[4] = { 0x00, 0x00, 0x00, 0x01 };
    gulong offset = 4, i = 0, nb_sps = 0, nb_pps = 0;

  {
    /* Get NAL length size */
    if (from_len < offset + 1) {
        logD (hls_msg, _func, "No NAL length size");
        goto _failure;
    }
    arr.get (offset, Memory (tmp_buf, 1));
    Size const nal_length_size = (tmp_buf [0] & 0x03) + 1;
    hls_stream->nal_length_size = nal_length_size;
    offset++;
    logD (hls_msg, _func, "NAL length size: ", nal_length_size);

    /* How many SPS */
    if (from_len < offset + 1) {
        logD (hls_msg, _func, "No SPS count");
        goto _failure;
    }
    arr.get (offset, Memory (tmp_buf, 1));
    nb_sps = tmp_buf [0] & 0x1f;
    offset++;
    logD (hls_msg, _func, "SPS count: ", nb_sps);

    /* For each SPS */
    for (i = 0; i < nb_sps; i++) {
        logD (hls_msg, _func, "SPS iteration");

        if (from_len < offset + 2) {
            logD (hls_msg, _func, "No SPS size");
            goto _failure;
        }
        arr.get (offset, Memory (tmp_buf, 2));
        guint16 sps_size = GST_READ_UINT16_BE (tmp_buf);
        /* Jump over SPS size */
        offset += 2;
        logD (hls_msg, _func, "SPS size: ", sps_size);

        /* Fake a start code */
        page_pool->getFillPages (&dst_frame->page_list, ConstMemory::forObject (startcode));

        /* Now push the SPS */
        if (from_len < offset + sps_size) {
            logD (hls_msg, _func, "No SPS body");
            goto _failure;
        }
        logD (hls_msg, _func, "from_len: ", from_len, ", offset: ", offset, ", sps_size: ", sps_size,
              ", getNextInPageOffset(): ", arr.getNextInPageOffset());
        page_pool->getFillPagesFromPages (&dst_frame->page_list,
                                          arr.getNextPageToAccess(),
                                          arr.getNextInPageOffset(),
                                          sps_size);
        offset += sps_size;
    }

    /* How many PPS */
    if (from_len < offset + 1) {
        logD (hls_msg, _func, "No PPS count");
        goto _failure;
    }
    arr.get (offset, Memory (tmp_buf, 1));
    nb_pps = tmp_buf [0];
    offset++;
    logD (hls_msg, _func, "PPS count: ", nb_pps);

    /* For each PPS */
    for (i = 0; i < nb_pps; i++) {
        logD (hls_msg, _func, "PPS iteration");

        if (from_len < offset + 2) {
            logD (hls_msg, _func, "No PPS size");
            goto _failure;
        }
        arr.get (offset, Memory (tmp_buf, 2));
        gint pps_size = GST_READ_UINT16_BE (tmp_buf);
        /* Jump over PPS size */
        offset += 2;
        logD (hls_msg, _func, "PPS size: ", pps_size);

        /* Fake a start code */
        page_pool->getFillPages (&dst_frame->page_list, ConstMemory::forObject (startcode));

        /* Now push the PPS */
        if (from_len < offset + pps_size) {
            logD (hls_msg, _func, "No PPS body");
            goto _failure;
        }
        logD (hls_msg, _func, "offset: ", offset, ", getNextInPageOffset(): ", arr.getNextInPageOffset());
        page_pool->getFillPagesFromPages (&dst_frame->page_list,
                                          arr.getNextPageToAccess(),
                                          arr.getNextInPageOffset(),
                                          pps_size);
        offset += pps_size;
    }
  }

    logD (hls_msg, _func, "Success");
    return Result::Success;

_failure:
    logD (hls_msg, _func, "Failure");
    return Result::Failure;
}

mt_mutex (hls_stream->mutex) Result
HlsServer::muxMpegtsH264Frame (HlsStream              * const hls_stream,
                               PagePool::PageListHead * const mt_nonnull from_pages,
                               Size                     const from_offset,
                               Size                     const from_len,
                               HlsFrame               * const dst_frame)
{
    dst_frame->page_pool = page_pool;
    dst_frame->msg_offset = 0;

    PagePool::PageListArray arr (from_pages->first, from_offset, from_len);
    unsigned char tmp_buf [4];

    if (hls_stream->is_rtmp_stream || insert_au_delimiters) {
      // This is access_unit_delimiter_rbsp() { primary_pic_type[u(3)] rbsp_trailing_bits() }
        Byte const test_prefix [6] = { 0, 0, 0, 1, 0x09, 0xf0 };
        page_pool->getFillPages (&dst_frame->page_list, ConstMemory::forObject (test_prefix));
    }

    guint8 startcode[4] = { 0x00, 0x00, 0x00, 0x01 };
    gsize in_offset = 0;

    if (from_len < 4)
        goto _failure;

    while (in_offset < from_len) {
        guint32 nal_size = 0;

        switch (hls_stream->nal_length_size) {
            case 1:
                if (from_len < in_offset + 1)
                    goto _failure;
                arr.get (in_offset, Memory (tmp_buf, 1));
                nal_size = GST_READ_UINT8 (tmp_buf);
                break;
            case 2:
                if (from_len < in_offset + 2)
                    goto _failure;
                arr.get (in_offset, Memory (tmp_buf, 2));
                nal_size = GST_READ_UINT16_BE (tmp_buf);
                break;
            case 4:
                if (from_len < in_offset + 4)
                    goto _failure;
                arr.get (in_offset, Memory (tmp_buf, 4));
                nal_size = GST_READ_UINT32_BE (tmp_buf);
                break;
            default:
                logW (hls_msg, _func, "Unsupported NAL length size: ", hls_stream->nal_length_size);
        }
        in_offset += hls_stream->nal_length_size;

        /* Generate an Elementary stream buffer by inserting a startcode */
        page_pool->getFillPages (&dst_frame->page_list, ConstMemory::forObject (startcode));

        if (from_len < in_offset + nal_size)
            goto _failure;

        page_pool->getFillPagesFromPages (&dst_frame->page_list,
                                          arr.getNextPageToAccess(),
                                          arr.getNextInPageOffset(),
                                          nal_size);
        in_offset += nal_size;
    }

    return Result::Success;

_failure:
    return Result::Failure;
}

mt_mutex (hls_stream->mutex) Result
HlsServer::muxMpegtsAacFrame (HlsStream              * const hls_stream,
                              PagePool::PageListHead * const mt_nonnull from_pages,
                              Size                     const from_offset,
                              Size                     const from_len,
                              HlsFrame               * const mt_nonnull dst_frame)
{
    assert (hls_stream->got_aac_codec_data);

    dst_frame->page_pool = page_pool;
    dst_frame->msg_offset = 0;

    PagePool::PageListArray codec_data_arr (hls_stream->aac_codec_data.page_list.first,
                                            hls_stream->aac_codec_data.msg_offset,
                                            hls_stream->aac_codec_data.msg_len);
    unsigned char tmp_buf [2];

    guint8 adts_header[7] = { 0, };
    Size const out_len = from_len + 7;

    guint8 rate_idx = 0, channels = 0;

    /* Generate ADTS header */
    if (hls_stream->aac_codec_data.msg_len < 2)
        return Result::Failure;

    codec_data_arr.get (0 /* offset */, Memory (tmp_buf, 2));

    // Note: For reference, see http://wiki.multimedia.cx/index.php?title=ADTS
    //
    // Note: gst-plugins-bad/gst/mpegtsmux/mpegtsmux_aac.c and
    //       gst-plugins-bad/gst/mpegpsmux/mpegpsmux_aac.c
    //       contain a bug: wrong bits taken for obj_type,
    //                      and obj_type++ there is incorrect.
    //
    Byte const obj_type = (((tmp_buf [0] & 0xf8) >> 3) & 0x3) - (1 /* for adts header */);
//
// Old (wrong) code:
//    obj_type = (GST_READ_UINT8 (tmp_buf) & 0xC) >> 2;
//    obj_type++;

    rate_idx = (GST_READ_UINT8 (tmp_buf) & 0x3) << 1;
    rate_idx |= (GST_READ_UINT8 (tmp_buf + 1) & 0x80) >> 7;
    channels = (GST_READ_UINT8 (tmp_buf + 1) & 0x78) >> 3;
    /* Sync point over a full byte */
    adts_header[0] = 0xFF;
    /* Sync point continued over first 4 bits + static 4 bits
     * (ID, layer, protection)*/
    // TODO ID is MPEG version. 0 for MPEG-4, 1 for MPEG-2.
    adts_header[1] = 0xF1;
    /* Object type over first 2 bits */
    adts_header[2] = obj_type << 6;
    /* rate index over next 4 bits */
    adts_header[2] |= (rate_idx << 2);
    /* channels over last 2 bits */
    adts_header[2] |= (channels & 0x4) >> 2;
    /* channels continued over next 2 bits + 4 bits at zero */
    adts_header[3] = (channels & 0x3) << 6;
    /* frame size over last 2 bits */
    adts_header[3] |= (out_len & 0x1800) >> 11;
    /* frame size continued over full byte */
    adts_header[4] = (out_len & 0x1FF8) >> 3;
    /* frame size continued first 3 bits */
    adts_header[5] = (out_len & 0x7) << 5;
    /* buffer fullness (0x7FF for VBR) over 5 last bits */
    adts_header[5] |= 0x1F;
    /* buffer fullness (0x7FF for VBR) continued over 6 first bits + 2 zeros for
     * number of raw data blocks */
    adts_header[6] = 0xFC;

    /* Insert ADTS header */
    page_pool->getFillPages (&dst_frame->page_list, ConstMemory::forObject (adts_header));

    /* Now copy complete frame */
    page_pool->getFillPagesFromPages (&dst_frame->page_list,
                                      from_pages->first,
                                      from_offset,
                                      from_len);

    return Result::Success;
}

VideoStream::EventHandler const HlsServer::vs_event_handler = {
    audioMessage,
    videoMessage,
    NULL /* rtmpCommandMessage */,
    videoStreamClosed,
    NULL /* numWatchersChanged */
};

void
HlsServer::audioMessage (VideoStream::AudioMessage * const mt_nonnull msg,
                         void                      * const _hls_stream)
{
    // TODO Mux only if there're stream sessions (i.e. when doAddFrame() is not a no-op).

//    logD (hls_msg, _func, "timestamp: 0x", fmt_hex, msg->timestamp_nanosec, " (", fmt_def, msg->timestamp_nanosec, ") ",
//          msg->codec_id, " ", msg->frame_type, " msg_offset ", msg->msg_offset);

    PagePool *normalized_page_pool;
    PagePool::PageListHead normalized_pages;
    Size normalized_offs = 0;
    bool unref_normalized_pages;
    if (msg->prechunk_size == 0) {
        normalized_page_pool = msg->page_pool;
        normalized_pages = msg->page_list;
        normalized_offs = msg->msg_offset;
        unref_normalized_pages = false;
    } else {
        unref_normalized_pages = true;
        RtmpConnection::normalizePrechunkedData (msg,
                                                 msg->page_pool,
                                                 &normalized_page_pool,
                                                 &normalized_pages,
                                                 &normalized_offs);
    }

    HlsStream * const hls_stream = static_cast <HlsStream*> (_hls_stream);
    HlsServer * const self = hls_stream->hls_server;

    if (msg->frame_type == VideoStream::AudioFrameType::AacSequenceHeader) {
        hls_stream->mutex.lock ();

        if (hls_stream->aac_codec_data.page_pool)
            hls_stream->aac_codec_data.page_pool->msgUnref (hls_stream->aac_codec_data.page_list.first);

        hls_stream->aac_codec_data.page_pool = normalized_page_pool;
        hls_stream->aac_codec_data.page_list = normalized_pages;
        if (!unref_normalized_pages)
            normalized_page_pool->msgRef (normalized_pages.first);

        hls_stream->aac_codec_data.msg_offset = normalized_offs;
        hls_stream->aac_codec_data.msg_len = msg->msg_len;

        hls_stream->got_aac_codec_data = true;

        hls_stream->mutex.unlock ();
        return;
    }

#ifdef MOMENT_HLS__FRAME_LIST
    Ref<HlsFrame> const frame = grab (new HlsFrame);
#else
    // No need to malloc.
    HlsFrame auto_frame;
    HlsFrame * const frame = &auto_frame;
#endif
    frame->is_video_frame = false;
    frame->timestamp_nanosec = msg->timestamp_nanosec;
    frame->audio_frame_type = msg->frame_type;

    frame->random_access = false /* TODO gstreamer does 'false', but 'true' looks more logical */;
    frame->is_data_frame = frame->audio_frame_type.isAudioData();

    hls_stream->mutex.lock ();
    if (!hls_stream->got_aac_codec_data) {
        hls_stream->mutex.unlock ();
//        logW (hls_msg, _func, "no AAC codec data");
        goto _return;
    }

    if (!self->muxMpegtsAacFrame (hls_stream,
                                  &normalized_pages,
                                  normalized_offs,
                                  msg->msg_len,
                                  frame))
    {
        hls_stream->mutex.unlock ();
        logE (hls_msg, _func, "muxMpegtsAacData() failed");
        goto _return;
    }

    self->doAddFrame (hls_stream, frame);
    hls_stream->mutex.unlock ();

_return:
    if (unref_normalized_pages)
        normalized_page_pool->msgUnref (normalized_pages.first);
}

void
HlsServer::videoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
                         void                      * const _hls_stream)
{
    // TODO Mux only if there're stream sessions (i.e. when doAddFrame() is not a no-op).

//    logD (hls_msg, _func, "timestamp: 0x", fmt_hex, msg->timestamp_nanosec, " (", fmt_def, msg->timestamp_nanosec, ") ",
//          msg->codec_id, " ", msg->frame_type, " msg_offset ", msg->msg_offset);

//    logLock ();
//    PagePool::dumpPages (logs, &msg->page_list);
//    logUnlock ();

    if (!msg->frame_type.isVideoData()
        && msg->frame_type != VideoStream::VideoFrameType::AvcSequenceHeader
        /* && msg->frame_type != VideoStream::VideoFrameType::AvcEndOfSequence */)
    {
        logD (hls_msg, _func, "ignoring frame by type");
        return;
    }

    PagePool *normalized_page_pool;
    PagePool::PageListHead normalized_pages;
    Size normalized_offs = 0;
    bool unref_normalized_pages;
    if (msg->prechunk_size == 0) {
        normalized_page_pool = msg->page_pool;
        normalized_pages = msg->page_list;
        normalized_offs = msg->msg_offset;
        unref_normalized_pages = false;
    } else {
        unref_normalized_pages = true;
        RtmpConnection::normalizePrechunkedData (msg,
                                                 msg->page_pool,
                                                 &normalized_page_pool,
                                                 &normalized_pages,
                                                 &normalized_offs);
    }

    HlsStream * const hls_stream = static_cast <HlsStream*> (_hls_stream);
    HlsServer * const self = hls_stream->hls_server;

#ifdef MOMENT_HLS__FRAME_LIST
    Ref<HlsFrame> const frame = grab (new HlsFrame);
#else
    HlsFrame auto_frame;
    HlsFrame *frame = &auto_frame;

    Ref<HlsFrame> ref_frame;
    if (msg->frame_type == VideoStream::VideoFrameType::AvcSequenceHeader) {
        ref_frame = grab (new HlsFrame);
        frame = ref_frame;
    }
#endif

    frame->is_video_frame = true;
    frame->timestamp_nanosec = msg->timestamp_nanosec;
    frame->video_frame_type = msg->frame_type;

    frame->random_access = frame->video_frame_type.isKeyFrame();
    frame->is_data_frame = frame->video_frame_type.isVideoData();

    hls_stream->mutex.lock ();

    if (frame->video_frame_type == VideoStream::VideoFrameType::AvcSequenceHeader) {
        if (!self->muxMpegtsH264CodecData (hls_stream,
                                           &normalized_pages,
                                           normalized_offs,
                                           msg->msg_len,
                                           frame))
        {
            hls_stream->mutex.unlock ();
            logE (hls_msg, _func, "muxMpegtsH264CodecData() failed");
            goto _return;
        }

#if 0
        logLock ();
        logD_unlocked (hls_msg, _func, "Muxed AVC codec data:");
        PagePool::dumpPages (logs, &frame->page_list);
        logUnlock ();
#endif

        hls_stream->got_avc_codec_data = true;
        // Note that 'frame' is allocated on heap in this case.
        hls_stream->avc_codec_data_frame = frame;

        if (!hls_stream->is_rtmp_stream && !self->send_codec_data) {
            hls_stream->mutex.unlock ();
            goto _return;
        }
    } else  {
        if (!hls_stream->got_avc_codec_data) {
            hls_stream->mutex.unlock ();
            logW (hls_msg, _func, "no AVC codec data");
            goto _return;
        }

        if (!self->muxMpegtsH264Frame (hls_stream,
                                       &normalized_pages,
                                       normalized_offs,
                                       msg->msg_len,
                                       frame))
        {
            hls_stream->mutex.unlock ();
            logE (hls_msg, _func, "muxMpegtsH264Frame() failed");
            goto _return;
        }
    }

    if ((hls_stream->is_rtmp_stream || self->send_codec_data)
        && hls_stream->avc_codec_data_frame)
    {
        Time const cur_time_millisec = getTimeMilliseconds ();
        if (frame->video_frame_type == VideoStream::VideoFrameType::AvcSequenceHeader) {
            hls_stream->last_codec_data_time_millisec = cur_time_millisec;
        } else {
            if (hls_stream->last_codec_data_time_millisec < cur_time_millisec
                && cur_time_millisec - hls_stream->last_codec_data_time_millisec >=
                           self->codec_data_interval_millisec)
            {
                hls_stream->last_codec_data_time_millisec = cur_time_millisec;

#ifdef MOMENT_HLS__FRAME_LIST
                Ref<HlsFrame> const cdata_frame = grab (new HlsFrame);
#else
                // No need to malloc.
                HlsFrame auto_cdata_frame;
                HlsFrame * const cdata_frame = &auto_cdata_frame;
#endif
                cdata_frame->is_video_frame = true;
                cdata_frame->timestamp_nanosec = frame->timestamp_nanosec;
                cdata_frame->video_frame_type = VideoStream::VideoFrameType::AvcSequenceHeader;
                cdata_frame->random_access = false;
                cdata_frame->is_data_frame = false;
                cdata_frame->page_pool = hls_stream->avc_codec_data_frame->page_pool;
                cdata_frame->page_list = hls_stream->avc_codec_data_frame->page_list;
                cdata_frame->page_pool->msgRef (cdata_frame->page_list.first);
                cdata_frame->msg_offset = hls_stream->avc_codec_data_frame->msg_offset;

                self->doAddFrame (hls_stream, cdata_frame);
            }
        }
    }

    self->doAddFrame (hls_stream, frame);
    hls_stream->mutex.unlock ();

_return:
    if (unref_normalized_pages)
        normalized_page_pool->msgUnref (normalized_pages.first);
}

mt_mutex (hls_stream->mutex) void
HlsServer::doAddFrame (HlsStream * const hls_stream,
                       HlsFrame  * const frame)
{
//    logD (hls_msg, _func_);

#ifdef MOMENT_HLS__FRAME_LIST
// 'frame_list' makes little sense since the data is already mpeg-ts encoded.

    Time const cur_time = getTime();
    frame->arrival_time_seconds = cur_time;

    if (!hls_stream->frame_list.isEmpty()) {
        Uint64 const last_timestamp_nanosec = hls_stream->frame_list.getLast()->timestamp_nanosec;
        while (!hls_stream->frame_list.isEmpty()) {
            HlsFrame * const first_frame = hls_stream->frame_list.getFirst();
            if ((first_frame->timestamp_nanosec < last_timestamp_nanosec
                     && last_timestamp_nanosec - first_frame->timestamp_nanosec > hls_stream->frame_window_nanosec)
                ||
                (first_frame->arrival_time_seconds < cur_time
                     && cur_time - first_frame->arrival_time_seconds > 2 * (hls_stream->frame_window_nanosec / 1000000000))
                ||
                hls_stream->frame_list_size > 20    /* safe enough multiplier */
                                              * 120 /* frames per sec */
                                              * (hls_stream->frame_window_nanosec / 1000000000))
            {
                hls_stream->frame_list.remove (first_frame);
                first_frame->unref ();

                assert (hls_stream->frame_list_size > 0);
                --hls_stream->frame_list_size;
                continue;
            }

            break;
        }
    }

    hls_stream->frame_list.append (frame);
    frame->ref ();
    ++hls_stream->frame_list_size;
#endif

    {
        StartedStreamSessionList::iter iter (hls_stream->started_stream_sessions);
        while (!hls_stream->started_stream_sessions.iter_done (iter)) {
            StreamSession * const stream_session = hls_stream->started_stream_sessions.iter_next (iter);
            stream_session->newFrameAdded (frame, false /* force_finish_segment */);
        }
    }
}

void
HlsServer::videoStreamClosed (void * const _hls_stream)
{
    HlsStream * const hls_stream = static_cast <HlsStream*> (_hls_stream);
    HlsServer * const self = hls_stream->hls_server;

    logD (hls, _func_);

    self->mutex.lock ();

    if (hls_stream->is_closed) {
        logD (hls, _func, "stream 0x", fmt_hex, (UintPtr) hls_stream, " already closed\n");
        self->mutex.unlock ();
        return;
    }
    hls_stream->is_closed = true;
    hls_stream->closed_time = getTime();

    if (hls_stream->bound_stream_session) {
        logD (hls, _func, "appending session 0x", fmt_hex, (UintPtr) hls_stream->bound_stream_session, " "
              "to stream_session_release_queue");
        self->stream_session_release_queue.append (hls_stream->bound_stream_session);
        hls_stream->bound_stream_session->in_release_queue = true;
        hls_stream->bound_stream_session->release_time = getTime();
        logD (hls, _func, "nullifying bound stream session for hls_stream 0x", fmt_hex, (UintPtr) hls_stream);
        hls_stream->bound_stream_session = NULL;
    } else {
        logD (hls, _func, "no bound stream session for hls_stream 0x", fmt_hex, (UintPtr) hls_stream);
    }

    self->hls_stream_hash.remove (hls_stream->hash_key);

    self->mutex.unlock ();
}

VideoStream::FrameSaver::FrameHandler const HlsServer::saved_frame_handler = {
    savedAudioFrame,
    savedVideoFrame
};

Result
HlsServer::savedAudioFrame (VideoStream::AudioMessage * const mt_nonnull audio_msg,
                            void                      * const _hls_stream)
{
    audioMessage (audio_msg, _hls_stream);
    return Result::Success;
}

Result
HlsServer::savedVideoFrame (VideoStream::VideoMessage * const mt_nonnull video_msg,
                            void                      * const _hls_stream)
{
    videoMessage (video_msg, _hls_stream);
    return Result::Success;
}

MomentServer::VideoStreamHandler const HlsServer::video_stream_handler = {
    videoStreamAdded
};

void
HlsServer::videoStreamAdded (VideoStream * const mt_nonnull video_stream,
                             ConstMemory   const stream_name,
                             void        * const _self)
{
    HlsServer * const self = static_cast <HlsServer*> (_self);

    logD (hls, _func, "stream_name: ", stream_name);

    bool is_rtmp_stream;
    {
        ConstMemory const source = video_stream->getParam ("source");
        is_rtmp_stream = equal (source, "rtmp");
    }

    bool no_audio = false;
    ConstMemory const audio_codec = video_stream->getParam ("audio_codec");
    if (!audio_codec.isEmpty()
        && !equal (audio_codec, "aac"))
    {
        no_audio = true;

        // TODO Generalized condition function.
        if (!self->default_opts.no_audio
            && !(self->default_opts.no_rtmp_audio && is_rtmp_stream))
        {
            logD (hls, _func, "incompatible audio codec \"", audio_codec, "\", ignoring stream for HLS");
            return;
        }
    }

    Ref<HlsStream> const hls_stream = grab (new HlsStream);
    logD (hls, _func, "new HlsStream: 0x", fmt_hex, (UintPtr) hls_stream.ptr());

    hls_stream->hls_server = self;
    hls_stream->weak_video_stream = video_stream;

    hls_stream->is_rtmp_stream = is_rtmp_stream;
    hls_stream->no_audio = no_audio;

    hls_stream->frame_window_nanosec = self->frame_window_seconds * 1000000000;

    self->mutex.lock ();
    hls_stream->hash_key = self->hls_stream_hash.add (stream_name, hls_stream);

    if (self->default_opts.one_session_per_stream) {
        Ref<StreamSession> const stream_session = self->doCreateStreamSession (hls_stream);
        assert (stream_session);
        self->mutex.unlock ();

        stream_session->hls_stream->mutex.lock ();
        stream_session->mutex.lock ();

        stream_session->started = true;
        stream_session->hls_stream->started_stream_sessions.append (stream_session);

        stream_session->mutex.unlock ();
        stream_session->hls_stream->mutex.unlock ();
    } else {
        self->mutex.unlock ();
    }

    video_stream->lock ();
    if (video_stream->isClosed_unlocked()) {
        video_stream->unlock ();

        videoStreamClosed (hls_stream.ptr());
        return;
    }

    video_stream->getFrameSaver()->reportSavedFrames (&saved_frame_handler, hls_stream);

    // TODO Unsubscribe later on (this is not strictly necessary).
    hls_stream->stream_sbn =
            video_stream->getEventInformer()->subscribe_unlocked (
                    CbDesc<VideoStream::EventHandler> (
                            &vs_event_handler,
                            hls_stream,
                            self,
                            hls_stream /* ref_data */));
    video_stream->unlock ();
}

HttpService::HttpHandler const HlsServer::http_handler = {
    httpRequest,
    NULL /* httpMessageBody */
};

mt_mutex (mutex) void
HlsServer::StreamSession::doNewPacketCb (guint8          * const data_buf,
                                         guint             const data_len,
                                         NewPacketCbData * const new_packet_cb_data)
{
    page_pool->getFillPages (&forming_segment->page_list,
                             ConstMemory ((unsigned char const *) data_buf, data_len));
    forming_segment->seg_len += data_len;

    if (!new_packet_cb_data->first_new_page)
        new_packet_cb_data->first_new_page = forming_segment->page_list.first;

    if (opts.realtime_mode
        && forming_segment->seg_len >= opts.realtime_target_len)
    {
        if (forming_segment->seg_len > opts.realtime_target_len) {
            logF (hls, _func, "Segment length mismatch: got ", forming_segment->seg_len, ", "
                  "expected ", opts.realtime_target_len);
        }

        finishSegment (&new_packet_cb_data->first_new_page, &new_packet_cb_data->new_page_offs);
    }
}

mt_mutex (mutex) void
HlsServer::StreamSession::finishSegment (PagePool::Page ** const first_new_page,
                                         Size            * const new_page_offs)
{
    logD (hls_msg, _this_func_);

    {
        SegmentSessionList::iter iter (forming_seg_sessions);
        while (!forming_seg_sessions.iter_done (iter)) {
            SegmentSession * const seg_session = forming_seg_sessions.iter_next (iter);

            if (opts.realtime_mode) {
                page_pool->msgRef (*first_new_page);
                seg_session->conn_sender->sendPages (page_pool,
                                                     *first_new_page,
                                                     *new_page_offs,
                                                     true /* do_flush */);
            } else {
                MOMENT_HLS__HEADERS_DATE
                seg_session->conn_sender->send (
                        page_pool,
                        false /* do_flush */,
                        MOMENT_HLS__OK_HEADERS (mpegts_mime_type, forming_segment->seg_len),
                        "\r\n");
                page_pool->msgRef (forming_segment->page_list.first);
                seg_session->conn_sender->sendPages (page_pool,
                                                     forming_segment->page_list.first,
                                                     0    /* msg_offset */,
                                                     true /* do_flush */);
            }

            if (!seg_session->conn_keepalive)
                seg_session->conn_sender->closeAfterFlush ();

            destroySegmentSession (seg_session);

            forming_seg_sessions.remove (seg_session);
            seg_session->unref ();

            logD (hls_seg, _this_func, "segment sent: ", forming_segment->seg_len, " bytes, ",
                  seg_session->client_address, " ", seg_session->request_line);
        }
        assert (forming_seg_sessions.isEmpty());
    }

    logD (hls_seg, _this_func, "appending segment ", forming_seg_no);
    segment_list.append (forming_segment);
    forming_segment->ref ();
    ++num_active_segments;

    if (num_active_segments > opts.num_real_segments) {
        HlsSegment *segment_to_exclude = NULL;
        HlsSegmentList::rev_iter iter (segment_list);
        while (!segment_list.rev_iter_done (iter)) {
            HlsSegment * const segment = segment_list.rev_iter_next (iter);
            if (!segment->excluded)
                segment_to_exclude = segment;
            else
                break;
        }
        assert (segment_to_exclude);

        segment_to_exclude->excluded = true;
        segment_to_exclude->excluded_time = getTime();

        ++head_seg_no;
    }

    newest_seg_no = forming_seg_no;
    ++forming_seg_no;

    forming_segment = grab (new HlsSegment);
    forming_segment->first_timestamp = last_frame_timestamp;
    forming_segment->seg_no = forming_seg_no;
    forming_segment->seg_len = 0;

    *first_new_page = NULL;
    *new_page_offs = 0;

    {
      // TODO O(N), ineffective.
        SegmentSessionList::iter iter (waiting_seg_sessions);
        while (!waiting_seg_sessions.iter_done (iter)) {
            SegmentSession * const seg_session = waiting_seg_sessions.iter_next (iter);
            if (seg_session->seg_no <= forming_seg_no) {
                waiting_seg_sessions.remove (seg_session);
                forming_seg_sessions.append (seg_session);
                seg_session->in_forming_list = true;
            }
        }
    }
}

mt_mutex (mutex) gboolean
HlsServer::StreamSession::new_packet_cb (guint8 * const data_buf,
                                         guint    const data_len,
                                         void   * const _new_packet_cb_data,
                                         gint64   const /* new_pcr */)
{
    NewPacketCbData * const new_packet_cb_data = static_cast <NewPacketCbData*> (_new_packet_cb_data);
    StreamSession * const stream_session = new_packet_cb_data->stream_session;

    stream_session->doNewPacketCb (data_buf, data_len, new_packet_cb_data);

    return TRUE;
}

mt_mutex (mutex) void
HlsServer::StreamSession::newFrameAdded_unlocked (HlsFrame * const mt_nonnull frame,
                                                  bool       const force_finish_segment)
{
//    logD (hls_msg, _this_func_);

    if (!started) {
        logD (hls_msg, _this_func, "not started");
        return;
    }

    if (!frame->is_video_frame) {
        if (!audio_ts) {
            logD (hls_msg, _this_func, "dropping audio frame");
            return;
        }
    } else {
        if (!video_ts) {
            logD (hls_msg, _this_func, "dropping video frame");
            return;
        }
    }

    if (!forming_segment) {
        forming_segment = grab (new HlsSegment);
        forming_segment->first_timestamp = last_frame_timestamp;
        forming_segment->seg_no = forming_seg_no;
        forming_segment->seg_len = 0;

        {
          // TODO O(N), ineffective.
            SegmentSessionList::iter iter (waiting_seg_sessions);
            while (!waiting_seg_sessions.iter_done (iter)) {
                SegmentSession * const seg_session = waiting_seg_sessions.iter_next (iter);
                if (seg_session->seg_no <= forming_seg_no) {
                    waiting_seg_sessions.remove (seg_session);
                    forming_seg_sessions.append (seg_session);
                    seg_session->in_forming_list = true;
                }
            }
        }
    }

    TsMuxStream *cur_ts;
    if (frame->is_video_frame)
        cur_ts = video_ts;
    else
        cur_ts = audio_ts;

    PagePool::Page *first_new_page = forming_segment->page_list.last;
    Size new_page_offs = first_new_page ? first_new_page->data_len : 0;

// Adjusting pts is likely unnecessary.
// Note: not adjusting breaks timestamps for dummy starters [put -1 for pts there].
//#define MOMENT__HLS__ADJUST_PTS

    guint64 pts = -1; // timestamp?
    if (frame->timestamp_nanosec != (Uint64) -1 /* Shaky, just to be sure */) {
        pts = frame->timestamp_nanosec * 9 / 100000;

        if (!got_first_pts) {
            if (frame->is_data_frame) {
                first_pts = pts;
                got_first_pts = true;

#ifndef MOMENT__HLS__ADJUST_PTS
                forming_segment->first_timestamp = frame->timestamp_nanosec;
#endif
            } else {
#ifdef MOMENT__HLS__ADJUST_PTS
                pts = 0;
#endif
            }
        }

#ifdef MOMENT__HLS__ADJUST_PTS
        if (pts >= first_pts)
            pts -= first_pts;
        else
            pts = 0;
#endif

#if 0
        {
          // TEST

            if (last_pts > pts) {
                logW_ (_func, "backwards pts: ", pts, " -> ", last_pts);
//                pts = last_pts;
            }

            last_pts = pts;
        }
#endif
    }

    bool got_data = false;
    {
//        logD_ (_func, "pts: ", pts);

        PagePool::Page *page = frame->page_list.first;
        while (page) {
            if (page->data_len > 0) {
                tsmux_stream_add_data (cur_ts,
                                       page->getData(),
                                       page->data_len,
                                       NULL /* buf */,
                                       pts,
                                       -1 /* dts */,
                                       frame->random_access);
                got_data = true;
            }

            page = page->getNextMsgPage ();
        }
    }

    if (got_data) {
        new_packet_cb_data.stream_session = this;
        new_packet_cb_data.first_new_page = first_new_page;
        new_packet_cb_data.new_page_offs  = new_page_offs;

        // TODO Try to optimize mpegts container overhead.
        while (tsmux_stream_bytes_in_buffer (cur_ts) > 0) {
            if (!tsmux_write_stream_packet (tsmux, cur_ts)) {
                logE (hls_msg, _this_func, "Failed to write data packet");
                // TODO Stop muxing, stream error.
            }
        }

        first_new_page = new_packet_cb_data.first_new_page;
        new_page_offs  = new_packet_cb_data.new_page_offs;
    } // if (got_data)

    {
        bool segment_finished = false;

        if (!opts.realtime_mode
            && forming_segment->seg_len >= opts.max_segment_len)
        {
            segment_finished = true;
        }

        if (got_first_pts) {
            Uint64 const timestamp = pts * 100000 / 9;
            last_frame_timestamp = timestamp;

//            logD_ (_func, "timestamp: ", timestamp, ", first_timestamp: ", forming_segment->first_timestamp,
//                   ", segment_duration_millisec * 1000000: ", opts.segment_duration_millisec * 1000000);

            if (!opts.realtime_mode) {
                if (timestamp > forming_segment->first_timestamp
                    && timestamp - forming_segment->first_timestamp >= opts.segment_duration_millisec * 1000000)
                {
                    segment_finished = true;
                }
            }
        }

        if (force_finish_segment || segment_finished)
            finishSegment (&first_new_page, &new_page_offs);
    }

    if (opts.realtime_mode
        && first_new_page)
    {
        SegmentSessionList::iter iter (forming_seg_sessions);
        while (!forming_seg_sessions.iter_done (iter)) {
            SegmentSession * const seg_session = forming_seg_sessions.iter_next (iter);
            logD (hls_msg, _this_func, "new_page_offs: ", new_page_offs, ", "
                  "first_new_page->data_len: ", first_new_page->data_len);

            {
              // Copying the pages because forming_segment->page_list is not finalized yet,
              // i.e. new pages will be appended to it soon.
                PagePool::PageListHead page_list;
                Size const msg_len = PagePool::countPageListDataLen (first_new_page, new_page_offs);
                page_pool->getFillPagesFromPages (&page_list, first_new_page, new_page_offs, msg_len);
                seg_session->conn_sender->sendPages (page_pool,
                                                     page_list.first,
                                                     0    /* msg_offset */,
                                                     true /* do_flush */);
            }
        }
    }
}

void
HlsServer::StreamSession::newFrameAdded (HlsFrame * const mt_nonnull frame,
                                         bool       const force_finish_segment)
{
    mutex.lock ();
    newFrameAdded_unlocked (frame, force_finish_segment);
    mutex.unlock ();
}

Result
HlsServer::StreamSession::addSegmentSession (SegmentSession * const mt_nonnull seg_session)
{
    mutex.lock ();

    if (!valid) {
        logD (hls_seg, _this_func, "stream session gone");
        Result const res = sendHttpNotFound (seg_session->client_address,
                                             seg_session->request_line->mem(),
                                             seg_session->conn_keepalive,
                                             seg_session->conn_sender,
                                             page_pool);
        destroySegmentSession (seg_session);
        mutex.unlock ();
        return res;
    }

    // If the segment is too old or if it has not been advertised yet, reply 404.
    if (seg_session->seg_no < oldest_seg_no
        || seg_session->seg_no > newest_seg_no + opts.num_lead_segments)
    {
        logD (hls_seg, _this_func, "seg_no ", seg_session->seg_no, " not in segment window "
              "[", oldest_seg_no, ", ", newest_seg_no + opts.num_lead_segments, "]");
        Result const res = sendHttpNotFound (seg_session->client_address,
                                             seg_session->request_line->mem(),
                                             seg_session->conn_keepalive,
                                             seg_session->conn_sender,
                                             page_pool);
        destroySegmentSession (seg_session);
        mutex.unlock ();
        return res;
    }

    // If the segment has already been formed, send it in reply.
    if (!segment_list.isEmpty()
        && seg_session->seg_no <= newest_seg_no)
    {
        HlsSegment *segment = NULL;
        {
            HlsSegmentList::iter iter (segment_list);
            while (!segment_list.iter_done (iter)) {
                segment = segment_list.iter_next (iter);
                if (segment->seg_no >= seg_session->seg_no)
                    break;
            }
            if (!segment) {
                logD (hls_seg, _this_func, "MISSING SEGMENT: seg_no: ", seg_session->seg_no);
            }
            assert (segment);
        }

        Size const segment_len = segment->seg_len;

        MOMENT_HLS__HEADERS_DATE
        seg_session->conn_sender->send (
                page_pool,
                false /* do_flush */,
                MOMENT_HLS__OK_HEADERS (mpegts_mime_type, segment->seg_len),
                "\r\n");
        page_pool->msgRef (segment->page_list.first);
        seg_session->conn_sender->sendPages (page_pool, segment->page_list.first, true /* do_flush */);

        if (!seg_session->conn_keepalive)
            seg_session->conn_sender->closeAfterFlush ();

        destroySegmentSession (seg_session);
        mutex.unlock ();

        logD (hls_seg, _this_func, "segment sent: ", segment_len, " bytes, ",
              seg_session->client_address, " ", seg_session->request_line);

        return Result::Success;
    }

    if (opts.realtime_mode) {
        MOMENT_HLS__HEADERS_DATE
        seg_session->conn_sender->send (
                page_pool,
                false /* do_flush */,
                MOMENT_HLS__OK_HEADERS (mpegts_mime_type, opts.realtime_target_len),
                "\r\n");
    }

    if (!forming_segment
        || forming_seg_no < seg_session->seg_no)
    {
        seg_session->conn_sender->flush ();

        seg_session->in_forming_list = false;
        waiting_seg_sessions.append (seg_session);
        seg_session->ref ();

        mutex.unlock ();
        return Result::Success;
    }

    if (opts.realtime_mode) {
      // Copying the pages because forming_segment->page_list is not finalized yet,
      // i.e. new pages will be appended to it soon.
        PagePool::PageListHead page_list;
        page_pool->getFillPagesFromPages (&page_list,
                                          forming_segment->page_list.first,
                                          0 /* from_offset */,
                                          forming_segment->seg_len);
        seg_session->conn_sender->sendPages (page_pool, page_list.first, true /* do_flush */);
    }

    seg_session->in_forming_list = true;
    forming_seg_sessions.append (seg_session);
    seg_session->ref ();

    mutex.unlock ();

    return Result::Success;
}

mt_mutex (mutex) void
HlsServer::StreamSession::destroySegmentSession (SegmentSession * const mt_nonnull seg_session)
{
    if (!seg_session->valid) {
        return;
    }
    seg_session->valid = false;

    if (seg_session->sender_sbn) {
        seg_session->conn_sender->getEventInformer()->unsubscribe (seg_session->sender_sbn);
        seg_session->sender_sbn = NULL;
    }
}

Result
HlsServer::sendHttpNotFound (IpAddress     const /* client_address */,
                             ConstMemory   const /* request_line */,
                             bool          const keepalive,
                             Sender      * const mt_nonnull conn_sender,
                             PagePool    * const mt_nonnull page_pool)
{
    ConstMemory const reply_body = "404 Not Found";

    MOMENT_HLS__HEADERS_DATE
    conn_sender->send (
            page_pool,
            true /* do_flush */,
            MOMENT_HLS__404_HEADERS (reply_body.len()),
            "\r\n",
            reply_body);

    if (!keepalive)
        conn_sender->closeAfterFlush ();

    return Result::Success;
}

Ref<HlsServer::StreamSession>
HlsServer::createStreamSession (ConstMemory const stream_name)
{
    mutex.lock ();

    Ref<HlsStream> hls_stream;
    {
        HlsStreamHash::EntryKey const entry = hls_stream_hash.lookup (ConstMemory (stream_name));
        if (!entry) {
          mutex.unlock ();
          return NULL;
        }

        hls_stream = entry.getData();
    }

    Ref<HlsServer::StreamSession> const stream_session = doCreateStreamSession (hls_stream);

    mutex.unlock ();

    return stream_session;
}

MOMENT_HLS__INC

mt_mutex (mutex) Ref<HlsServer::StreamSession>
HlsServer::doCreateStreamSession (HlsStream * const hls_stream)
{
    Ref<StreamSession> const stream_session = grab (new StreamSession (&default_opts));
    stream_session->weak_hls_server = this;
    stream_session->valid = true;
    stream_session->started = false;
    stream_session->last_request_time_millisec = getTimeMilliseconds();
    stream_session->page_pool = page_pool;
    stream_session->hls_stream = hls_stream;

    stream_session->tsmux = tsmux_new ();
    tsmux_set_write_func (stream_session->tsmux, StreamSession::new_packet_cb, &stream_session->new_packet_cb_data);
    TsMuxProgram * const prog = tsmux_program_new (stream_session->tsmux);

    {
        if (default_opts.no_video
            || (default_opts.no_rtmp_video && hls_stream->is_rtmp_stream)
            || hls_stream->no_video)
        {
            logD (hls, _func, "no video");
            stream_session->video_ts = NULL;
        } else {
            stream_session->video_ts = tsmux_create_stream (stream_session->tsmux, TSMUX_ST_VIDEO_H264, TSMUX_PID_AUTO);
            assert (stream_session->video_ts);
            tsmux_program_add_stream (prog, stream_session->video_ts);
        }

        if (default_opts.no_audio
            || (default_opts.no_rtmp_audio && hls_stream->is_rtmp_stream)
            || hls_stream->no_audio)
        {
            logD (hls, _func, "no audio");
            stream_session->audio_ts = NULL;
        } else {
            stream_session->audio_ts = tsmux_create_stream (stream_session->tsmux, TSMUX_ST_AUDIO_AAC, TSMUX_PID_AUTO);
            assert (stream_session->audio_ts);
            tsmux_program_add_stream (prog, stream_session->audio_ts);
        }

        if (stream_session->video_ts)
            tsmux_program_set_pcr_stream (prog, stream_session->video_ts);
        else
        if (stream_session->audio_ts)
            tsmux_program_set_pcr_stream (prog, stream_session->audio_ts);
    }

    if (stream_session->opts.one_session_per_stream) {
        assert (!hls_stream->bound_stream_session);
        logD (hls, _func, "creating bound stream session, hls_stream 0x", fmt_hex, (UintPtr) hls_stream);
        hls_stream->bound_stream_session = stream_session;
    }

// TEST (enable random)
    stream_session->session_id = makeString (randomUint32(), "_", stream_id_counter);
//    stream_session->session_id = makeString (0, "_", stream_id_counter);
    ++stream_id_counter;

    logD (hls, _func, "adding stream session: ", stream_session->session_id);
    stream_sessions.add (stream_session);
    if (!stream_session->opts.one_session_per_stream)
        stream_session_cleanup_list.append (stream_session);

    MOMENT_HLS__INIT

    stream_session->ref ();
    return stream_session;
}

mt_unlocks (mutex) void
HlsServer::markStreamSessionRequest (StreamSession * const mt_nonnull stream_session)
{
    stream_session->last_request_time_millisec = getTimeMilliseconds();

    if (!stream_session->opts.one_session_per_stream) {
        stream_session_cleanup_list.remove (stream_session);
        stream_session_cleanup_list.append (stream_session);
    }

    if (!stream_session->watched_now) {
        Ref<VideoStream> const video_stream =
                stream_session->hls_stream->weak_video_stream.getRef();
        if (video_stream) {
            stream_session->watched_now = true;
            watched_stream_sessions.append (stream_session);
            stream_session->ref ();

            mutex.unlock ();

            video_stream->plusOneWatcher ();
            return;
        }
    } else {
        watched_stream_sessions.remove (stream_session);
        watched_stream_sessions.append (stream_session);
    }

    mutex.unlock ();
}

static void
doMinusOneWatcher (void * const _video_stream)
{
    VideoStream * const video_stream = static_cast <VideoStream*> (_video_stream);
    video_stream->minusOneWatcher ();
}

// Returns 'false' if the session should not be destroyed yet.
mt_mutex (mutex) bool
HlsServer::destroyStreamSession (StreamSession * const mt_nonnull stream_session,
                                 bool            const force_destroy)
{
    logD (hls, _func, "stream_session: 0x", fmt_hex, (UintPtr) stream_session);

    stream_session->hls_stream->mutex.lock ();
    stream_session->mutex.lock ();
    if (!stream_session->valid) {
        stream_session->mutex.unlock ();
        stream_session->hls_stream->mutex.unlock ();
        return true;
    }

    if (!force_destroy
        && !stream_session->waiting_seg_sessions.isEmpty()
        && !stream_session->hls_stream->is_closed)
    {
        stream_session->mutex.unlock ();
        stream_session->hls_stream->mutex.unlock ();
        return false;
    }

    stream_session->valid = false;
    stream_session->hls_stream->bound_stream_session = NULL;

    if (stream_session->started) {
        stream_session->hls_stream->started_stream_sessions.remove (stream_session);
        stream_session->started = false;
    }

    {
        SegmentSessionList::iter iter (stream_session->waiting_seg_sessions);
        while (!stream_session->waiting_seg_sessions.iter_done (iter)) {
            SegmentSession * const seg_session = stream_session->waiting_seg_sessions.iter_next (iter);
            assert (!seg_session->in_forming_list);
            seg_session->conn_sender->close ();
            stream_session->destroySegmentSession (seg_session);
            seg_session->unref ();
        }
        stream_session->waiting_seg_sessions.clear ();
    }

    {
        SegmentSessionList::iter iter (stream_session->forming_seg_sessions);
        while (!stream_session->forming_seg_sessions.iter_done (iter)) {
            SegmentSession * const seg_session = stream_session->forming_seg_sessions.iter_next (iter);
            assert (seg_session->in_forming_list);
            seg_session->conn_sender->close ();
            stream_session->destroySegmentSession (seg_session);
            seg_session->unref ();
        }
        stream_session->forming_seg_sessions.clear ();
    }

    {
        HlsSegmentList::iter iter (stream_session->segment_list);
        while (!stream_session->segment_list.iter_done (iter)) {
            HlsSegment * const segment = stream_session->segment_list.iter_next (iter);
            page_pool->msgUnref (segment->page_list.first);
            segment->unref ();
        }
        stream_session->segment_list.clear ();
        stream_session->num_active_segments = 0;
    }

    bool const tmp_one_per_stream = stream_session->opts.one_session_per_stream;
    bool const tmp_in_release_queue = stream_session->in_release_queue;

    logD (hls, _func, "calling tsmux_free: stream_session: 0x", fmt_hex, (UintPtr) stream_session, ", "
          "tsmux: 0x", (UintPtr) stream_session->tsmux);
    tsmux_free (stream_session->tsmux);

    stream_session->mutex.unlock ();
    stream_session->hls_stream->mutex.unlock ();

    stream_sessions.remove (stream_session);
    if (!tmp_one_per_stream)
        stream_session_cleanup_list.remove (stream_session);
    else
    if (tmp_in_release_queue)
        stream_session_release_queue.remove (stream_session);

    if (stream_session->watched_now) {
        stream_session->watched_now = false;
        watched_stream_sessions.remove (stream_session);
        stream_session->unref ();

        if (Ref<VideoStream> const video_stream = stream_session->hls_stream->weak_video_stream.getRef()) {
            Cb< void (void*) > cb (doMinusOneWatcher,
                                   video_stream,
                                   video_stream);
            cb.call_deferred (&def_reg,
                              doMinusOneWatcher,
                              NULL /* extra_ref_data */);
        }
    }

    stream_session->unref ();

    return true;
}

Result
HlsServer::processStreamHttpRequest (HttpRequest * const mt_nonnull req,
                                     Sender      * const mt_nonnull conn_sender,
                                     ConstMemory   const stream_name)
{
    Ref<StreamSession> stream_session;

    if (!default_opts.one_session_per_stream) {
        stream_session = createStreamSession (stream_name);
        if (!stream_session) {
            logA_ ("hls_stream 404 ", req->getClientAddress(), " ", req->getRequestLine());
            return sendHttpNotFound (req, conn_sender);
        }
    } else {
        Ref<HlsStream> hls_stream;

        mutex.lock ();

        {
            HlsStreamHash::EntryKey const entry = hls_stream_hash.lookup (ConstMemory (stream_name));
            if (!entry) {
                mutex.unlock ();
                logA_ ("hls_stream 404 ", req->getClientAddress(), " ", req->getRequestLine());
                return sendHttpNotFound (req, conn_sender);
            }

            hls_stream = entry.getData();
        }

        if (!hls_stream->bound_stream_session) {
            mutex.unlock ();
            logA_ ("hls_stream 404 (bound) ", req->getClientAddress(), " ", req->getRequestLine());
            return sendHttpNotFound (req, conn_sender);
        }

        stream_session = hls_stream->bound_stream_session;

        mutex.unlock ();
    }

    PagePool::PageListHead page_list;
    page_pool->printToPages (
            &page_list,
            "#EXTM3U\n"
            "#EXT-X-VERSION:3\n"
            "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=300000,CODECS=\"avc1.66.31\",RESOLUTION=320x240\n"
            "data/segments.m3u8?s=", stream_session->session_id->mem(), "\n"
            "#EXT-X-ENDLIST\n");
    Size const data_len = PagePool::countPageListDataLen (page_list.first, 0 /* msg_offset */);

    MOMENT_HLS__HEADERS_DATE
    conn_sender->send (
            page_pool,
            false /* do_flush */,
            MOMENT_HLS__OK_HEADERS (m3u8_mime_type, data_len),
            "\r\n");
    conn_sender->sendPages (page_pool, page_list.first, true /* do_flush */);

    if (!req->getKeepalive())
        conn_sender->closeAfterFlush ();

    logA_ ("hls_stream 200 ", req->getClientAddress(), " ", req->getRequestLine());

    return Result::Success;
}

Result
HlsServer::processSegmentListHttpRequest (HttpRequest * const mt_nonnull req,
                                          Sender      * const mt_nonnull conn_sender,
                                          ConstMemory   const stream_session_id)
{
    mutex.lock ();

    Ref<StreamSession> const stream_session = stream_sessions.lookup (stream_session_id);
    if (!stream_session) {
        mutex.unlock ();
        logD_ (_func, "stream session not found, id ", stream_session_id);
        return sendHttpNotFound (req, conn_sender);
    }

    return mt_unlocks (mutex) doProcessSegmentListHttpRequest (req,
                                                               conn_sender,
                                                               stream_session,
                                                               ConstMemory() /* path_prefix */);
}

mt_unlocks (mutex) Result
HlsServer::doProcessSegmentListHttpRequest (HttpRequest   * const req,
                                            Sender        * const mt_nonnull conn_sender,
                                            StreamSession * const stream_session,
                                            ConstMemory     const path_prefix)
{
    logD (hls_seg, _func, "hls_stream 0x", fmt_hex, (UintPtr) stream_session->hls_stream.ptr());

    mt_unlocks (mutex) markStreamSessionRequest (stream_session);

    stream_session->mutex.lock ();
    if (!stream_session->valid) {
        stream_session->mutex.unlock ();
        logD_ (_func, "stream session invalidated");
        return sendHttpNotFound (req, conn_sender);
    }

    bool just_started = false;
    if (!stream_session->started)
        just_started = true;

    PagePool::PageListHead page_list;
    {
        page_pool->printToPages (
                &page_list,
                "#EXTM3U\n"
                "#EXT-X-VERSION:3\n"
                "#EXT-X-TARGETDURATION:", target_duration_seconds, "\n"
                "#EXT-X-ALLOW-CACHE:NO\n"
                "#EXT-X-MEDIA-SEQUENCE:", stream_session->head_seg_no, "\n");
//                "#EXT-X-PLAYLIST-TYPE:EVENT\n");

        Size num_segments = 0;
        HlsSegmentList::iter iter (stream_session->segment_list);
        while (!stream_session->segment_list.iter_done (iter)) {
            HlsSegment * const segment = stream_session->segment_list.iter_next (iter);
            if (segment->excluded)
                continue;

            page_pool->printToPages (
                    &page_list,
                    "#EXTINF:", extinf_str, "\n",
                    path_prefix, "segment.ts?s=", stream_session->session_id->mem(), "&n=", segment->seg_no, "\n");
            ++num_segments;
        }
        logD (hls_seg, _func, "num_segments: ", num_segments);

//        if (stream_session->forming_segment) {
            for (unsigned i = 0; i < stream_session->opts.num_lead_segments; ++i) {
                page_pool->printToPages (
                        &page_list,
                        "#EXTINF:", extinf_str, "\n",
                        path_prefix, "segment.ts?s=", stream_session->session_id->mem(),
                        "&n=", stream_session->forming_seg_no + i, "\n");
            }
//        }
    }
    stream_session->mutex.unlock ();

    if (just_started) {
        stream_session->hls_stream->mutex.lock ();
        stream_session->mutex.lock ();

        if (!stream_session->started) {
            stream_session->hls_stream->started_stream_sessions.append (stream_session);
            stream_session->started = true;

#if 0
// TODO Set correct timestamp for this extra codec data frame.

            if ((hls_stream->is_rtmp_stream || send_codec_data)
                && stream_session->hls_stream->avc_codec_data_frame)
            {
                // TODO timestamp = 0
                stream_session->newFrameAdded_unlocked (stream_session->hls_stream->avc_codec_data_frame,
                                                        false /* force_finish_segment */);
            }
#endif

            {
              // TEST: Buffer eaters.

                HlsFrame frame;
                frame.is_video_frame = true;
                frame.timestamp_nanosec = 0;
                frame.video_frame_type = VideoStream::VideoFrameType::Unknown;
                frame.random_access = false;
                frame.is_data_frame = false;
                frame.msg_offset = 0;
                frame.page_pool = page_pool;

                {
                  // MPEG-TS access unit delimiter.
                    Byte const delimiter [6] = { 0, 0, 0, 1, 0x09, 0xf0 };
                    page_pool->getFillPages (&frame.page_list, ConstMemory::forObject (delimiter));
                }

                for (unsigned i = 0; i < num_dummy_starters; ++i)
                    stream_session->newFrameAdded_unlocked (&frame, true /* force_finish_segment */);
            }

            stream_session->mutex.unlock ();
            stream_session->hls_stream->mutex.unlock ();
        }
    }

    Size const data_len = PagePool::countPageListDataLen (page_list.first, 0 /* msg_offset */);

    MOMENT_HLS__HEADERS_DATE
    conn_sender->send (
            page_pool,
            false /* do_flush */,
            MOMENT_HLS__OK_HEADERS (m3u8_mime_type, data_len),
            "\r\n");
    conn_sender->sendPages (page_pool, page_list.first, true /* do_flush */);

    if (!req->getKeepalive())
        conn_sender->closeAfterFlush ();

    logD (hls_seg, _func, "segment list sent: ", req->getClientAddress(), " ", req->getRequestLine());

    return Result::Success;
}

Sender::Frontend const HlsServer::StreamSession::sender_event_handler = {
    NULL         /* sendStateChanged */,
    senderClosed /* closed */
};

void
HlsServer::StreamSession::senderClosed (Exception * const /* exc_ */,
                                        void      * const _seg_session)
{
    SegmentSession * const seg_session = static_cast <SegmentSession*> (_seg_session);
    Ref<StreamSession> const stream_session = seg_session->weak_stream_session.getRef ();
    if (!stream_session) {
      // If StreamSession is gone, then this SegmentSession should have been
      // released already.
        return;
    }

    stream_session->mutex.lock ();
    if (!seg_session->valid) {
        stream_session->mutex.unlock ();
        return;
    }
    seg_session->valid = false;

    stream_session->destroySegmentSession (seg_session);

    if (seg_session->in_forming_list)
        stream_session->forming_seg_sessions.remove (seg_session);
    else
        stream_session->waiting_seg_sessions.remove (seg_session);

    stream_session->mutex.unlock ();

    seg_session->unref ();
}

Result
HlsServer::processSegmentHttpRequest (HttpRequest * const mt_nonnull req,
                                      Sender      * const mt_nonnull conn_sender,
                                      ConstMemory   const stream_session_id,
                                      ConstMemory   const seg_no_mem)
{
    Uint64 seg_no;
    if (!strToUint64_safe (seg_no_mem, &seg_no, 10)) {
        logD (hls_seg, _func, "Bad seg no param \"n\": ", seg_no_mem);
        return sendHttpNotFound (req, conn_sender);
    }

    mutex.lock ();

#ifdef MOMENT_HLS_DEMO
    if (send_delimiters) {
        mutex.unlock ();
        return sendHttpNotFound (req, conn_sender);
    }
#endif

    Ref<StreamSession> const stream_session = stream_sessions.lookup (stream_session_id);
    if (!stream_session) {
        mutex.unlock ();
        logD (hls_seg, _func, "stream session not found, id ", stream_session_id);
        return sendHttpNotFound (req, conn_sender);
    }

    logD (hls_seg, _func, "hls_stream 0x", fmt_hex, (UintPtr) stream_session->hls_stream.ptr());

    mt_unlocks (mutex) markStreamSessionRequest (stream_session);

    Ref<SegmentSession> const seg_session = grab (new SegmentSession);
    seg_session->weak_stream_session = stream_session;
    seg_session->valid = true;
    seg_session->in_forming_list = false;

    seg_session->conn_sender = conn_sender;

    seg_session->conn_keepalive = req->getKeepalive();
    seg_session->client_address = req->getClientAddress();
    seg_session->request_line = grab (new String (req->getRequestLine()));

    seg_session->seg_no = seg_no;

    {
      // Subscribing for "sender closed" event.

        conn_sender->lock ();
        if (conn_sender->isClosed_unlocked ()) {
            conn_sender->unlock ();
            logD (hls_seg, _func, "connection gone");
            return Result::Success;
        }

        seg_session->sender_sbn =
                conn_sender->getEventInformer()->subscribe_unlocked (
                        CbDesc<Sender::Frontend> (&StreamSession::sender_event_handler,
                                                  seg_session,
                                                  seg_session));
        conn_sender->unlock ();
    }

    return stream_session->addSegmentSession (seg_session);
}

Result
HlsServer::httpRequest (HttpRequest   * const mt_nonnull req,
                        Sender        * const mt_nonnull conn_sender,
                        Memory const  & /* msg_body */,
                        void         ** const mt_nonnull /* ret_msg_data */,
                        void          * const _self)
{
    HlsServer * const self = static_cast <HlsServer*> (_self);

    logD (hls_seg, _func, "sender 0x", fmt_hex, (UintPtr) conn_sender, " ", req->getRequestLine());

    // TODO It would be more logical to use htp://server/stream/stream.m3u8
    //                                      htp://server/stream/segments.m3u8?s=0
    //                                      htp://server/stream/segment.ts?s=0&n=0

    if (req->getNumPathElems() >= 2
        && equal (req->getPath (req->getNumPathElems() - 2), "data"))
    {
        ConstMemory const ts_ext (".ts");
        ConstMemory const file_name = req->getPath (req->getNumPathElems() - 1);

        if (equal (req->getPath (req->getNumPathElems() - 1), "segments.m3u8")) {
            ConstMemory const stream_session_id = req->getParameter ("s");
            return self->processSegmentListHttpRequest (req, conn_sender, stream_session_id);
        } else
        if (file_name.len() >= ts_ext.len()
            && equal (file_name.region (file_name.len() - ts_ext.len(), ts_ext.len()),
                      ts_ext))
        {
            ConstMemory const stream_session_id = req->getParameter ("s");
            ConstMemory const seg_no_mem = req->getParameter ("n");
            return self->processSegmentHttpRequest (req, conn_sender, stream_session_id, seg_no_mem);
        }
    }

    if (req->getNumPathElems() >= 1) {
        ConstMemory const m3u8_ext (".m3u8");
        ConstMemory const file_name = req->getPath (req->getNumPathElems() - 1);

        if (file_name.len() >= m3u8_ext.len()
            && equal (file_name.region (file_name.len() - m3u8_ext.len(), m3u8_ext.len()),
                      m3u8_ext))
        {
            ConstMemory const stream_name = file_name.region (0, file_name.len() - m3u8_ext.len());

            if (!self->default_opts.one_session_per_stream) {
                return self->processStreamHttpRequest (req, conn_sender, stream_name);
            } else {
                self->mutex.lock ();

                Ref<HlsStream> hls_stream;
                {
                    HlsStreamHash::EntryKey const entry = self->hls_stream_hash.lookup (ConstMemory (stream_name));
                    if (!entry) {
                        self->mutex.unlock ();
                        logA_ ("hls_stream 404 ", req->getClientAddress(), " ", req->getRequestLine(), " (", stream_name, ")");
                        return self->sendHttpNotFound (req, conn_sender);
                    }

                    hls_stream = entry.getData();
                }

                if (!hls_stream->bound_stream_session) {
                    self->mutex.unlock ();
                    logA_ ("hls_stream 404 (bound) ", req->getClientAddress(), " ", req->getRequestLine());
                    return self->sendHttpNotFound (req, conn_sender);
                }

                Ref<StreamSession> const stream_session = hls_stream->bound_stream_session;

                return mt_unlocks (mutex) self->doProcessSegmentListHttpRequest (req,
                                                                                 conn_sender,
                                                                                 stream_session,
                                                                                 "data/");
            }
        }
    }

    logD_ (_func, "bad request: ", req->getRequestLine());
    return self->sendHttpNotFound (req, conn_sender);
}

mt_const void
HlsServer::init (MomentServer  * const mt_nonnull moment,
                 StreamOptions * const opts,
                 bool            const insert_au_delimiters,
                 bool            const send_codec_data,
                 Uint64          const codec_data_interval_millisec,
                 Uint64          const target_duration_seconds,
                 Uint64          const num_dummy_starters,
                 Time            const stream_timeout,
                 Time            const watcher_timeout_millisec,
                 Uint64          const frame_window_seconds)
{
    this->default_opts = *opts;

    this->insert_au_delimiters         = insert_au_delimiters;
    this->send_codec_data              = send_codec_data;
    this->codec_data_interval_millisec = codec_data_interval_millisec,
    this->target_duration_seconds      = target_duration_seconds;
    this->num_dummy_starters           = num_dummy_starters;
    this->stream_timeout               = stream_timeout;
    this->watcher_timeout_millisec     = watcher_timeout_millisec;
    this->frame_window_seconds         = frame_window_seconds;

    def_reg.setDeferredProcessor (moment->getServerApp()->getServerContext()->getMainThreadContext()->getDeferredProcessor());

    if (default_opts.segment_duration_millisec % 1000 == 0) {
        extinf_str = makeString (default_opts.segment_duration_millisec / 1000);
    } else {
        Format fmt;
        fmt.precision = 3;
        extinf_str = makeString (fmt, (double) default_opts.segment_duration_millisec / 1000.0);
        {
          // Stripping trailing zeros.
            unsigned i = 0;
            for (; i < extinf_str->mem().len(); ++i) {
                if (extinf_str->mem().mem() [extinf_str->mem().len() - 1 - i] != '0')
                    break;
            }
            extinf_str = grab (new String (extinf_str->mem().region (0, extinf_str->mem().len() - i)));
        }
    }
// TEST (remove)
//    extinf_str = grab (new String ("0.01"));
    logD_ (_func, "extinf_str: ", extinf_str);

    ServerApp * const server_app = moment->getServerApp();
    HttpService * const http_service = moment->getHttpService();

    timers = server_app->getServerContext()->getMainThreadContext()->getTimers();
    page_pool = moment->getPagePool();

    segments_cleanup_timer =
            timers->addTimer (CbDesc<Timers::TimerCallback> (
                                      segmentsCleanupTimerTick,
                                      this,
                                      this),
                              target_duration_seconds,
                              true /* periodical */);

    stream_session_cleanup_timer =
            timers->addTimer (CbDesc<Timers::TimerCallback> (
                                      streamSessionCleanupTimerTick,
                                      this,
                                      this),
                              target_duration_seconds / 2 > 0 ? target_duration_seconds / 2 : 1,
                              true /* periodical */);

    watched_streams_timer =
            timers->addTimer_microseconds (CbDesc<Timers::TimerCallback> (
                                                   watchedStreamsTimerTick,
                                                   this,
                                                   this),
                                           (watcher_timeout_millisec / 5 > 0 ? watcher_timeout_millisec / 5 : 1) * 1000,
                                           true /* periodical */);

    moment->addVideoStreamHandler (CbDesc<MomentServer::VideoStreamHandler> (
            &video_stream_handler, this, this));

    http_service->addHttpHandler (
            CbDesc<HttpService::HttpHandler> (&http_handler, this, this),
            "hls");

#ifdef MOMENT_HLS_DEMO
    timers->addTimer (CbDesc<Timers::TimerCallback> (
                              watchedStreamsTimerTick_,
                              this,this),
                      3600 * 6,
                      true /* periodical */);
#endif
}

HlsServer::HlsServer ()
    : timers (NULL),
      page_pool (NULL)
#ifdef MOMENT_HLS_DEMO
      , send_delimiters (false)
#endif
{
}

HlsServer::~HlsServer ()
{
    mutex.lock ();

    {
        StreamSessionHash::iter iter (stream_sessions);
        while (!stream_sessions.iter_done (iter)) {
            StreamSession * const stream_session = stream_sessions.iter_next (iter);
            bool const destroyed = destroyStreamSession (stream_session, true /* force_destroy */);
            assert (destroyed);
        }
    }

    mutex.unlock ();
}

static void momentHlsInit ()
{
    logD_ (_func, "Initializing mod_hls");

    CodeDepRef<MomentServer> const moment = MomentServer::getInstance();
    /* TODO CodeDepRef<Cofig> */ MConfig::Config * const config = moment->getConfig ();

    {
	ConstMemory const opt_name = "mod_hls/enable";
	MConfig::BooleanValue const enable = config->getBoolean (opt_name);
	if (enable == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return;
	}

	if (enable == MConfig::Boolean_False) {
	    logI_ (_func, "Apple HTTP Live Streaming module is not enabled. "
		   "Set \"", opt_name, "\" option to \"y\" to enable.");
	    return;
	}
    }

    bool one_session_per_stream = true;
    {
        ConstMemory const opt_name = "mod_hls/one_session_per_stream";
        MConfig::BooleanValue const val = config->getBoolean (opt_name);
        if (val == MConfig::Boolean_Invalid) {
            logE_ (_func, "bad value for ", opt_name, ": ", config->getString (opt_name));
            return;
        }

        if (val == MConfig::Boolean_False)
            one_session_per_stream = false;

        logI_ (_func, opt_name, ": ", one_session_per_stream);
    }

    bool realtime_mode = false;
    {
        ConstMemory const opt_name = "mod_hls/realtime_mode";
        MConfig::BooleanValue const val = config->getBoolean (opt_name);
        if (val == MConfig::Boolean_Invalid) {
            logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
            return;
        }

        if (val == MConfig::Boolean_True)
            realtime_mode = true;

	logI_ (_func, opt_name, ": ", realtime_mode);
    }

    bool no_audio = false;
    {
        ConstMemory const opt_name = "mod_hls/no_audio";
        MConfig::BooleanValue const val = config->getBoolean (opt_name);
        if (val == MConfig::Boolean_Invalid) {
            logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
            return;
        }

        if (val == MConfig::Boolean_True)
            no_audio = true;

        logI_ (_func, opt_name, ": ", no_audio);
    }

    bool no_video = false;
    {
        ConstMemory const opt_name = "mod_hls/no_video";
        MConfig::BooleanValue const val = config->getBoolean (opt_name);
        if (val == MConfig::Boolean_Invalid) {
            logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
            return;
        }

        if (val == MConfig::Boolean_True)
            no_video = true;

        logI_ (_func, opt_name, ": ", no_video);
    }

    bool no_rtmp_audio = false;
    {
        ConstMemory const opt_name = "mod_hls/no_rtmp_audio";
        MConfig::BooleanValue const val = config->getBoolean (opt_name);
        if (val == MConfig::Boolean_Invalid) {
            logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
            return;
        }

        if (val == MConfig::Boolean_True)
            no_rtmp_audio = true;

        logI_ (_func, opt_name, ": ", no_rtmp_audio);
    }

    bool no_rtmp_video = false;
    {
        ConstMemory const opt_name = "mod_hls/no_rtmp_video";
        MConfig::BooleanValue const val = config->getBoolean (opt_name);
        if (val == MConfig::Boolean_Invalid) {
            logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
            return;
        }

        if (val == MConfig::Boolean_True)
            no_rtmp_video = true;

        logI_ (_func, opt_name, ": ", no_rtmp_video);
    }

    bool insert_au_delimiters = false;
    {
        ConstMemory const opt_name = "mod_hls/insert_au_delimiters";
        MConfig::BooleanValue const val = config->getBoolean (opt_name);
        if (val == MConfig::Boolean_Invalid) {
            logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
            return;
        }

        if (val == MConfig::Boolean_True)
            insert_au_delimiters = true;

        logI_ (_func, opt_name, ": ", insert_au_delimiters);
    }

    bool send_codec_data = true;
    {
        ConstMemory const opt_name = "mod_hls/send_codec_data";
        MConfig::BooleanValue const val = config->getBoolean (opt_name);
        if (val == MConfig::Boolean_Invalid) {
            logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
            return;
        }

        if (val == MConfig::Boolean_False)
            send_codec_data = false;

        logI_ (_func, opt_name, ": ", send_codec_data);
    }

    Uint64 codec_data_interval_millisec = 1000;
    {
        ConstMemory const opt_name = "mod_hls/codec_data_interval";
        if (!config->getUint64_default (opt_name, &codec_data_interval_millisec, codec_data_interval_millisec))
            logE_ (_func, "bad value for ", opt_name, ": ", config->getString (opt_name));

        logI_ (_func, opt_name, ": ", codec_data_interval_millisec);
    }

    /* TODO Make some better estimates based on bitrate and target duration. */
    Uint64 realtime_target_len = 500 * 188;
    {
	ConstMemory const opt_name = "mod_hls/realtime_target_len";
	if (!config->getUint64_default (opt_name, &realtime_target_len, realtime_target_len))
	    logE_ (_func, "bad value for ", opt_name);

	logI_ (_func, opt_name, ": ", realtime_target_len);
    }

    Uint64 target_duration = 1;
    {
        ConstMemory const opt_name = "mod_hls/target_duration";
        if (!config->getUint64_default (opt_name, &target_duration, target_duration))
            logE_ (_func, "bad value for ", opt_name);

	logI_ (_func, opt_name, ": ", target_duration);
    }

    Uint64 segment_duration = 1000;
    {
        ConstMemory const opt_name = "mod_hls/segment_duration";
        if (!config->getUint64_default (opt_name, &segment_duration, segment_duration))
            logE_ (_func, "bad value for ", opt_name);

        logI_ (_func, opt_name, ": ", segment_duration);
    }

    Uint64 max_segment_len = 1 << 24 /* 16 MB */;
    {
        ConstMemory const opt_name = "mod_hls/max_segment_len";
        if (!config->getUint64_default (opt_name, &max_segment_len, max_segment_len))
            logE_ (_func, "bad value for ", opt_name);

        logI_ (_func, opt_name, ": ", max_segment_len);
    }

    Uint64 num_dummy_starters = 0;
    {
        ConstMemory const opt_name = "mod_hls/num_dummy_starters";
        if (!config->getUint64_default (opt_name, &num_dummy_starters, num_dummy_starters))
            logE_ (_func, "bad value for ", opt_name);

        logI_ (_func, opt_name, ": ", num_dummy_starters);
    }

    Uint64 num_real_segments = 2;
    {
        ConstMemory const opt_name = "mod_hls/num_real_segments";
        if (!config->getUint64_default (opt_name, &num_real_segments, num_real_segments))
            logE_ (_func, "bad value for ", opt_name);

        logI_ (_func, opt_name, ": ", num_real_segments);
    }

    Uint64 num_lead_segments = 1;
    {
        ConstMemory const opt_name = "mod_hls/num_lead_segments";
        if (!config->getUint64_default (opt_name, &num_lead_segments, num_lead_segments))
            logE_ (_func, "bad value for ", opt_name);

        logI_ (_func, opt_name, ": ", num_lead_segments);
    }

    Uint64 stream_timeout = 60;
    {
        ConstMemory const opt_name = "mod_hls/stream_timeout";
        if (!config->getUint64_default (opt_name, &stream_timeout, stream_timeout))
            logE_ (_func, "bad value for ", opt_name);

        logI_ (_func, opt_name, ": ", stream_timeout);
    }

    Uint64 watcher_timeout_millisec = 5000;
    {
        ConstMemory const opt_name = "mod_hls/watcher_timeout";
        if (!config->getUint64_default (opt_name, &watcher_timeout_millisec, watcher_timeout_millisec))
            logE_ (_func, "bad value for ", opt_name);

        logI_ (_func, opt_name, ": ", watcher_timeout_millisec);
    }

    Uint64 frame_window_seconds = target_duration * 3;
    {
        ConstMemory const opt_name = "mod_hls/frame_window";
        if (!config->getUint64_default (opt_name, &frame_window_seconds, frame_window_seconds))
            logE_ (_func, "bad value for ", opt_name);

        logI_ (_func, opt_name, ": ", frame_window_seconds);
    }

    HlsServer::StreamOptions opts;
    opts.one_session_per_stream    = one_session_per_stream;
    opts.realtime_mode             = realtime_mode;
    opts.no_audio                  = no_audio;
    opts.no_video                  = no_video;
    opts.no_rtmp_audio             = no_rtmp_audio;
    opts.no_rtmp_video             = no_rtmp_video;
    opts.realtime_target_len       = realtime_target_len;
    opts.segment_duration_millisec = segment_duration;
    opts.max_segment_len           = max_segment_len;
    opts.num_real_segments         = num_real_segments;
    opts.num_lead_segments         = num_lead_segments;

    glob_hls_server.init (moment,
                          &opts,
                          insert_au_delimiters,
                          send_codec_data,
                          codec_data_interval_millisec,
                          target_duration,
                          num_dummy_starters,
                          stream_timeout,
                          watcher_timeout_millisec,
                          frame_window_seconds);
}

static void momentHlsUnload ()
{
    logD_ (_func, "Unloading mod_hls");
}


namespace M {

void libMary_moduleInit ()
{
    momentHlsInit ();
}

void libMary_moduleUnload ()
{
    momentHlsUnload ();
}

}

