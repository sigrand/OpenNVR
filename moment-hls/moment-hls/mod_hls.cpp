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
#include <moment/moment_request_handler.h>

#include <moment-hls/inc.h>
#include <moment-hls/ffmpeg_ts_muxer.h>

// aditional includes
#include <sstream>
#include <fstream>      // std::ifstream

#ifndef LIBMARY_MT_SAFE
	#error Please check the build options else you can catch unexpected errors (in this code is used StateMutexLock)
#endif

#if defined(LIBMARY_PLATFORM_WIN32)
	#define _strcasecmp stricmp
#else
	#define _strcasecmp strcasecmp
#endif

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
static LogGroup libMary_logGroup_hls_msg    ("mod_hls.msg",    LogLevel::D);
static LogGroup libMary_logGroup_hls_timers ("mod_hls.timers", LogLevel::D);

static ConstMemory const m3u8_mime_type   = "application/vnd.apple.mpegurl";
static ConstMemory const mpegts_mime_type = "video/mp2t";

static std::string _m3u8_mime_type = "application/vnd.apple.mpegurl";
static std::string _mpegts_mime_type = "video/mp2t";


class HandlerContext
{
public:

    HandlerContext(HTTPServerRequest & req, HTTPServerResponse & resp)
		: m_value(false)
		, m_request(req)
		, m_response(resp)
    {
		initMutexCond();
    }

	~HandlerContext()
	{
		destroyMutexCond();
	}

	void waitForSignal()
	{
		pthread_mutex_lock(&m_mutex);
		while(!m_value)
		{
			pthread_cond_wait(&m_cond, &m_mutex);
		}
		pthread_mutex_unlock(&m_mutex);
	}

	void doSignal()
	{
		pthread_mutex_lock(&m_mutex);
		m_value = true;
		pthread_cond_signal(&m_cond);
		pthread_mutex_unlock(&m_mutex);
	}

	Result sendHttpNotFound()
	{
		logD(hls_seg, _func_, "!!!");
		static const std::string not_found = "404 Not Found";
		m_response.setStatus(HTTPResponse::HTTP_NOT_FOUND);
		m_response.setContentLength(not_found.length());
		std::ostream& out = m_response.send();
		out << not_found;
		out.flush();

		return Result::Success;
	}

	Result sendHttpNotFoundAndDoSignal()
	{
		Result res = sendHttpNotFound();
		doSignal();
		return res;
	}

	HTTPServerRequest & Request() const { return m_request; }
	HTTPServerResponse & Response() const { return m_response; }

private:

	void initMutexCond()
	{
		pthread_mutex_init(&m_mutex, NULL);
		pthread_cond_init(&m_cond, NULL);
	}

	void destroyMutexCond()
	{
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_cond);
	}

    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
    volatile bool m_value;
    HTTPServerRequest & m_request;
    HTTPServerResponse & m_response;
};


namespace MomentHls
{

class HlsServer : public Object
{
private:
    StateMutex mutex_main;

	static Referenced m_refsHlsSegment;
	static Referenced m_refsSegmentSession;
	static Referenced m_refsStreamSession;
	static Referenced m_refsHlsStream;

    class HlsSegment : public Referenced,
                       public IntrusiveListElement<>
    {
    public:
        Uint64 seg_no;

        // Excluded from playlist? (i.e. scheduled for deletion)
        bool excluded;
        Uint64 excluded_time;

		std::vector<Uint8>	segment_data;

        HlsSegment ()
            : seg_no(0)
			, excluded (false)
            , excluded_time (0)
		{
			logD (hls_msg, _this_func_, "refs = ", m_refsHlsSegment.getRefCount() - 1);
			m_refsHlsSegment.ref();
		}

        ~HlsSegment ()
        {
			m_refsHlsSegment.unref();
            logD (hls_msg, _this_func_, "refs = ", m_refsHlsSegment.getRefCount() - 1, ", seg_no = ", seg_no , ", data size = ", segment_data.size());
			segment_data.clear();	// JIC
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

        HandlerContext &     handler_ctx;
        mt_const bool        conn_keepalive;
        mt_const IpAddress   client_address;
        mt_const Ref<String> request_line;

        mt_const Uint64 seg_no;

        mt_mutex (StreamSession::mutex_ss) bool in_forming_list;

        mt_mutex (StreamSession::mutex_ss) bool valid;

        SegmentSession ( HandlerContext & ctx)
            : handler_ctx(ctx)
		{
			logD (hls_msg, _this_func_, "refs = ", m_refsSegmentSession.getRefCount() - 1);
			m_refsSegmentSession.ref();
		}

        ~SegmentSession ()
		{
			m_refsSegmentSession.unref();
			logD (hls_msg, _this_func_, "refs = ", m_refsSegmentSession.getRefCount() - 1);
        }
    };

    typedef IntrusiveList <SegmentSession, SegmentSessionList_name> SegmentSessionList;

    class HlsStream;

    class WatchedStreamSessionList_name;
    class StartedStreamSessionList_name;
    class StreamSessionCleanupList_name;
    class StreamSessionReleaseQueue_name;

public:
    class StreamOptions
    {
    public:
        mt_const bool one_session_per_stream;
        //mt_const bool realtime_mode;
        mt_const Uint64 segment_duration_seconds;
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
        StateMutex mutex_ss;

        mt_const WeakRef<HlsServer> weak_hls_server;

        mt_mutex (HlsServer::mutex_main) bool in_release_queue;
        mt_mutex (HlsServer::mutex_main) Time release_time;

        mt_mutex (HlsServer::mutex_main) bool watched_now;
        mt_mutex (HlsServer::mutex_main) Uint64 last_request_time_millisec;

        mt_const Ref<String> session_id;

        mt_const Ref<HlsStream> hls_stream;
        mt_const DataDepRef<PagePool> page_pool;

        mt_mutex (mutex_ss) bool valid;

        // If 'true', then the session in on 'hls_stream->started_stream_sessions' list.
        mt_mutex (mutex_ss) bool started;

        // A session can be either in 'forming_seg_sessions' list or
        // in 'waiting_seg_sessions' list, never in both lists at the same time.
        SegmentSessionList forming_seg_sessions;
        SegmentSessionList waiting_seg_sessions;

        Result addSegmentSession (SegmentSession * mt_nonnull seg_session);

        mt_mutex (mutex_ss) void destroySegmentSession (SegmentSession * mt_nonnull seg_session);

		// we put ref of hlsSegment and current num_active_segments from hls_stream to avoid a lock mutex_objs
        mt_mutex (mutex_ss) void finishSegment (const Ref<HlsSegment> & hlsSegment, Size hls_stream_num_active_segments);

      mt_iface (Sender::Frontend)
        static Sender::Frontend const sender_event_handler;

        static void senderClosed (Exception *exc_,
                                  void      *_seg_session);
      mt_iface_end

        StreamSession ()
            : in_release_queue     (false),
              release_time         (0),
              watched_now          (false),
              last_request_time_millisec (0),
              page_pool            (this /* coderef_container */)
        {
        	logD (hls_msg, _this_func_, "refs = ", m_refsStreamSession.getRefCount() - 1);
        	m_refsStreamSession.ref();
        }

        ~StreamSession ()
		{
        	m_refsStreamSession.unref();
        	logD (hls_msg, _this_func_, "refs = ", m_refsStreamSession.getRefCount() - 1);
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
		StateMutex mutex_objs;	// this mutex for main objects in class except of ffmpeg_ts_muxer.
								// to allow another objects to work with HlsStream when it prepares next HlsSegment.
		
		StateMutex mutex_tsmux;	// this mutex for ffmpeg_ts_muxer

		mt_const StreamOptions opts;

        mt_const HlsServer *hls_server;
        mt_mutex (HlsServer::mutex_main) StreamSession *bound_stream_session;

        WeakRef<VideoStream> weak_video_stream;

		mt_mutex (mutex_tsmux) CTsMuxer * ffmpeg_ts_muxer;

        mt_const GenericStringHash::EntryKey hash_key;

        mt_mutex (HlsServer::mutex_main) GenericInformer::SubscriptionKey stream_sbn;

        mt_mutex (HlsServer::mutex_main) bool is_closed;
        mt_mutex (HlsServer::mutex_main) Time closed_time;

        mt_mutex (mutex_objs) StartedStreamSessionList started_stream_sessions;

		// each HLSStream stores all data in segment_list
		mt_mutex (mutex_objs) Uint64 oldest_seg_no;
		mt_mutex (mutex_objs) Uint64 newest_seg_no;
		// First segment to appear in the playlist.
		mt_mutex (mutex_objs) Uint64 head_seg_no;

		mt_mutex (mutex_objs) HlsSegmentList segment_list;
		mt_mutex (mutex_objs) Size num_active_segments;

		mt_mutex (mutex_objs) void trimSegmentList (Time cur_time);

		mt_mutex (mutex_objs) void addVideoData(VideoStream::VideoMessage * const mt_nonnull msg);

		mt_mutex (mutex_objs) void addAudioData(VideoStream::AudioMessage * const mt_nonnull msg);

        HlsStream (StreamOptions * const mt_nonnull pOpts)
            : opts					(*pOpts),
			  bound_stream_session	(NULL),
			  ffmpeg_ts_muxer		(NULL),
              is_closed				(false),
			  closed_time			(0),
			  oldest_seg_no			(0),
			  newest_seg_no			(0),
			  head_seg_no			(0),
			  num_active_segments	(0)
		{
			logD (hls_msg, _this_func_, "refs = ", m_refsHlsStream.getRefCount() - 1);
			m_refsHlsStream.ref();
		}

        ~HlsStream ()
		{
			m_refsHlsStream.unref();
			logD (hls_msg, _this_func_, "refs = ", m_refsHlsStream.getRefCount() - 1);
        }
    };

    typedef IntrusiveList<HlsStream, StreamDeletionQueue_name> StreamDeletionQueue;
    typedef StringHash< Ref<HlsStream> > HlsStreamHash;

    mt_const StreamOptions default_opts;

    mt_const Time   stream_timeout;
    mt_const Time   watcher_timeout_millisec;

    mt_const Ref<String> extinf_str;

    mt_const Timers *timers;
    mt_const PagePool *page_pool;

    DeferredProcessor::Registration def_reg;

    // TODO These are unnecessary.
    mt_const Timers::TimerKey segments_cleanup_timer;
    mt_const Timers::TimerKey stream_session_cleanup_timer;
    mt_const Timers::TimerKey watched_streams_timer;

    MOMENT_HLS__DATA

    mt_mutex (mutex_main) Uint64 stream_id_counter;

    mt_mutex (mutex_main) HlsStreamHash hls_stream_hash;

    mt_mutex (mutex_main) StreamSessionHash stream_sessions;
    mt_mutex (mutex_main) WatchedStreamSessionList watched_stream_sessions;
    mt_mutex (mutex_main) StreamSessionCleanupList stream_session_cleanup_list;
    mt_mutex (mutex_main) StreamSessionReleaseQueue stream_session_release_queue;

    static void segmentsCleanupTimerTick (void *_self);

    static void streamSessionCleanupTimerTick (void *_self);

    static void watchedStreamsTimerTick (void *_self);

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

    Ref<StreamSession> createStreamSession (ConstMemory stream_name);

    mt_unlocks (mutex_main) void markStreamSessionRequest (StreamSession * mt_nonnull stream_session);

    mt_mutex (mutex_main) Ref<HlsServer::StreamSession> doCreateStreamSession (HlsStream *hls_stream);

    mt_mutex (mutex_main) bool destroyStreamSession (StreamSession * mt_nonnull stream_session,
                                                bool           force_destroy);

    Result processStreamHttpRequest (HandlerContext & ctx,
                                      const std::string & stream_name);

    Result processSegmentListHttpRequest (HandlerContext & ctx,
                                           const std::string & stream_session_id);

    mt_unlocks (mutex_main) Result doProcessSegmentListHttpRequest (HandlerContext & ctx,
                                                                StreamSession *stream_session,
                                                                const std::string & path_prefix);

    Result processSegmentHttpRequest (HandlerContext & ctx,
                                       const std::string & stream_session_id,
                                       const std::string & seg_no_mem);

    static bool httpRequest(HTTPServerRequest &req, HTTPServerResponse &resp, void * _self);

    static Time localGetTimeMilliseconds();

public:
    mt_const void init (MomentServer  * mt_nonnull moment,
                        StreamOptions * mt_nonnull opts,
                        Time           stream_timeout,
                        Time           watcher_timeout_millisec);

     HlsServer ();
    ~HlsServer ();
};

static HlsServer glob_hls_server;

Time
HlsServer::localGetTimeMilliseconds()
{
    struct timespec ts;
    /*int const res = */clock_gettime (CLOCK_MONOTONIC, &ts);
    Time const new_microseconds = (Uint64) ts.tv_sec * 1000000 + (Uint64) ts.tv_nsec / 1000;
    return new_microseconds / 1000;
}

mt_mutex (mutex_objs) void
HlsServer::HlsStream::trimSegmentList (Time cur_time)
{
	StateMutexLock lock(&mutex_objs);

	HlsSegmentList::iter iter (segment_list);
	while (!segment_list.iter_done (iter))
	{
		HlsSegment * const segment = segment_list.iter_next (iter);
		if (!segment->excluded)
			break;

		if (segment->excluded_time < cur_time
			&& cur_time - segment->excluded_time >
			// TODO Calculate this once and remember
			opts.segment_duration_seconds *
			(opts.num_real_segments + opts.num_lead_segments + 1))
		{
			logD (hls_seg, _this_func, "removing segment ", oldest_seg_no, ", seg_no = ", segment->seg_no, ", ref_cnt = ", segment->getRefCount());
			segment_list.remove (segment);
			segment->unref();
			++oldest_seg_no;
		}
	}
}

mt_mutex (mutex_objs) void
HlsServer::HlsStream::addAudioData(VideoStream::AudioMessage * const mt_nonnull msg)
{
	// nothing now
}

mt_mutex (mutex_objs) void
HlsServer::HlsStream::addVideoData(VideoStream::VideoMessage * const mt_nonnull msg)
{
	Ref<HlsSegment> ref_hls_segment;

	{
		StateMutexLock lock(&mutex_tsmux);

		if(!ffmpeg_ts_muxer)
		{
			logE (hls_msg, _func, "ffmpeg_ts_muxer is NULL!!!");
			return;
		}

		ConstMemory muxedData;
		CTsMuxer::Result res = ffmpeg_ts_muxer->WriteVideoMessage(msg, muxedData);

		if(res != CTsMuxer::Success)
		{
			if(res != CTsMuxer::NeedMoreData)
			{
				logE (hls_msg, _func, "ffmpeg_ts_muxer->WriteVideoMessage failed!");
			}

			return;
		}
		// full segment was prepared
		assert(muxedData.len() > 0);
		if(muxedData.len() == 0 || !muxedData.mem())
		{
			logE (hls_msg, _func, "wrong muxedData, len = ", muxedData.len(), ", ptr = ", reinterpret_cast<UintPtr>(muxedData.mem()));
			return;
		}

		HlsSegment * hlsSegment = new (std::nothrow) HlsSegment;	// 1 ref on creating

		if(!hlsSegment)
		{
			logE (hls_msg, _func, "out of memory, hls segment");
			return;
		}

		try
		{
			hlsSegment->segment_data.resize(muxedData.len());
		}
		catch(std::bad_alloc&)
		{
			logE (hls_msg, _func, "out of memory, hlsSegment->segment_data");
		}

		if(hlsSegment->segment_data.size() != muxedData.len())
		{
			delete hlsSegment;
			return;
		}

		memcpy(&hlsSegment->segment_data[0], muxedData.mem(), muxedData.len());

		ref_hls_segment = hlsSegment;
	}

	StartedStreamSessionList unlocked_started_stream_sessions;
	Size hls_stream_num_active_segments;

	{
		// add new segment to list
		StateMutexLock lock(&mutex_objs);

		ref_hls_segment->seg_no = num_active_segments;

		segment_list.append(ref_hls_segment);
		newest_seg_no = num_active_segments;
		++num_active_segments;
		logD (hls_msg, _func, "new hlsSegment with seg_no = ", ref_hls_segment->seg_no, ", ref_cnt = ", ref_hls_segment->getRefCount(), ", num_active = ", num_active_segments);

		if (num_active_segments > opts.num_real_segments)
		{
			HlsSegment *segment_to_exclude = NULL;
			HlsSegmentList::rev_iter iter (segment_list);
			while (!segment_list.rev_iter_done (iter))
			{
				HlsSegment * const segment = segment_list.rev_iter_next (iter);
				if (!segment->excluded)
					segment_to_exclude = segment;
				else
					break;
			}
			assert (segment_to_exclude);
			if(segment_to_exclude)
			{
				logD (hls_msg, _func, "excluded hlsSegment with seg_no = ", segment_to_exclude->seg_no);

				segment_to_exclude->excluded = true;
				segment_to_exclude->excluded_time = getTime();

				++head_seg_no;
			}
			else
			{
				logE (hls_msg, _func, "segment_to_exclude is NULL");
			}
		}

		// copy list started_stream_sessions to unlocked_started_stream_sessions.
		// we will send finishSegment() without lock with HlsStream->mutex_objs
		unlocked_started_stream_sessions = started_stream_sessions;
		hls_stream_num_active_segments = num_active_segments;
	}

	StartedStreamSessionList::iter iter (unlocked_started_stream_sessions);
	while (!unlocked_started_stream_sessions.iter_done (iter))
	{
		StreamSession * const stream_session = unlocked_started_stream_sessions.iter_next (iter);
		stream_session->finishSegment (ref_hls_segment, hls_stream_num_active_segments);
	}
}

void
HlsServer::segmentsCleanupTimerTick (void * const _self)
{
    HlsServer * const self = static_cast <HlsServer*> (_self);
    Time const cur_time = getTime();

	StateMutexLock lock(&self->mutex_main);

    HlsStreamHash::iter iter (self->hls_stream_hash);
    while (!self->hls_stream_hash.iter_done (iter))
	{
        HlsStream * hls_stream = self->hls_stream_hash.iter_next (iter).getData();
        hls_stream->trimSegmentList (cur_time);
    }
}

void
HlsServer::streamSessionCleanupTimerTick (void * const _self)
{
    HlsServer * const self = static_cast <HlsServer*> (_self);

    //logD (hls_timers, _self_func);

    Time const cur_time = getTime();
    Time const cur_time_millisec = getTimeMilliseconds();

	StateMutexLock lock(&self->mutex_main);

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
}

void
HlsServer::watchedStreamsTimerTick (void * const _self)
{
    HlsServer * const self = static_cast <HlsServer*> (_self);

    //logD (hls_timers, _self_func, "before lock");

    Time const cur_time_millisec = getTimeMilliseconds ();

    self->mutex_main.lock ();

    while (!self->watched_stream_sessions.isEmpty()) {
        StreamSession * const stream_session = self->watched_stream_sessions.getFirst();
        //logD (hls_timers, _func_, "last_request_time_millisec=",stream_session->last_request_time_millisec);
        //logD (hls_timers, _func_, "cur_time_millisec=",cur_time_millisec);
        //logD (hls_timers, _func_, "last_request_time_millisec=",stream_session->last_request_time_millisec);
        //logD (hls_timers, _func_, "watcher_timeout_millisec=",self->watcher_timeout_millisec);
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
            self->mutex_main.unlock ();
            logD (hls, _func_, "video_stream->minusOneWatcher");
            video_stream->minusOneWatcher ();
            self->mutex_main.lock ();
        }
    }

    self->mutex_main.unlock ();
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
	// TODO: need to check the frame type of msg
	HlsStream * const hls_stream = static_cast <HlsStream*> (_hls_stream);

	hls_stream->addAudioData(msg);
}

void
HlsServer::videoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
                         void                      * const _hls_stream)
{
    if (!msg->frame_type.isVideoData()
        && msg->frame_type != VideoStream::VideoFrameType::AvcSequenceHeader
        /* && msg->frame_type != VideoStream::VideoFrameType::AvcEndOfSequence */)
    {
        logD (hls_msg, _func, "ignoring frame by type, ", msg->frame_type);
        return;
    }

    HlsStream * const hls_stream = static_cast <HlsStream*> (_hls_stream);

	hls_stream->addVideoData(msg);
}

void
HlsServer::videoStreamClosed (void * const _hls_stream)
{
    HlsStream * const hls_stream = static_cast <HlsStream*> (_hls_stream);
    HlsServer * const self = hls_stream->hls_server;

    logD (hls, _func_);

	StateMutexLock lock_main(&self->mutex_main);

    if (hls_stream->is_closed) {
        logD (hls, _func, "stream 0x", fmt_hex, (UintPtr) hls_stream, " already closed\n");
        return;
    }
    hls_stream->is_closed = true;
    hls_stream->closed_time = getTime();

	{
		StateMutexLock lock(&hls_stream->mutex_tsmux);

		if(hls_stream->ffmpeg_ts_muxer)
		{
			delete hls_stream->ffmpeg_ts_muxer;
			hls_stream->ffmpeg_ts_muxer = NULL;
		}
	}

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

	{
		HlsSegmentList::iter iter (hls_stream->segment_list);
		while (!hls_stream->segment_list.iter_done (iter)) {
			HlsSegment * const segment = hls_stream->segment_list.iter_next (iter);
			segment->unref ();
		}
		hls_stream->segment_list.clear ();
		hls_stream->num_active_segments = 0;
	}

    self->hls_stream_hash.remove (hls_stream->hash_key);
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

MomentServer::VideoStreamHandler const HlsServer::video_stream_handler =
{
    videoStreamAdded
};

void
HlsServer::videoStreamAdded (VideoStream * const mt_nonnull video_stream,
                             ConstMemory   const stream_name,
                             void        * const _self)
{
    HlsServer * const self = static_cast <HlsServer*> (_self);

    logD (hls, _func, "stream_name: ", stream_name);

	{
		StateMutexLock lock_main(&self->mutex_main);
		HlsStreamHash::EntryKey const entry = self->hls_stream_hash.lookup (ConstMemory (stream_name));

		if(entry)
		{
			logE (hls, _func, "stream with name: [", stream_name, "] was added before!!!");
			return;
		}
	}

	{
		// check if new stream contains 'low' on the end of name, it is necessary to detect 
		bool bNameValid = false;

		if(stream_name.len() >= 3)
		{
			const char * pszStartLow = reinterpret_cast<const char *>(stream_name.mem() + stream_name.len() - 3);

			if(_strcasecmp(pszStartLow, "low") == 0)
			{
				bNameValid = true;
				logA (hls, _func, "stream with name: [", stream_name, "] is a 'low' stream");
			}
		}

		if(!bNameValid)
		{
			logA (hls, _func, "stream with name: [", stream_name, "] is not a 'low' stream");
			return;
		}
	}

    Ref<HlsStream> const hls_stream = grab (new HlsStream(&self->default_opts));
    logD (hls, _func, "new HlsStream: 0x", fmt_hex, (UintPtr) hls_stream.ptr());

    hls_stream->hls_server = self;
    hls_stream->weak_video_stream = video_stream;

	hls_stream->ffmpeg_ts_muxer = new CTsMuxer();
	hls_stream->ffmpeg_ts_muxer->Init(self->default_opts.segment_duration_seconds,
		// TODO: calculate using info: bits per second
		10 * 1024 * 1024);	// 10 Mb for one of segment is more than enough

    self->mutex_main.lock ();
    hls_stream->hash_key = self->hls_stream_hash.add (stream_name, hls_stream);

    if (self->default_opts.one_session_per_stream) {
        Ref<StreamSession> const stream_session = self->doCreateStreamSession (hls_stream);
        assert (stream_session);
        self->mutex_main.unlock ();

		StateMutexLock lock_hls_stream(&stream_session->hls_stream->mutex_objs);

		{
			StateMutexLock lock_stream_session(&stream_session->mutex_ss);

			stream_session->started = true;
			stream_session->hls_stream->started_stream_sessions.append (stream_session);
		}
    } else {
        self->mutex_main.unlock ();
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

mt_mutex (mutex_ss) void
HlsServer::StreamSession::finishSegment (const Ref<HlsSegment> & hlsSegment, Size hls_stream_num_active_segments)
{
	logD(hls_seg, _func_, "begin");
	StateMutexLock lock_stream_session(&mutex_ss);

	if(!valid)
	{
		logE(hls_seg, _func_, "exit, !valid");
		return;
	}

	{
        SegmentSessionList::iter iter (forming_seg_sessions);
		logD(hls_seg, _func_, "hlsSegment seg_no = ", hlsSegment->seg_no, ", size = ", hlsSegment->segment_data.size());
        while (!forming_seg_sessions.iter_done (iter))
		{
            SegmentSession * const seg_session = forming_seg_sessions.iter_next (iter);

//             if (opts.realtime_mode)
// 			{
// 				// realtime mode is not supported
//             }
//             else
            {
				const std::vector<Uint8> & segment_data = hlsSegment->segment_data;
                HandlerContext & ctx = seg_session->handler_ctx;
				HTTPServerResponse & response = ctx.Response();

				response.setContentLength(segment_data.size());
				response.setStatus(HTTPResponse::HTTP_OK);
				response.setContentType(_mpegts_mime_type);
				response.sendBuffer(&segment_data[0], segment_data.size());

                ctx.doSignal();
            }

            destroySegmentSession (seg_session);

            forming_seg_sessions.remove (seg_session);
            seg_session->unref ();

            logD(hls_seg, _func_, "segment sent: ", hlsSegment->segment_data.size(), " bytes, ",
                  seg_session->client_address, " ", seg_session->request_line);
        }
        assert (forming_seg_sessions.isEmpty());
    }

    {
      // TODO O(N), ineffective.
        SegmentSessionList::iter iter (waiting_seg_sessions);
        while (!waiting_seg_sessions.iter_done (iter))
		{
            SegmentSession * const seg_session = waiting_seg_sessions.iter_next (iter);
            if (seg_session->seg_no <= hls_stream_num_active_segments)
			{
				waiting_seg_sessions.remove (seg_session);
				logD(hls_seg, _func_, "moved SegmentSession from waiting to forming, seg_no = ", seg_session->seg_no);
                forming_seg_sessions.append (seg_session);
                seg_session->in_forming_list = true;
            }
        }
    }
    logD(hls_seg, _func_, "end");
}

Result
HlsServer::StreamSession::addSegmentSession (SegmentSession * const mt_nonnull seg_session)
{
	StateMutexLock lock_stream_session(&mutex_ss);
	Uint64 num_active_segments;

    if (!valid)
	{
		lock_stream_session.unlock();
        logD(hls_seg, _func_, "stream session gone");
        Result const res = seg_session->handler_ctx.sendHttpNotFoundAndDoSignal();
        destroySegmentSession (seg_session);
        return res;
    }

	{
		Ref<HlsSegment> hlsSegment;

		{
			hls_stream->mutex_objs.lock();

			// If the segment is too old or if it has not been advertised yet, reply 404.
			if (seg_session->seg_no < hls_stream->oldest_seg_no
				|| seg_session->seg_no > hls_stream->newest_seg_no + hls_stream->opts.num_lead_segments)
			{
				logD(hls_seg, _func_, "seg_no ", seg_session->seg_no, " not in segment window "
					"[", hls_stream->oldest_seg_no, ", ", hls_stream->newest_seg_no + hls_stream->opts.num_lead_segments, "]");

				hls_stream->mutex_objs.unlock();
				lock_stream_session.unlock();

				Result const res = seg_session->handler_ctx.sendHttpNotFoundAndDoSignal();
				destroySegmentSession (seg_session);
				return res;
			}

			HlsSegmentList & segment_list = hls_stream->segment_list;

			if (!segment_list.isEmpty() && seg_session->seg_no <= hls_stream->newest_seg_no)
			{
				logD(hls_seg, _func_, "If the segment has already been formed, send it in reply.");
				HlsSegment *segment = NULL;
				{
					HlsSegmentList::iter iter (segment_list);
					while (!segment_list.iter_done (iter)) {
						segment = segment_list.iter_next (iter);
						if (segment->seg_no >= seg_session->seg_no)
							break;
					}
					if (!segment) {
						logD(hls_seg, _func_, "MISSING SEGMENT: seg_no: ", seg_session->seg_no);
					}
					assert (segment);
				}

				hlsSegment = segment;
			}

			num_active_segments = hls_stream->num_active_segments;
			hls_stream->mutex_objs.unlock();
		}

		if(!hlsSegment.isNull())
		{
			lock_stream_session.unlock();
			logD(hls_seg, _func_, "the segment has already been formed, send it in reply");

			// If the segment has already been formed, send it in reply.
			HandlerContext & ctx = seg_session->handler_ctx;
			HTTPServerResponse & response = ctx.Response();
			const std::vector<Uint8> & segment_data = hlsSegment->segment_data;

			response.setStatus(HTTPResponse::HTTP_OK);
			response.setContentType(_mpegts_mime_type);
			response.setContentLength(segment_data.size());
			response.sendBuffer(&segment_data[0], segment_data.size());

			ctx.doSignal();

			destroySegmentSession (seg_session);

			logD(hls_seg, _func_, "segment sent: ", segment_data.size(), " bytes, ",
				seg_session->client_address, " ", seg_session->request_line);

			return Result::Success;
		}
	}

//     if (opts.realtime_mode)
// 	{
// 		// realtime_mode is not supported now
//         HandlerContext * ctx = seg_session->pHandlerCtx;
//         HTTPServerResponse* pResponse = ctx->pResponse;
// 
//         pResponse->setStatus(HTTPResponse::HTTP_OK);
//         pResponse->setContentType(_mpegts_mime_type);
//         pResponse->setContentLength(opts.realtime_target_len);
// 
//         pResponse->send();
// 
//         doSignal(*ctx);
//     }

    if (num_active_segments < seg_session->seg_no)
    {
        logD(hls_seg, _func_, "num_active_segments < seg_session->seg_no(", seg_session->seg_no, "), moved to waiting_seg_sessions list");
        seg_session->in_forming_list = false;
        waiting_seg_sessions.append (seg_session);
        seg_session->ref ();

        return Result::Success;
    }

//     if (opts.realtime_mode)
// 	{
// 		// send data from hls segment that is created at current time
// 		// IT IS NOT SUPPORTED YET
//     }

    seg_session->in_forming_list = true;
    forming_seg_sessions.append (seg_session);
    seg_session->ref ();

    logD(hls_seg, _func_, "end, added seg_session to forming_seg_sessions list with seg_no = ", seg_session->seg_no);

    return Result::Success;
}

mt_mutex (mutex_ss) void
HlsServer::StreamSession::destroySegmentSession (SegmentSession * const mt_nonnull seg_session)
{
    if (!seg_session->valid) {
        return;
    }
    seg_session->valid = false;
}

Ref<HlsServer::StreamSession>
HlsServer::createStreamSession (ConstMemory const stream_name)
{
	StateMutexLock lock_main(&mutex_main);

    logD(hls_seg, _func_, "Create New Stream Session");

    Ref<HlsStream> hls_stream;
    {
        HlsStreamHash::EntryKey const entry = hls_stream_hash.lookup (ConstMemory (stream_name));
        if (!entry) {
          return NULL;
        }

        hls_stream = entry.getData();
    }

    return doCreateStreamSession (hls_stream);
}

MOMENT_HLS__INC

mt_mutex (mutex_main) Ref<HlsServer::StreamSession>
HlsServer::doCreateStreamSession (HlsStream * const hls_stream)
{
    logD(hls_seg, _func_, "New Stream Session");

    Ref<StreamSession> const stream_session = grab (new StreamSession ());
    stream_session->weak_hls_server = this;
    stream_session->valid = true;
    stream_session->started = false;
    stream_session->last_request_time_millisec = getTimeMilliseconds();
    stream_session->page_pool = page_pool;
	stream_session->hls_stream = hls_stream;

    if (hls_stream->opts.one_session_per_stream) {
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
    if (!hls_stream->opts.one_session_per_stream)
        stream_session_cleanup_list.append (stream_session);

    MOMENT_HLS__INIT

    stream_session->ref ();
    return stream_session;
}

mt_unlocks (mutex_main) void
HlsServer::markStreamSessionRequest (StreamSession * const mt_nonnull stream_session)
{
    logD (hls, _func_);
    stream_session->last_request_time_millisec = localGetTimeMilliseconds();
    logD (hls, _func_, "last_request_time_millisec = ", stream_session->last_request_time_millisec);

    if (!stream_session->hls_stream->opts.one_session_per_stream) {
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

            mutex_main.unlock ();
            logD (hls, _func_, "video_stream->plusOneWatcher");
            video_stream->plusOneWatcher ();
            return;
        }
    } else {
        watched_stream_sessions.remove (stream_session);
        watched_stream_sessions.append (stream_session);
    }

    mutex_main.unlock ();
}

static void
doMinusOneWatcher (void * const _video_stream)
{
    VideoStream * const video_stream = static_cast <VideoStream*> (_video_stream);
    logD (hls, _func_, "video_stream->minusOneWatcher");
    video_stream->minusOneWatcher ();
}

// Returns 'false' if the session should not be destroyed yet.
mt_mutex (mutex_main) bool
HlsServer::destroyStreamSession (StreamSession * const mt_nonnull stream_session,
                                 bool            const force_destroy)
{
    logD (hls, _func, "stream_session: 0x", fmt_hex, (UintPtr) stream_session);

    stream_session->hls_stream->mutex_objs.lock ();
    stream_session->mutex_ss.lock ();
    if (!stream_session->valid) {
        stream_session->mutex_ss.unlock ();
        stream_session->hls_stream->mutex_objs.unlock ();
        return true;
    }

    if (!force_destroy
        && !stream_session->waiting_seg_sessions.isEmpty()
        && !stream_session->hls_stream->is_closed)
    {
        stream_session->mutex_ss.unlock ();
        stream_session->hls_stream->mutex_objs.unlock ();
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

            StreamSession::senderClosed(NULL, seg_session);

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

            StreamSession::senderClosed(NULL, seg_session);

            stream_session->destroySegmentSession (seg_session);
            seg_session->unref ();
        }
        stream_session->forming_seg_sessions.clear ();
    }

    bool const tmp_one_per_stream = stream_session->hls_stream->opts.one_session_per_stream;
    bool const tmp_in_release_queue = stream_session->in_release_queue;

    logD (hls, _func, "stream_session: 0x", fmt_hex, ", ptr = ", (UintPtr) stream_session);

    stream_session->mutex_ss.unlock ();
    stream_session->hls_stream->mutex_objs.unlock ();

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
HlsServer::processStreamHttpRequest (HandlerContext & ctx,
                                      const std::string & stream_name)
{
    logD(hls, _func_);
    Ref<StreamSession> stream_session;

    ConstMemory   const stream_name_mem = ConstMemory(stream_name.c_str(), stream_name.size());

    if (!default_opts.one_session_per_stream)
    {
        stream_session = createStreamSession (stream_name_mem);
        if (!stream_session)
        {
            logA_ ("hls_stream 404 ", ctx.Request().clientAddress().toString().c_str(), " ", ctx.Request().getURI().c_str());
            return ctx.sendHttpNotFound ();
        }
    }
    else
    {
        Ref<HlsStream> hls_stream;

        mutex_main.lock ();

        {
            HlsStreamHash::EntryKey const entry = hls_stream_hash.lookup (ConstMemory (stream_name_mem));
            if (!entry)
            {
                mutex_main.unlock ();
                logA_ ("hls_stream 404 ", ctx.Request().clientAddress().toString().c_str(), " ", ctx.Request().getURI().c_str());
                return ctx.sendHttpNotFound ();
            }

            hls_stream = entry.getData();
        }

        if (!hls_stream->bound_stream_session)
        {
            mutex_main.unlock ();
            logA_ ("hls_stream 404 (bound) ", ctx.Request().clientAddress().toString().c_str(), " ", ctx.Request().getURI().c_str());
            return ctx.sendHttpNotFound ();
        }

        stream_session = hls_stream->bound_stream_session;

        mutex_main.unlock ();
    }

    std::stringstream msg_body;
    msg_body << "#EXTM3U\n";
    msg_body << "#EXT-X-VERSION:3\n";
    msg_body << "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=300000,CODECS=\"avc1.66.31\",RESOLUTION=320x240\n";
    msg_body << "data/segments.m3u8?s=";
    StRef<String> session_id = st_makeString(stream_session->session_id->mem());
    msg_body << session_id->cstr();
    msg_body << "\n";
    msg_body << "#EXT-X-ENDLIST\n";

    ctx.Response().setStatus(HTTPResponse::HTTP_OK);
    ctx.Response().setContentType(_m3u8_mime_type);
    ctx.Response().setContentLength(msg_body.str().length());

    std::ostream& out = ctx.Response().send();

    out << msg_body.str();
    out.flush();

	logA_ ("hls_stream 200 ", ctx.Request().clientAddress().toString().c_str(), " ", ctx.Request().getURI().c_str());

    return Result::Success;
}

Result
HlsServer::processSegmentListHttpRequest (HandlerContext & ctx,
                                          const std::string & stream_session_id)
{
    logD(hls, _func_);
	mutex_main.lock ();

    ConstMemory stream_session_id_mem = ConstMemory(stream_session_id.c_str(), stream_session_id.size());

    Ref<StreamSession> const stream_session = stream_sessions.lookup (stream_session_id_mem);
    if (!stream_session)
    {
        mutex_main.unlock ();
        logD(hls_seg, _func, "stream session not found, id ", stream_session_id.c_str());
        return ctx.sendHttpNotFound ();
    }

    return mt_unlocks (mutex_main) doProcessSegmentListHttpRequest (ctx,
                                                               stream_session,
                                                                "" /* path_prefix */);
}

mt_unlocks (mutex_main) Result
HlsServer::doProcessSegmentListHttpRequest (HandlerContext & ctx,
                                             StreamSession * const stream_session,
                                             const std::string & path_prefix)
{
    logD(hls_seg, _func_, "hls_stream 0x", fmt_hex, (UintPtr) stream_session->hls_stream.ptr());

    mt_unlocks (mutex_main) markStreamSessionRequest (stream_session);

    stream_session->mutex_ss.lock ();
    if (!stream_session->valid)
    {
        stream_session->mutex_ss.unlock ();
        logD(hls_seg, _func_, "stream session invalidated");
        return ctx.sendHttpNotFound ();
    }

    std::stringstream msg_body;
    bool just_started = false;
    if (!stream_session->started)
        just_started = true;

	{
		StateMutexLock lock_hls_stream(&stream_session->hls_stream->mutex_objs);

		HlsStream * hls_stream = stream_session->hls_stream;

        msg_body << "#EXTM3U\n";
        msg_body << "#EXT-X-VERSION:3\n";
        msg_body << "#EXT-X-TARGETDURATION:" << hls_stream->opts.segment_duration_seconds << "\n";
        msg_body << "#EXT-X-ALLOW-CACHE:NO\n";
        msg_body << "#EXT-X-MEDIA-SEQUENCE:" << hls_stream->head_seg_no << "\n";

		HlsSegmentList & segment_list = hls_stream->segment_list;
        Size num_segments = 0;
        HlsSegmentList::iter iter (segment_list);
        while (!segment_list.iter_done (iter))
        {
            HlsSegment * const segment = segment_list.iter_next (iter);
            if (segment->excluded)
                continue;

            msg_body << "#EXTINF:" << extinf_str->cstr() << ",\n";
            StRef<String> session_id = st_makeString(stream_session->session_id->mem());
            msg_body << path_prefix << "segment.ts?s=" << session_id->cstr() << "&n=" << segment->seg_no << "\n";
            ++num_segments;
        }
        logD(hls_seg, _func_, "num_segments: ", num_segments);

        for (unsigned i = 0; i < hls_stream->opts.num_lead_segments; ++i)
		{
            msg_body << "#EXTINF:" << extinf_str->cstr() << ",\n";
            StRef<String> session_id = st_makeString(stream_session->session_id->mem());
            msg_body << path_prefix << "segment.ts?s=" << session_id->cstr() << "&n=" << hls_stream->num_active_segments + i << "\n";
        }
    }
    stream_session->mutex_ss.unlock ();

    if (just_started)
    {
		logD(hls_seg, _func_, "just_started");
		StateMutexLock lock_hls_stream(&stream_session->hls_stream->mutex_objs);

		{
			StateMutexLock lock_stream(&stream_session->mutex_ss);

			if (!stream_session->started)
			{
				stream_session->hls_stream->started_stream_sessions.append (stream_session);
				stream_session->started = true;
			}
		}
    }

	ctx.Response().setStatus(HTTPResponse::HTTP_OK);
    ctx.Response().setContentType(_m3u8_mime_type);
    ctx.Response().setContentLength(msg_body.str().length());

    std::ostream& out = ctx.Response().send();
    out << msg_body.str();
    out.flush();

    logD(hls_seg, _func_, "segment list = ", msg_body.str().c_str());
	logD(hls_seg, _func_, "segment list sent: ", ctx.Request().clientAddress().toString().c_str(), " ", ctx.Request().getURI().c_str());

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
    logD(hls_seg, _func_, "senderClosed");
    SegmentSession * const seg_session = static_cast <SegmentSession*> (_seg_session);
    Ref<StreamSession> const stream_session = seg_session->weak_stream_session.getRef ();
    if (!stream_session) {
      // If StreamSession is gone, then this SegmentSession should have been
      // released already.
        return;
    }

	{
		StateMutexLock lock_stream_session(&stream_session->mutex_ss);

		if (!seg_session->valid) {
			return;
		}
		seg_session->valid = false;

		stream_session->destroySegmentSession (seg_session);

		if (seg_session->in_forming_list)
			stream_session->forming_seg_sessions.remove (seg_session);
		else
			stream_session->waiting_seg_sessions.remove (seg_session);
	}

    seg_session->unref ();
}

Result
HlsServer::processSegmentHttpRequest (HandlerContext & ctx,
                                       const std::string & stream_session_id,
                                       const std::string & seg_no_str)
{
    logD(hls, _func_);
    ConstMemory   const stream_session_id_mem = ConstMemory(stream_session_id.c_str(), stream_session_id.size());
    ConstMemory   const seg_no_str_mem = ConstMemory(seg_no_str.c_str(), seg_no_str.size());
    Uint64 seg_no;
    if (!strToUint64_safe (seg_no_str_mem, &seg_no, 10))
    {
		logD (hls_seg, _func, "Bad seg no param \"n\": ", seg_no_str.c_str());
		return ctx.sendHttpNotFoundAndDoSignal();
    }

    mutex_main.lock ();

    Ref<StreamSession> const stream_session = stream_sessions.lookup (stream_session_id_mem);
    if (!stream_session)
    {
        mutex_main.unlock ();
		logD(hls_seg, _func_, "stream session not found, id ", stream_session_id_mem);
        return ctx.sendHttpNotFoundAndDoSignal();
    }

    logD(hls_seg, _func_, "hls_stream 0x", fmt_hex, (UintPtr) stream_session->hls_stream.ptr());

    mt_unlocks (mutex_main) markStreamSessionRequest (stream_session);

    Ref<SegmentSession> const seg_session = grab (new SegmentSession(ctx));
    seg_session->weak_stream_session = stream_session;
    seg_session->valid = true;
    seg_session->in_forming_list = false;

    seg_session->conn_keepalive = ctx.Request().getKeepAlive();

    // client_address and request_line are not actually used (just for log msgs)
    Uint32 uiAddr = *(Uint32*)ctx.Request().clientAddress().host().addr();
    seg_session->client_address.ip_addr = uiAddr;
    seg_session->client_address.port = Uint16(ctx.Request().clientAddress().port());
    ConstMemory request_line_mem = ConstMemory(ctx.Request().getURI().c_str(), ctx.Request().getURI().size());
    seg_session->request_line = grab (new String (request_line_mem));

    seg_session->seg_no = seg_no;

    return stream_session->addSegmentSession (seg_session);
}

bool
HlsServer::httpRequest (HTTPServerRequest &req, HTTPServerResponse &resp, void * _self)
{
    HandlerContext ctx(req, resp);
    HlsServer * const self = static_cast <HlsServer*> (_self);

    logD(hls_seg, _func_, "START, URI:", req.getURI().c_str(), ", hlsServerP = ", (ptrdiff_t)self);

	std::string decodedURI;
	URI::decode(req.getURI(), decodedURI);
    URI uri(decodedURI);
	logD(hls_seg, _func_, "START, decoded URI:", decodedURI.c_str());
    std::vector < std::string > segments;
    uri.getPathSegments(segments);
    if(segments.size() >= 3 && segments[1].compare("data") == 0)
    {
        std::string ts_ext = ".ts";
        std::string file_name = segments[2];
        logD(hls_seg, _func_, "1, file_name = ", file_name.c_str());
        if(!file_name.compare("segments.m3u8"))
        {
            HTMLForm form( req );

            NameValueCollection::ConstIterator stream_session_id_iter = form.find("s");
            std::string stream_session_id = (stream_session_id_iter != form.end()) ? stream_session_id_iter->second: "";
            logD(hls_seg, _func_, "stream_session_id = ", stream_session_id.c_str());
            Result res = self->processSegmentListHttpRequest (ctx, stream_session_id);
            logD(hls_seg, _func_, "res after sending M3U8 file = ", (bool)(res == Result::Success));
            return res == Result::Success;
        }
        else if(file_name.size() >= ts_ext.size()
                && file_name.substr(file_name.size() - ts_ext.size()).compare(ts_ext) == 0)
        {
			size_t sessionIdIdx = decodedURI.find("?s=");
			size_t segNoIdx = decodedURI.find("&n=");

			if(sessionIdIdx != std::string::npos && segNoIdx != std::string::npos && sessionIdIdx < segNoIdx)
			{
				sessionIdIdx += 3;	// + strlen of "?s="

				const std::string stream_session_id = decodedURI.substr(sessionIdIdx, segNoIdx - sessionIdIdx);

				segNoIdx += 3;		// + strlen of "&n="

				const std::string seg_no_mem = decodedURI.substr(segNoIdx, decodedURI.size() - segNoIdx);

				logD(hls_seg, _func_, "stream_session_id222 = ", stream_session_id.c_str(), ", seg_no_mem = ", seg_no_mem.c_str());
				Result res = self->processSegmentHttpRequest (ctx, stream_session_id, seg_no_mem);

				ctx.waitForSignal();
				logD(hls_seg, _func_, "res after sending TS file = ", (bool)(res == Result::Success));
				return res == Result::Success;
			}
        }
    }

    if (segments.size() >= 2)
    {
        std::string m3u8_ext (".m3u8");
        std::string file_name = segments[1];
        logD(hls_seg, _func_, "2, file_name = ", file_name.c_str());
        if(file_name.size() >= m3u8_ext.size()
                        && file_name.substr(file_name.size() - m3u8_ext.size()).compare(m3u8_ext) == 0)
        {
            std::string stream_name = file_name.substr(0, file_name.rfind(m3u8_ext));
            logD(hls_seg, _func_, "continue, stream_name = ", stream_name.c_str());
            if (!self->default_opts.one_session_per_stream)
            {
                logD(hls_seg, _func_, "continue, !one_session_per_stream");
                Result res = self->processStreamHttpRequest (ctx, stream_name);
                logD(hls_seg, _func_, "continue, !one_session_per_stream, res = ", (bool)(res == Result::Success));
                return res == Result::Success;
            }
            else
            {
                logD(hls_seg, _func_, "continue, one_session_per_stream, before lock ");
                self->mutex_main.lock ();
                Ref<HlsStream> hls_stream;
                {
                    HlsStreamHash::EntryKey const entry = self->hls_stream_hash.lookup (ConstMemory (stream_name.c_str(), stream_name.size()));
                    if (!entry)
                    {
                        self->mutex_main.unlock ();
                        logA_ ("hls_stream 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
                        Result res = ctx.sendHttpNotFound ();
                        return res == Result::Success;
                    }
                    logD(hls_seg, _func_, "continue, hls_stream");
                    hls_stream = entry.getData();
                }
                logD(hls_seg, _func_, "continue, hls_stream->bound_stream_session = ", !!hls_stream->bound_stream_session);
                if (!hls_stream->bound_stream_session)
                {
                    self->mutex_main.unlock ();
                    logA_ ("hls_stream 404 (bound) ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
                    Result res = ctx.sendHttpNotFound ();
                    return res == Result::Success;
                }
                Ref<StreamSession> const stream_session = hls_stream->bound_stream_session;

                Result res = mt_unlocks (mutex_main) self->doProcessSegmentListHttpRequest (ctx,
                                                                                 stream_session,
                                                                                 "data/");
                logD(hls_seg, _func_, "continue, end res = ", (bool)(res == Result::Success));
                return res == Result::Success;
            }
        }
    }

    logD(hls_seg, _func_, "bad request: ", req.getURI().c_str());

    Result res = ctx.sendHttpNotFound ();
    return res == Result::Success;
}

mt_const void
HlsServer::init (MomentServer  * const mt_nonnull moment,
                 StreamOptions * const opts,
                 Time            const stream_timeout,
                 Time            const watcher_timeout_millisec)
{
    this->default_opts = *opts;

    this->stream_timeout               = stream_timeout;
    this->watcher_timeout_millisec     = watcher_timeout_millisec;

    def_reg.setDeferredProcessor (moment->getServerApp()->getServerContext()->getMainThreadContext()->getDeferredProcessor());

	extinf_str = makeString (default_opts.segment_duration_seconds);
// TEST (remove)
//    extinf_str = grab (new String ("0.01"));
    logD(hls_seg, _func, "extinf_str: ", extinf_str);

    ServerApp * const server_app = moment->getServerApp();

    timers = server_app->getServerContext()->getMainThreadContext()->getTimers();
    page_pool = moment->getPagePool();

    segments_cleanup_timer =
            timers->addTimer (CbDesc<Timers::TimerCallback> (
                                      segmentsCleanupTimerTick,
                                      this,
                                      this),
                              default_opts.segment_duration_seconds,
                              true /* periodical */);

    stream_session_cleanup_timer =
            timers->addTimer (CbDesc<Timers::TimerCallback> (
                                      streamSessionCleanupTimerTick,
                                      this,
                                      this),
                              default_opts.segment_duration_seconds / 2 > 0 ? default_opts.segment_duration_seconds / 2 : 1,
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

    HttpReqHandler::addHandler(std::string("hls"), httpRequest, this);
}


Referenced HlsServer::m_refsHlsSegment;
Referenced HlsServer::m_refsSegmentSession;
Referenced HlsServer::m_refsStreamSession;
Referenced HlsServer::m_refsHlsStream;

HlsServer::HlsServer ()
    : timers (NULL),
      page_pool (NULL)
{
    logD(hls_seg, _func_, "HlsServer CONSTRUCTOR, this = ", (ptrdiff_t)this);
}

HlsServer::~HlsServer ()
{
	StateMutexLock lock_main(&mutex_main);
    logD(hls_seg, _func_, "HlsServer DESTRUCTOR, this = ", (ptrdiff_t)this);
    {
        StreamSessionHash::iter iter (stream_sessions);
        while (!stream_sessions.iter_done (iter)) {
            StreamSession * const stream_session = stream_sessions.iter_next (iter);
            bool const destroyed = destroyStreamSession (stream_session, true /* force_destroy */);
            assert (destroyed);
        }
    }
}

static void momentHlsInit ()
{
    logD(hls_seg, _func, "Initializing mod_hls");

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
			logI(hls_seg, _func, "Apple HTTP Live Streaming module is not enabled. "
				"Set \"", opt_name, "\" option to \"y\" to enable.");
			return;
		}
    }

    bool one_session_per_stream = true;
    {
        ConstMemory const opt_name = "mod_hls/one_session_per_stream";
        MConfig::BooleanValue const val = config->getBoolean (opt_name);
        if (val == MConfig::Boolean_Invalid)
            logE_ (_func, "bad value for ", opt_name, ": ", config->getString (opt_name));

        if (val == MConfig::Boolean_False)
            one_session_per_stream = false;

        logI(hls_seg, _func, opt_name, ": ", one_session_per_stream);
    }

//     bool realtime_mode = false;
//     {
//         ConstMemory const opt_name = "mod_hls/realtime_mode";
//         MConfig::BooleanValue const val = config->getBoolean (opt_name);
//         if (val == MConfig::Boolean_Invalid) {
//             logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
//             return;
//         }
// 
//         if (val == MConfig::Boolean_True)
//             realtime_mode = true;
// 
//     logI(hls_seg, _func, opt_name, ": ", realtime_mode);
//     }

    Uint64 segment_duration = 3;
    {
        ConstMemory const opt_name = "mod_hls/segment_duration";
        if (!config->getUint64_default (opt_name, &segment_duration, segment_duration))
            logE_ (_func, "bad value for ", opt_name);

        logI(hls_seg, _func, opt_name, ": ", segment_duration);
    }

    Uint64 num_real_segments = 8;
    {
        ConstMemory const opt_name = "mod_hls/num_real_segments";
        if (!config->getUint64_default (opt_name, &num_real_segments, num_real_segments))
            logE_ (_func, "bad value for ", opt_name);

        logI(hls_seg, _func, opt_name, ": ", num_real_segments);
    }

    Uint64 num_lead_segments = 0;
    {
        ConstMemory const opt_name = "mod_hls/num_lead_segments";
        if (!config->getUint64_default (opt_name, &num_lead_segments, num_lead_segments))
            logE_ (_func, "bad value for ", opt_name);

        logI(hls_seg, _func, opt_name, ": ", num_lead_segments);
    }

    Uint64 stream_timeout = 60;
    {
        ConstMemory const opt_name = "mod_hls/stream_timeout";
        if (!config->getUint64_default (opt_name, &stream_timeout, stream_timeout))
            logE_ (_func, "bad value for ", opt_name);

        logI(hls_seg, _func, opt_name, ": ", stream_timeout);
    }

    Uint64 watcher_timeout_millisec = 5000;
    {
        ConstMemory const opt_name = "mod_hls/watcher_timeout";
        if (!config->getUint64_default (opt_name, &watcher_timeout_millisec, watcher_timeout_millisec))
            logE_ (_func, "bad value for ", opt_name);

        logI(hls_seg, _func, opt_name, ": ", watcher_timeout_millisec);
    }

    HlsServer::StreamOptions opts;
    opts.one_session_per_stream    = one_session_per_stream;
    //opts.realtime_mode             = realtime_mode; - now realtime mode is not supported.
	opts.segment_duration_seconds  = segment_duration;
    opts.num_real_segments         = num_real_segments;
    opts.num_lead_segments         = num_lead_segments;

    glob_hls_server.init (moment,
                          &opts,
                          stream_timeout,
                          watcher_timeout_millisec);
}

static void momentHlsUnload ()
{
    logD(hls_seg, _func, "Unloading mod_hls");
}

} // end namespace MomentHls

namespace M {

void libMary_moduleInit ()
{
    MomentHls::momentHlsInit ();
}

void libMary_moduleUnload ()
{
    MomentHls::momentHlsUnload ();
}

}

