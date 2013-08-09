/*  Moment Video Server - High performance media server
    Copyright (C) 2011-2013 Dmitry Shatrov
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
#include <moment/rtmp_push_protocol.h>
#include <moment/rtmp_fetch_protocol.h>


#define MOMENT_RTMP__FLOW_CONTROL


namespace Moment {

namespace {

static LogGroup libMary_logGroup_mod_rtmp  ("mod_rtmp", LogLevel::I);
static LogGroup libMary_logGroup_session   ("mod_rtmp.session", LogLevel::I);
static LogGroup libMary_logGroup_framedrop ("mod_rtmp.framedrop", LogLevel::I);

class MomentRtmpModule : public Object
{
public:
    RtmpService  rtmp_service;
    RtmptService rtmpt_service;


  // __________________________________ Admin __________________________________

    static HttpService::HttpHandler const admin_http_handler;

    static Result adminHttpRequest (HttpRequest   * mt_nonnull req,
                                    Sender        * mt_nonnull conn_sender,
                                    Memory const  &msg_body,
                                    void         ** mt_nonnull ret_msg_data,
                                    void          *_self);

  // ___________________________________________________________________________


    MomentRtmpModule ()
        : rtmp_service  (this /* coderef_container */),
          rtmpt_service (this /* coderef_container */)
    {
    }
};

mt_const bool audio_waits_video = false;
mt_const bool wait_for_keyframe = true;
mt_const bool default_start_paused = false;
mt_const Uint64 paused_avc_interframes = 3;

mt_const bool record_all = false;
mt_const StRef<String> record_path = st_grab (new (std::nothrow) String ("/opt/moment/records"));
mt_const Uint64 recording_limit = 1 << 24 /* 16 Mb */;

mt_const Count no_keyframe_limit = 250; // 25 fps * 10 seconds

mt_const DataDepRef<MomentServer> moment (NULL /* coderef_container */);
mt_const DataDepRef<Timers> timers (NULL /* coderef_container */);
mt_const DataDepRef<PagePool> page_pool (NULL /* coderef_container */);

mt_const Stat::ParamKey stat_num_sessions;

class TranscodeEntry : public Referenced
{
public:
    Ref<String> suffix;
    Ref<String> chain;
    Transcoder::TranscodingMode audio_mode;
    Transcoder::TranscodingMode video_mode;
};

typedef List< Ref<TranscodeEntry> > TranscodeList;

mt_const TranscodeList transcode_list;
mt_const bool transcode_on_demand = true;
mt_const Uint64 transcode_on_demand_timeout_millisec = 5000;

class StreamingParams
{
public:
    bool transcode;
    Ref<String> auth_key;

    void reset ()
    {
        transcode = false;
    }

    StreamingParams ()
    {
        reset ();
    }
};

class WatchingParams
{
public:
    bool start_paused;
    Ref<String> auth_key;

    void reset ()
    {
	start_paused = default_start_paused;
        auth_key = NULL;
    }

    WatchingParams ()
    {
	reset ();
    }
};

class ClientSession : public Object
{
public:
    StateMutex mutex;

    mt_mutex (mutex) bool valid;

    IpAddress client_addr;

    mt_const DataDepRef<RtmpConnection> rtmp_conn;
    // Remember that RtmpConnection must be available when we're calling
    // RtmpServer's methods.
    RtmpServer rtmp_server;

    ServerThreadContext *recorder_thread_ctx;
    AvRecorder recorder;
    FlvMuxer flv_muxer;

    mt_const Ref<String> stream_name;

    mt_mutex (mutex) Ref<MomentServer::ClientSession> srv_session;

    mt_mutex (mutex) Ref<VideoStream> video_stream;
    mt_mutex (mutex) Ref<Transcoder> transcoder;
    mt_mutex (mutex) List<MomentServer::VideoStreamKey> out_stream_keys;

    mt_mutex (mutex) Ref<VideoStream> watching_video_stream;

    mt_mutex (mutex) StreamingParams streaming_params;
    mt_mutex (mutex) WatchingParams watching_params;

#ifdef MOMENT_RTMP__FLOW_CONTROL
    mt_mutex (mutex) bool overloaded;
#endif

    mt_mutex (mutex) Count no_keyframe_counter;
    mt_mutex (mutex) bool keyframe_sent;
    mt_mutex (mutex) bool first_keyframe_sent;
    mt_mutex (mutex) Uint64 first_interframes_sent;

    mt_mutex (mutex) bool resumed;

    mt_mutex (mutex) PagePool *paused_keyframe_page_pool;
    mt_mutex (mutex) PagePool::Page *paused_keyframe_page;

    // Synchronized by rtmp_server.
    bool streaming;
    bool watching;

    void doResume ();

    ClientSession ()
	: valid (true),
          rtmp_conn   (this /* coderef_container */),
          rtmp_server (this /* coderef_container */),
	  recorder_thread_ctx (NULL),
	  recorder (this),
#ifdef MOMENT_RTMP__FLOW_CONTROL
	  overloaded (false),
#endif
	  no_keyframe_counter (0),
	  keyframe_sent       (false),
	  first_keyframe_sent (false),
          first_interframes_sent (0),
	  resumed   (false),
          paused_keyframe_page_pool (NULL),
          paused_keyframe_page      (NULL),
	  streaming (false),
	  watching  (false)
    {
	logD (session, _func, "0x", fmt_hex, (UintPtr) this);
    }

    ~ClientSession ()
    {
	logD (session, _func, "0x", fmt_hex, (UintPtr) this);

	MomentServer * const moment = MomentServer::getInstance();

	if (recorder_thread_ctx) {
	    moment->getRecorderThreadPool()->releaseThreadContext (recorder_thread_ctx);
	    recorder_thread_ctx = NULL;
	}

        mutex.lock ();
        if (paused_keyframe_page) {
            assert (paused_keyframe_page_pool);
            paused_keyframe_page_pool->pageUnref (paused_keyframe_page);
            paused_keyframe_page = NULL;
            paused_keyframe_page_pool = NULL;
        }
        mutex.unlock ();
    }
};

static mt_mutex (client_session) Result savedAudioFrame (VideoStream::AudioMessage * mt_nonnull audio_msg,
                                                         void                      *_client_session);

static mt_mutex (client_session) Result savedVideoFrame (VideoStream::VideoMessage * mt_nonnull video_msg,
                                                         void                      *_client_session);

static VideoStream::FrameSaver::FrameHandler const saved_frame_handler = {
    savedAudioFrame,
    savedVideoFrame
};

void
ClientSession::doResume ()
{
    mutex.lock ();
    if (!resumed) {
        resumed = true;
        if (video_stream) {
            video_stream->lock ();
            video_stream->getFrameSaver()->reportSavedFrames (&saved_frame_handler, this);
            video_stream->unlock ();

            if (paused_keyframe_page) {
                assert (paused_keyframe_page_pool);
                paused_keyframe_page_pool->pageUnref (paused_keyframe_page);
                paused_keyframe_page_pool = NULL;
                paused_keyframe_page = NULL;
            }
        }
    }
    mutex.unlock ();
}

void destroyClientSession (ClientSession * const client_session)
{
    client_session->mutex.lock ();
    if (!client_session->valid) {
	client_session->mutex.unlock ();
	logD (mod_rtmp, _func, "invalid session");
	return;
    }
    client_session->valid = false;
    client_session->mutex.unlock ();

    client_session->recorder.stop();

    client_session->mutex.lock ();
    getStat()->addInt (stat_num_sessions, -1);

    {
        List<MomentServer::VideoStreamKey>::iter iter (client_session->out_stream_keys);
        while (!client_session->out_stream_keys.iter_done (iter)) {
            MomentServer::VideoStreamKey &stream_key = client_session->out_stream_keys.iter_next (iter)->data;
            moment->removeVideoStream (stream_key);
        }
        client_session->out_stream_keys.clear ();
    }

    Ref<VideoStream> const video_stream = client_session->video_stream;

    Ref<MomentServer::ClientSession> const srv_session = client_session->srv_session;
    client_session->srv_session = NULL;

    client_session->mutex.unlock ();

    MomentServer * const moment = MomentServer::getInstance();

    if (srv_session)
	moment->clientDisconnected (srv_session);

    // Closing video stream *after* firing clientDisconnected() to avoid
    // premature closing of client connections in streamClosed().
    if (video_stream)
	video_stream->close ();

    client_session->unref ();
}

static mt_mutex (client_session->mutex) void
startTranscoder (ClientSession * const client_session)
{
    if (!client_session->streaming_params.transcode ||
        !client_session->video_stream ||
        client_session->transcoder)
    {
        logD_ (_func, "no transcoding for stream \"", client_session->stream_name, "\"");
        return;
    }

    Ref<Transcoder> transcoder;
    if (!transcode_list.isEmpty()) {
        transcoder = moment->newTranscoder();
        if (!transcoder) {
            logD_ (_func, "transcoder module is missing");
            return;
        }

        transcoder->init (moment->getServerApp()->getServerContext()->
                                  getMainThreadContext()->getDeferredProcessor(),
                          timers,
                          moment->getPagePool(),
                          transcode_on_demand,
                          transcode_on_demand_timeout_millisec);

        TranscodeList::iter iter (transcode_list);
        while (!transcode_list.iter_done (iter)) {
            Ref<TranscodeEntry> &transcode_entry = transcode_list.iter_next (iter)->data;

            Ref<String> const out_stream_name =
                    makeString (client_session->stream_name ?
                                        client_session->stream_name->mem() : ConstMemory(),
                                transcode_entry->suffix->mem());
            Ref<VideoStream> const out_stream = grab (new (std::nothrow) VideoStream);
            {
                Ref<StreamParameters> const stream_params = grab (new (std::nothrow) StreamParameters);
                if (transcode_entry->audio_mode == Transcoder::TranscodingMode_Off)
                    stream_params->setParam ("no_audio", "true");
                if (transcode_entry->video_mode == Transcoder::TranscodingMode_Off)
                    stream_params->setParam ("no_video", "true");

                out_stream->setStreamParameters (stream_params);
            }

            transcoder->addOutputStream (out_stream,
                                         transcode_entry->chain->mem(),
                                         transcode_entry->audio_mode,
                                         transcode_entry->video_mode);

            MomentServer::VideoStreamKey const out_stream_key =
                    moment->addVideoStream (out_stream, out_stream_name->mem());
            client_session->out_stream_keys.append (out_stream_key);
        }

        transcoder->start (client_session->video_stream);
    }

    client_session->transcoder = transcoder;
}

void streamAudioMessage (VideoStream::AudioMessage * const mt_nonnull msg,
			 void                      * const _session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_session);

    client_session->mutex.lock ();

    if (msg->frame_type == VideoStream::AudioFrameType::RawData) {
        if (!client_session->resumed) {
            client_session->mutex.unlock ();
            return;
        }

        if (audio_waits_video) {
            if (!client_session->first_keyframe_sent) {
                client_session->mutex.unlock ();
                return;
            }
        }
    }

#ifdef MOMENT_RTMP__FLOW_CONTROL
    if (client_session->overloaded
	&& msg->frame_type == VideoStream::AudioFrameType::RawData)
    {
      // Connection overloaded, dropping this audio frame.
	logD (framedrop, _func, "Connection overloaded, dropping audio frame");
	client_session->mutex.unlock ();
	return;
    }
#endif

    client_session->mutex.unlock ();

    client_session->rtmp_conn->sendAudioMessage (msg);
}

void streamVideoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
			 void                      * const _session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_session);

/**/
    logS_ (_func, "ts ", msg->timestamp_nanosec, " ", msg->frame_type, (msg->is_saved_frame ? " SAVED" : ""));
    logS_ (_func, "fks: ", client_session->first_keyframe_sent, ", "
           "ks: ", client_session->keyframe_sent, ", "
           "nkfk: ", client_session->no_keyframe_counter, ", "
           "moff: ", msg->msg_offset);
/**/

    client_session->mutex.lock ();

#ifdef MOMENT_RTMP__FLOW_CONTROL
    if (client_session->overloaded
	&& (   msg->frame_type == VideoStream::VideoFrameType::KeyFrame
	    || msg->frame_type == VideoStream::VideoFrameType::InterFrame
	    || msg->frame_type == VideoStream::VideoFrameType::DisposableInterFrame
	    || msg->frame_type == VideoStream::VideoFrameType::GeneratedKeyFrame))
    {
      // Connection overloaded, dropping this video frame. In general, we'll
      // have to wait for the next keyframe after we've dropped a frame.
      // We do not care about disposable frames yet.

	logD (framedrop, _func, "Connection overloaded, dropping video frame");

	client_session->no_keyframe_counter = 0;
	client_session->keyframe_sent = false;

	client_session->mutex.unlock ();
	return;
    }
#endif // MOMENT_RTMP__FLOW_CONTROL

    bool got_keyframe = false;
    if (/* TODO WRONG? !msg->is_saved_frame
        && */ (msg->frame_type == VideoStream::VideoFrameType::KeyFrame ||
	    msg->frame_type == VideoStream::VideoFrameType::GeneratedKeyFrame))
    {
	got_keyframe = true;
    } else
    if (msg->frame_type == VideoStream::VideoFrameType::AvcSequenceHeader ||
        msg->frame_type == VideoStream::VideoFrameType::AvcEndOfSequence)
    {
        client_session->keyframe_sent = false;
    } else
    if (!client_session->keyframe_sent
	&& (   msg->frame_type == VideoStream::VideoFrameType::InterFrame
	    || msg->frame_type == VideoStream::VideoFrameType::DisposableInterFrame))
    {
	++client_session->no_keyframe_counter;
	if (client_session->no_keyframe_counter >= no_keyframe_limit) {
            logD_ (_func, "no_keyframe_limit hit: ", client_session->no_keyframe_counter);
	    got_keyframe = true;
	} else
        if (wait_for_keyframe) {
	  // Waiting for a keyframe, dropping current video frame.
            logS_ (_func, "wait_for_keyframe, dropping");
	    client_session->mutex.unlock ();
	    return;
	}
    }

    if (got_keyframe)
	client_session->no_keyframe_counter = 0;

    if (!client_session->resumed &&
        msg->frame_type.isVideoData())
    {
        if (msg->frame_type.isInterFrame())
        {
            if (msg->codec_id != VideoStream::VideoCodecId::AVC
                || client_session->first_interframes_sent >= paused_avc_interframes
                || !client_session->first_keyframe_sent)
            {
                client_session->mutex.unlock ();
//                logD_ (_func, "--- !resumed, interframe, dropping");
                return;
            }
            ++client_session->first_interframes_sent;
        } else {
            assert (msg->frame_type.isKeyFrame());

            if (client_session->first_keyframe_sent) {
//                logD_ (_func, "--- !resumed, first_keyframe_sent, dropping");
                client_session->keyframe_sent = false;
                client_session->mutex.unlock ();
                return;
            }

            if (client_session->paused_keyframe_page) {
                assert (client_session->paused_keyframe_page_pool);
                client_session->paused_keyframe_page_pool->pageUnref (client_session->paused_keyframe_page);
                client_session->paused_keyframe_page = NULL;
                client_session->paused_keyframe_page_pool = NULL;
            }

            client_session->paused_keyframe_page_pool = msg->page_pool;
            client_session->paused_keyframe_page = msg->page_list.first;
            msg->page_pool->pageRef (msg->page_list.first);
        }
    }

    if (got_keyframe) {
//        logD_ (_func, "first_keyframe_sent = true");
	client_session->first_keyframe_sent = true;
	client_session->keyframe_sent = true;
    }

    client_session->mutex.unlock ();

//    logD_ (_func, "sending ", toString (msg->codec_id), ", ", toString (msgo->frame_type));

    client_session->rtmp_conn->sendVideoMessage (msg);
}

void streamClosed (void * const _session)
{
    logD (session, _func_);
    ClientSession * const client_session = static_cast <ClientSession*> (_session);
    client_session->rtmp_conn->closeAfterFlush ();
}

VideoStream::EventHandler const video_event_handler = {
    streamAudioMessage,
    streamVideoMessage,
    NULL /* rtmpCommandMessage */,
    streamClosed,
    NULL /* numWatchersChanged */
};

Result connect (ConstMemory const &app_name,
		void * const _client_session)
{
    logD (session, _func, "app_name: ", app_name);

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    // TODO class MomentRtmpModule
    MomentServer * const moment = MomentServer::getInstance();

    Ref<MomentServer::ClientSession> const srv_session =
	    moment->rtmpClientConnected (app_name, client_session->rtmp_conn, client_session->client_addr);
    if (!srv_session)
	return Result::Failure;

    client_session->mutex.lock ();
    if (!client_session->valid) {
	assert (!client_session->srv_session);
	client_session->mutex.unlock ();

        moment->clientDisconnected (srv_session);
	return Result::Failure;
    }
    client_session->srv_session = srv_session;
    client_session->mutex.unlock ();

    return Result::Success;
}

static void startRtmpStreaming_paramCallback (ConstMemory   const name,
                                              ConstMemory   const value,
                                              void        * const _streaming_params)
{
    StreamingParams * const streaming_params = static_cast <StreamingParams*> (_streaming_params);

    logD_ (_func, "name: ", name, ", value: ", value);

    if (equal (name, "transcode"))
	streaming_params->transcode = true;
    if (equal (name, "auth"))
        streaming_params->auth_key = grab (new (std::nothrow) String (value));
}

static void startRtmpWatching_paramCallback (ConstMemory   const name,
                                             ConstMemory   const value,
                                             void        * const _watching_params)
{
    WatchingParams * const watching_params = static_cast <WatchingParams*> (_watching_params);

    if (equal (name, "paused"))
	watching_params->start_paused = true;
    else
    if (equal (name, "auth"))
        watching_params->auth_key = grab (new (std::nothrow) String (value));
}

class StartStreamingCallback_Data : public Referenced
{
public:
    WeakRef<ClientSession> weak_client_session;
    Cb<RtmpServer::StartRtmpStreamingCallback> cb;
    Ref<String> stream_name;
    RecordingMode rec_mode;
    Ref<VideoStream> video_stream;
};

static void completeStartStreaming (ClientSession *client_session,
                                    ConstMemory    stream_name,
                                    RecordingMode  rec_mode,
                                    VideoStream   *video_stream);

static void startStreamingCallback (Result   const res,
                                    void   * const _data)
{
    StartStreamingCallback_Data * const data = static_cast <StartStreamingCallback_Data*> (_data);

    Ref<ClientSession> const client_session = data->weak_client_session.getRef ();
    if (!client_session) {
        logD (mod_rtmp, _func, "client session gone");
        data->cb.call_ (Result::Failure);
        return;
    }

    if (!res) {
        logD (mod_rtmp, _func, "streaming not allowed");
        data->cb.call_ (Result::Failure);
        return;
    }

    completeStartStreaming (client_session,
                            data->stream_name->mem(),
                            data->rec_mode,
                            data->video_stream);
    data->cb.call_ (Result::Success);
}

static void completeStartStreaming (ClientSession * const client_session,
                                    ConstMemory     const stream_name,
                                    RecordingMode   const rec_mode,
                                    VideoStream   * const video_stream)
{
    client_session->mutex.lock ();
    if (!client_session->valid) {
        client_session->mutex.unlock ();
        return;
    }
    client_session->mutex.unlock ();

    if (record_all) {
	logD_ (_func, "rec_mode: ", (Uint32) rec_mode);
	if (rec_mode == RecordingMode::Replace ||
	    rec_mode == RecordingMode::Append)
	{
	    logD_ (_func, "recording");
	    // TODO Support "append" mode.
	    client_session->recorder.setVideoStream (video_stream);
	    client_session->recorder.start (
		    makeString (record_path->mem(), stream_name, ".flv")->mem());
	}
    }

    client_session->mutex.lock ();
    if (!client_session->valid) {
        client_session->mutex.unlock ();
        client_session->recorder.stop();
        return;
    }

    startTranscoder (client_session);
    client_session->mutex.unlock ();
}

bool startRtmpStreaming (ConstMemory     const _stream_name,
                         RecordingMode   const rec_mode,
                         bool            const momentrtmp_proto,
                         CbDesc<RtmpServer::StartRtmpStreamingCallback> const &cb,
                         Result        * const mt_nonnull ret_res,
                         void          * const _client_session)
{
    *ret_res = Result::Failure;

    logD (session, _func, "stream_name: ", _stream_name);

    // TODO class MomentRtmpModule
    MomentServer * const moment = MomentServer::getInstance();

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    if (client_session->streaming) {
	logE (mod_rtmp, _func, "already streaming another stream");
	*ret_res = Result::Success;
        return true;
    }
    client_session->streaming = true;

    ConstMemory stream_name = _stream_name;

    // 'srv_session' is created in connect(), which is synchronized with
    // startStreaming(). No locking needed.
    Ref<StreamParameters> const stream_params = grab (new (std::nothrow) StreamParameters);
    stream_params->setParam ("source", momentrtmp_proto ? ConstMemory ("momentrtmp") : ConstMemory ("rtmp"));
    if (!momentrtmp_proto) {
        // TODO nellymoser? Use "source" param from above instead.
        stream_params->setParam ("audio_codec", "speex");
    }

    Ref<VideoStream> const video_stream = grab (new (std::nothrow) VideoStream);
    video_stream->setStreamParameters (stream_params);

    logD_ (_func, "client_session: 0x", fmt_hex, (UintPtr) client_session, ", "
           "video_stream: 0x", (UintPtr) video_stream.ptr());

    client_session->mutex.lock ();
    if (!client_session->valid) {
        client_session->mutex.unlock ();
        *ret_res = Result::Failure;
        return true;
    }

    client_session->video_stream = video_stream;
    client_session->streaming_params.reset ();
    {
	Byte const * const name_sep = (Byte const *) memchr (stream_name.mem(), '?', stream_name.len());
	if (name_sep) {
            ConstMemory const params_mem = stream_name.region (name_sep + 1 - stream_name.mem());
            logD_ (_func, "parameters: ", params_mem);
	    parseHttpParameters (params_mem,
                                 startRtmpStreaming_paramCallback,
                                 &client_session->streaming_params);
	    stream_name = stream_name.region (0, name_sep - stream_name.mem());
	}
    }
    client_session->stream_name = grab (new (std::nothrow) String (stream_name));
    client_session->mutex.unlock ();

    Result res = Result::Failure;
    {
        Ref<StartStreamingCallback_Data> const data = grab (new (std::nothrow) StartStreamingCallback_Data);
        data->weak_client_session = client_session;
        data->cb = cb;
        data->stream_name = grab (new (std::nothrow) String (stream_name));
        data->rec_mode = rec_mode;
        data->video_stream = video_stream;

        bool const complete = moment->startStreaming (client_session->srv_session,
                                                      stream_name,
                                                      (client_session->streaming_params.auth_key ?
                                                              client_session->streaming_params.auth_key->mem() : ConstMemory()),
                                                      video_stream,
                                                      rec_mode,
                                                      CbDesc<MomentServer::StartStreamingCallback> (
                                                              startStreamingCallback,
                                                              data,
                                                              NULL,
                                                              data),
                                                      &res);
        if (!complete)
            return false;
    }

    if (!res) {
        *ret_res = Result::Failure;
        return true;
    }

    completeStartStreaming (client_session, stream_name, rec_mode, video_stream);
    *ret_res = Result::Success;
    return true;
}

static mt_mutex (client_session) Result
savedAudioFrame (VideoStream::AudioMessage * const mt_nonnull audio_msg,
                 void                      * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    if (!client_session->resumed
	&& audio_msg->frame_type == VideoStream::AudioFrameType::RawData)
    {
        return Result::Success;
    }

    client_session->rtmp_conn->sendAudioMessage (audio_msg);

    return Result::Success;
}

static mt_mutex (client_session) Result
savedVideoFrame (VideoStream::VideoMessage * const mt_nonnull video_msg,
                 void                      * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

/**/
    logS_ (_func, "ts ", video_msg->timestamp_nanosec, " ", video_msg->frame_type);
    logS_ (_func, "fks: ", client_session->first_keyframe_sent, ", "
           "ks: ", client_session->keyframe_sent, ", "
           "nkfk: ", client_session->no_keyframe_counter);
/**/

    if (!client_session->resumed
        && video_msg->frame_type.isInterFrame())
    {
        logS_ (_func, "!resumed, interframe, dropping");
        if (video_msg->codec_id != VideoStream::VideoCodecId::AVC
            || client_session->first_interframes_sent >= paused_avc_interframes
            || !client_session->first_keyframe_sent)
        {
            return Result::Success;
        }
        ++client_session->first_interframes_sent;
    }

    if (video_msg->frame_type.isKeyFrame()) {
        if (video_msg->page_list.first == client_session->paused_keyframe_page) {
            logS_ (_func, "same as paused keyframe, dropping");
            return Result::Success;
        }

        if (!client_session->resumed) {
            if (client_session->paused_keyframe_page) {
                assert (client_session->paused_keyframe_page_pool);
                client_session->paused_keyframe_page_pool->pageUnref (client_session->paused_keyframe_page);
                client_session->paused_keyframe_page = NULL;
                client_session->paused_keyframe_page_pool = NULL;
            }

            client_session->paused_keyframe_page_pool = video_msg->page_pool;
            client_session->paused_keyframe_page = video_msg->page_list.first;
            video_msg->page_pool->pageRef (video_msg->page_list.first);
        }

//        logD_ (_func, "first_keyframe_sent = true");
        client_session->first_keyframe_sent = true;
        client_session->keyframe_sent = true;
        client_session->no_keyframe_counter = 0;
    }

// TODO Set the same timestamp for prepush video messages (last video msg timestamp?).
//    VideoStream::VideoMessage tmp_video_msg = *video_msg;
//    tmp_video_msg.timestamp_nanosec = 0;

//    client_session->rtmp_conn->sendVideoMessage (&tmp_video_msg);
    client_session->rtmp_conn->sendVideoMessage (video_msg);
    return Result::Success;
}

class StartWatchingCallback_Data : public Referenced
{
public:
    WeakRef<ClientSession> weak_client_session;
    Cb<RtmpServer::StartRtmpWatchingCallback> cb;
    Ref<String> stream_name;
};

static Result completeStartWatching (VideoStream   *video_stream,
                                     ClientSession *client_session,
                                     ConstMemory    stream_name);

static void startWatchingCallback (VideoStream * const video_stream,
                                   void        * const _data)
{
    StartWatchingCallback_Data * const data = static_cast <StartWatchingCallback_Data*> (_data);
    Ref<ClientSession> const client_session = data->weak_client_session.getRef ();

    if (!video_stream) {
        logD_ (_func, "video stream not found: ", data->stream_name);
        data->cb.call_ (Result::Failure);
        return;
    }

    if (!client_session) {
        MomentServer::getInstance()->decStreamUseCount (video_stream);
        logD_ (_func, "client session gone");
        data->cb.call_ (Result::Failure);
        return;
    }

    Result const res = completeStartWatching (video_stream, client_session, data->stream_name->mem());
    data->cb.call_ (res);
}

static void clientSessionDeletionCallback (void * const _stream)
{
    VideoStream * const stream = static_cast <VideoStream*> (_stream);
    MomentServer * const moment = MomentServer::getInstance();
    moment->decStreamUseCount (stream);
}

static Result completeStartWatching (VideoStream   * const video_stream,
                                     ClientSession * const client_session,
                                     ConstMemory     const stream_name)
{
    client_session->addDeletionCallback (
            CbDesc<Object::DeletionCallback> (clientSessionDeletionCallback, video_stream, video_stream));

    client_session->mutex.lock ();
    // TODO Set watching_video_stream to NULL when it's not needed anymore.
    client_session->watching_video_stream = video_stream;

    video_stream->lock ();
    if (video_stream->isClosed_unlocked()) {
        video_stream->unlock ();
        client_session->mutex.unlock ();

        logD (mod_rtmp, _func, "video stream closed: ", stream_name);
        return Result::Failure;
    }

    video_stream->getFrameSaver()->reportSavedFrames (&saved_frame_handler, client_session);
    client_session->mutex.unlock ();

#warning TODO Proxy video stream events through mod_rtmp to prechunk all non-prechunked data before informAll() to avoid loosing lots of memory and wasting CPU when chunking for each client individually.

    video_stream->getEventInformer()->subscribe_unlocked (&video_event_handler,
                                                          client_session,
                                                          NULL /* ref_data */,
                                                          client_session);
    mt_async mt_unlocks_locks (video_stream->mutex) video_stream->plusOneWatcher_unlocked (
            client_session /* guard_obj */);
    video_stream->unlock ();

    return Result::Success;
}

static bool startRtmpWatching (ConstMemory    const _stream_name,
                               CbDesc<RtmpServer::StartRtmpWatchingCallback> const &cb,
                               Result       * const mt_nonnull ret_res,
                               void         * const _client_session)
{
    *ret_res = Result::Failure;

    MomentServer * const moment = MomentServer::getInstance();

    logD (mod_rtmp, _func, "client_session 0x", fmt_hex, (UintPtr) _client_session);
    logD (mod_rtmp, _func, "stream_name: ", _stream_name);

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    if (client_session->watching) {
	logE (mod_rtmp, _func, "already watching another stream");
        *ret_res = Result::Success;
        return true;
    }
    client_session->watching = true;

    ConstMemory stream_name = _stream_name;
    ConstMemory stream_name_with_params = stream_name;

    client_session->mutex.lock ();
    if (!client_session->valid) {
        client_session->mutex.unlock ();
        *ret_res = Result::Failure;
        return true;
    }

    client_session->watching_params.reset ();
    {
	Byte const * const name_sep = (Byte const *) memchr (stream_name.mem(), '?', stream_name.len());
	if (name_sep) {
	    parseHttpParameters (stream_name.region (name_sep + 1 - stream_name.mem()),
                                 startRtmpWatching_paramCallback,
                                 &client_session->watching_params);
	    stream_name = stream_name.region (0, name_sep - stream_name.mem());
	}
    }
    client_session->resumed = !client_session->watching_params.start_paused;
    client_session->mutex.unlock ();

    Ref<VideoStream> video_stream;
    {
        Ref<StartWatchingCallback_Data> const data = grab (new (std::nothrow) StartWatchingCallback_Data);
        data->weak_client_session = client_session;
        data->cb = cb;
        data->stream_name = grab (new (std::nothrow) String (stream_name));

        bool const complete =
                moment->startWatching (client_session->srv_session,
                                       stream_name,
                                       stream_name_with_params,
                                       (client_session->watching_params.auth_key ?
                                               client_session->watching_params.auth_key->mem() :
                                               ConstMemory()),
                                       CbDesc<MomentServer::StartWatchingCallback> (
                                               startWatchingCallback, data, NULL, data),
                                       &video_stream);
        if (!complete)
            return false;
    }

    if (!video_stream) {
        logD (mod_rtmp, _func, "video stream not found: ", stream_name);
        *ret_res = Result::Failure;
        return true;
    }

    Result const res = completeStartWatching (video_stream, client_session, stream_name);
    *ret_res = res;
    return true;
}

RtmpServer::CommandResult server_commandMessage (RtmpConnection       * const mt_nonnull conn,
						 Uint32                 const msg_stream_id,
						 ConstMemory const    &method_name,
						 VideoStream::Message * const mt_nonnull msg,
						 AmfDecoder           * const mt_nonnull amf_decoder,
						 void                 * const _client_session)
{
    logD (session, _func, "method_name: ", method_name);

    MomentServer * const moment = MomentServer::getInstance();
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    if (equal (method_name, "resume")) {
      // TODO Unused, we never get here.
        client_session->doResume ();
	conn->doBasicMessage (msg_stream_id, amf_decoder);
    } else {
	client_session->mutex.lock ();
	Ref<MomentServer::ClientSession> const srv_session = client_session->srv_session;
	client_session->mutex.unlock ();

	if (!srv_session) {
	    logW_ (_func, "No server session, command message dropped");
	    return RtmpServer::CommandResult::UnknownCommand;
	}

	moment->rtmpCommandMessage (srv_session, conn, /* TODO Unnecessary? msg_stream_id, */ msg, method_name, amf_decoder);
    }

    return RtmpServer::CommandResult::Success;
}

static Result pauseCmd (void * const /* _client_session */)
{
  // No-op
    logD_ (_func_);
    return Result::Success;
}

static Result resumeCmd (void * const _client_session)
{
//    logD_ (_func_);

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);
    client_session->doResume ();
    return Result::Success;
}

static RtmpServer::Frontend const rtmp_server_frontend = {
    connect,
    startRtmpStreaming,
    startRtmpWatching,
    server_commandMessage,
    pauseCmd,
    resumeCmd
};

Result audioMessage (VideoStream::AudioMessage * const mt_nonnull msg,
		     void                      * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    // We create 'video_stream' in startStreaming()/startWatching(), which is
    // synchronized with autioMessage(). No locking needed.
    if (client_session->video_stream)
	client_session->video_stream->fireAudioMessage (msg);

    return Result::Success;
}

Result videoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
		     void                      * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    // We create 'video_stream' in startStreaming()/startWatching(), which is
    // synchronized with videoMessage(). No locking needed.
    if (client_session->video_stream)
	client_session->video_stream->fireVideoMessage (msg);

    return Result::Success;
}

Result commandMessage (VideoStream::Message * const mt_nonnull msg,
		       Uint32                 const msg_stream_id,
		       AmfEncoding            const amf_encoding,
                       RtmpConnection::ConnectionInfo * const mt_nonnull conn_info,
		       void                 * const _client_session)
{
    logD (mod_rtmp, _func_);

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);
    return client_session->rtmp_server.commandMessage (msg, msg_stream_id, amf_encoding, conn_info);
}

void sendStateChanged (Sender::SendState   const send_state,
		       void              * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    switch (send_state) {
	case Sender::ConnectionReady:
	    logD (framedrop, _func, "ConnectionReady");
#ifdef MOMENT_RTMP__FLOW_CONTROL
	    client_session->mutex.lock ();
	    client_session->overloaded = false;
	    client_session->mutex.unlock ();
#endif
	    break;
	case Sender::ConnectionOverloaded:
	    logD (framedrop, _func, "ConnectionOverloaded");
            // We used to set 'client_session->overloaded' to 'true' here,
            // but this turned out to happen too frequently.
            // Moved that to QueueSoftLimit instead.
	    break;
	case Sender::QueueSoftLimit:
	    logD (framedrop, _func, "QueueSoftLimit");
#ifdef MOMENT_RTMP__FLOW_CONTROL
	    client_session->mutex.lock ();
	    client_session->overloaded = true;
	    client_session->mutex.unlock ();
#endif
	    break;
	case Sender::QueueHardLimit:
	    logE_ (_func, "QueueHardLimit");
            client_session->rtmp_conn->close ();
	    break;
	default:
	    unreachable();
    }
}

void closed (Exception * const exc,
	     void      * const _client_session)
{
    logD (mod_rtmp, _func, "client_session 0x", fmt_hex, (UintPtr) _client_session);

    if (exc)
	logD (mod_rtmp, _func, exc->toString());

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);
    destroyClientSession (client_session);
}

RtmpConnection::Frontend const rtmp_frontend = {
    NULL /* handshakeComplete */,
    commandMessage,
    audioMessage /* audioMessage */,
    videoMessage /* videoMessage */,
    sendStateChanged,
    closed
};

Result clientConnected (RtmpConnection  * const mt_nonnull rtmp_conn,
			IpAddress const &client_addr,
			void            * const /* cb_data */)
{
    logD (mod_rtmp, _func_);
//    logD_ (_func, "--- client_addr: ", client_addr);

    getStat()->addInt (stat_num_sessions, 1);

    Ref<ClientSession> const client_session = grab (new (std::nothrow) ClientSession);
    client_session->client_addr = client_addr;
    client_session->rtmp_conn = rtmp_conn;

    {
	MomentServer * const moment = MomentServer::getInstance();

	ServerThreadContext *thread_ctx =
		moment->getRecorderThreadPool()->grabThreadContext ("flash" /* TODO Configurable prefix */);
	if (thread_ctx) {
	    client_session->recorder_thread_ctx = thread_ctx;
	} else {
	    logE_ (_func, "Couldn't get recorder thread context: ", exc->toString());
	    thread_ctx = moment->getServerApp()->getServerContext()->getMainThreadContext();
	}

	client_session->flv_muxer.setPagePool (moment->getPagePool());

	client_session->recorder.init (thread_ctx, moment->getStorage());
	client_session->recorder.setRecordingLimit (recording_limit);
	client_session->recorder.setMuxer (&client_session->flv_muxer);
	// TODO recorder frontend + error reporting
    }

    client_session->rtmp_server.setFrontend (CbDesc<RtmpServer::Frontend> (
	    &rtmp_server_frontend, client_session, client_session));
    client_session->rtmp_server.setRtmpConnection (rtmp_conn);

    rtmp_conn->setFrontend (CbDesc<RtmpConnection::Frontend> (
	    &rtmp_frontend, client_session, client_session));

    rtmp_conn->startServer ();

    client_session->ref ();
    return Result::Success;
}

RtmpVideoService::Frontend const rtmp_video_service_frontend = {
    clientConnected
};

static void serverDestroy (void * const _rtmp_module)
{
    MomentRtmpModule * const rtmp_module = static_cast <MomentRtmpModule*> (_rtmp_module);

    logH_ (_func_);
    rtmp_module->unref ();
}

static MomentServer::Events const server_events = {
    NULL /* configReload */,
    serverDestroy
};


// ___________________________________ Admin ___________________________________

HttpService::HttpHandler const MomentRtmpModule::admin_http_handler = {
    adminHttpRequest,
    NULL /* httpMessageBody */
};

Result
MomentRtmpModule::adminHttpRequest (HttpRequest   * const mt_nonnull req,
                                    Sender        * const mt_nonnull conn_sender,
                                    Memory const  & /* msg_body */,
                                    void         ** const mt_nonnull /* ret_msg_data */,
                                    void          * const _self)
{
    MomentRtmpModule * const self = static_cast <MomentRtmpModule*> (_self);

    logD_ (_func_);

    MOMENT_SERVER__HEADERS_DATE;

    if (req->getNumPathElems() >= 2
	&& equal (req->getPath (1), "stat"))
    {
        Time const cur_unixtime = getUnixtime();

        PagePool::PageListHead page_list;

        page_pool->printToPages (
                &page_list,
                "<html>"
                "<body>"
                "<p>mod_rtmp stats</p>");

        {
            page_pool->printToPages (
                    &page_list,
                    "<table>");

            self->rtmp_service.rtmpServiceLock ();

            RtmpService::SessionsInfo sinfo_sum;
            RtmpService::SessionInfoIterator iter = self->rtmp_service.getClientSessionsInfo_unlocked (&sinfo_sum);

            page_pool->printToPages (
                    &page_list,
                    "<tr><td>RtmpService: num_session_objects</td><td>", sinfo_sum.num_session_objects, "</td></tr>"
                    "<tr><td>RtmpService: num_valid_sessions</td><td>",  sinfo_sum.num_valid_sessions,  "</td></tr>"
                    "<tr><td>RtmpService sessions:</td></tr>");

            while (!iter.done()) {
                RtmpService::ClientSessionInfo * const sinfo = iter.next ();

                Byte time_buf [unixtimeToString_BufSize];
                Size const time_len = unixtimeToString (Memory::forObject (time_buf), sinfo->creation_unixtime);

                page_pool->printToPages (
                        &page_list,
                        "<tr>"
                        "<td>", sinfo->client_addr, "</td>"
                        "<td>", ConstMemory (time_buf, time_len), "</td>"
                        "<td>", sinfo->last_send_unixtime, "</td>"
                        "<td>", sinfo->last_recv_unixtime, "</td>"
                        "<td>", sinfo->last_play_stream, "</td>"
                        "<td>", sinfo->last_publish_stream, "</td>"
                        "</tr>");
            }

            self->rtmp_service.rtmpServiceUnlock ();

            page_pool->printToPages (
                    &page_list,
                    "</table>");
        }

        {
            self->rtmpt_service.rtmptServiceLock ();

            RtmptService::RtmptSessionsInfo sinfo_sum;
            RtmptService::RtmptSessionInfoIterator sinfo_iter = self->rtmpt_service.getRtmptSessionsInfo_unlocked (&sinfo_sum);

            RtmptService::RtmptConnectionsInfo cinfo_sum;
            RtmptService::RtmptConnectionInfoIterator cinfo_iter = self->rtmpt_service.getRtmptConnectionsInfo_unlocked (&cinfo_sum);

            page_pool->printToPages (
                    &page_list,
                    "<h1>RTMPT stats</h1>"
                    "<table>"
                    "<tr><td>RtmptService: num_session_objects</td><td>",    sinfo_sum.num_session_objects,    "</td></tr>"
                    "<tr><td>RtmptService: num_valid_sessions</td><td>",     sinfo_sum.num_valid_sessions,     "</td></tr>"
                    "<tr><td>RtmptService: num_connection_objects</td><td>", cinfo_sum.num_connection_objects, "</td></tr>"
                    "<tr><td>RtmptService: num_valid_connections</td><td>",  cinfo_sum.num_valid_connections,  "</td></tr>"
                    "</table>"
                    "<h2>RTMPT sessions</h2>"
                    "<table>");

            while (!sinfo_iter.done()) {
                RtmptService::RtmptSessionInfo * const sinfo = sinfo_iter.next ();

                Byte time_buf [unixtimeToString_BufSize];
                Size const time_len = unixtimeToString (Memory::forObject (time_buf), sinfo->creation_unixtime);

                Time idle_time = cur_unixtime - sinfo->last_req_unixtime;
                if (sinfo->last_req_unixtime > cur_unixtime)
                    idle_time = 0;

                page_pool->printToPages (
                        &page_list,
                        "<tr>"
                        "<td>", sinfo->last_client_addr, "</td>"
                        "<td>", ConstMemory (time_buf, time_len), "</td>"
                        "<td>", idle_time, "</td>"
                        "</tr>");
            }

            page_pool->printToPages (
                    &page_list,
                    "</table>"
                    "<h2>RTMPT connections</h2>"
                    "<table>");

            while (!cinfo_iter.done()) {
                RtmptService::RtmptConnectionInfo * const cinfo = cinfo_iter.next ();

                Byte time_buf [unixtimeToString_BufSize];
                Size const time_len = unixtimeToString (Memory::forObject (time_buf), cinfo->creation_unixtime);

                Time idle_time = cur_unixtime - cinfo->last_req_unixtime;
                if (cinfo->last_req_unixtime > cur_unixtime)
                    idle_time = 0;

                page_pool->printToPages (
                        &page_list,
                        "<tr>"
                        "<td>", cinfo->client_addr, "</td>"
                        "<td>", ConstMemory (time_buf, time_len), "</td>"
                        "<td>", idle_time, "</td>"
                        "</tr>");
            }

            page_pool->printToPages (
                    &page_list,
                    "</table>");

            self->rtmpt_service.rtmptServiceUnlock ();
        }

        page_pool->printToPages (
                &page_list,
                "</body>"
                "</html>");

        Size const data_len = PagePool::countPageListDataLen (page_list.first, 0 /* msg_offset */);

        conn_sender->send (
                page_pool,
                false /* do_flush */,
                MOMENT_SERVER__OK_HEADERS ("text/html", data_len),
                "\r\n");
        conn_sender->sendPages (page_pool, page_list.first, true /* do_flush */);

        logA_ ("file 200 ", req->getClientAddress(), " ", req->getRequestLine());
    } else {
	logE_ (_func, "Unknown admin HTTP request: ", req->getFullPath());

	ConstMemory const reply_body = "mod_rtmp: unknown command";
	conn_sender->send (page_pool,
			   true /* do_flush */,
			   MOMENT_SERVER__404_HEADERS (reply_body.len()),
			   "\r\n",
			   reply_body);

	logA_ ("mod_rtmp__admin 404 ", req->getClientAddress(), " ", req->getRequestLine());
    }

    if (!req->getKeepalive())
        conn_sender->closeAfterFlush();

    return Result::Success;
}

// _____________________________________________________________________________


static char const opt_name__ping_timeout[] = "mod_rtmp/ping_timeout";

static Result momentRtmpInit ()
{
    stat_num_sessions = getStat()->createParam ("mod_rtmp/num_sessions",
                                                "Number of active RTMP(T) sessions",
                                                Stat::ParamType_Int64,
                                                0, 0.0);

    moment = MomentServer::getInstance();
    CodeDepRef<ServerApp> const server_app = moment->getServerApp();
    timers = server_app->getServerContext()->getMainThreadContext()->getTimers();
    page_pool = moment->getPagePool();

    MConfig::Config * const config = moment->getConfig();

    // TODO ref/unref
    MomentRtmpModule * const rtmp_module = new (std::nothrow) MomentRtmpModule;
    assert (rtmp_module);
    moment->getEventInformer()->subscribe (CbDesc<MomentServer::Events> (&server_events, rtmp_module, NULL));

    {
	ConstMemory const opt_name = "mod_rtmp/enable";
	MConfig::BooleanValue const enable = config->getBoolean (opt_name);
	if (enable == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return Result::Failure;
	}

	if (enable == MConfig::Boolean_False) {
	    logI_ (_func, "Unrestricted RTMP access module is not enabled. "
		   "Set \"", opt_name, "\" option to \"y\" to enable.");
	    return Result::Failure;
	}
    }

    Uint64 send_delay_millisec = 50;
    {
	ConstMemory const opt_name = "mod_rtmp/send_delay";
	MConfig::GetResult const res = config->getUint64_default (
		opt_name, &send_delay_millisec, send_delay_millisec);
	if (!res) {
	    logE_ (_func, "bad value for ", opt_name);
	} else {
	    logI_ (_func, "RTMP send delay: ", send_delay_millisec, " milliseconds");
	}
    }

    Time rtmp_ping_timeout_millisec;
    {
        Uint64 rtmp_ping_timeout_sec = 5 * 60;
        if (!configGetUint64 (config, opt_name__ping_timeout, &rtmp_ping_timeout_sec, rtmp_ping_timeout_sec))
            return Result::Failure;
        logI_ (_func, opt_name__ping_timeout, ": ", rtmp_ping_timeout_sec);

        rtmp_ping_timeout_millisec = rtmp_ping_timeout_sec * 1000;
    }

    Time rtmpt_session_timeout = 30;
    {
	ConstMemory const opt_name = "mod_rtmp/rtmpt_session_timeout";
	MConfig::GetResult const res = config->getUint64_default (
		opt_name, &rtmpt_session_timeout, rtmpt_session_timeout);
	if (!res)
	    logE_ (_func, "bad value for ", opt_name);
    }
    logI_ (_func, "rtmpt_session_timeout: ", rtmpt_session_timeout);

    bool rtmpt_no_keepalive_conns = false;
    {
	ConstMemory const opt_name = "mod_rtmp/rtmpt_no_keepalive_conns";
	MConfig::BooleanValue const enable = config->getBoolean (opt_name);
	if (enable == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return Result::Failure;
	}

	if (enable == MConfig::Boolean_True)
	    rtmpt_no_keepalive_conns = true;

	logI_ (_func, opt_name, ": ", rtmpt_no_keepalive_conns);
    }

    {
	ConstMemory const opt_name = "mod_rtmp/record_all";
	MConfig::BooleanValue const value = config->getBoolean (opt_name);
	if (value == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name),
		   ", assuming \"", record_all, "\"");
	} else {
	    if (value == MConfig::Boolean_True)
		record_all = true;
	    else
		record_all = false;

	    logI_ (_func, opt_name, ": ", record_all);
	}
    }

    record_path = st_grab (new (std::nothrow) String (
                          config->getString_default ("mod_rtmp/record_path", record_path->mem())));

    {
	ConstMemory const opt_name = "mod_rtmp/record_limit";
	MConfig::GetResult const res = config->getUint64_default (
		opt_name, &recording_limit, recording_limit);
	if (!res)
	    logE_ (_func, "bad value for ", opt_name);

	logI_ (_func, opt_name, ": ", recording_limit);
    }

    bool prechunking_enabled = true;
    {
        ConstMemory const opt_name = "mod_rtmp/prechunking";
        MConfig::BooleanValue const value = config->getBoolean (opt_name);
        if (value == MConfig::Boolean_Invalid) {
            logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name),
                   ", assuming \"", prechunking_enabled, "\"");
        } else {
            if (value == MConfig::Boolean_False)
                prechunking_enabled = false;
            else
                prechunking_enabled = true;

            logI_ (_func, opt_name, ": ", prechunking_enabled);
        }
    }

    {
	ConstMemory const opt_name = "mod_rtmp/audio_waits_video";
	MConfig::BooleanValue const opt_val = config->getBoolean (opt_name);
	if (opt_val == MConfig::Boolean_Invalid)
	    logE_ (_func, "Invalid value for config option ", opt_name);
	else
	if (opt_val == MConfig::Boolean_True)
	    audio_waits_video = true;
	else
	    audio_waits_video = false;

        logI_ (_func, opt_name, ": ", audio_waits_video);
    }

    {
	ConstMemory const opt_name = "mod_rtmp/wait_for_keyframe";
	MConfig::BooleanValue const opt_val = config->getBoolean (opt_name);
	if (opt_val == MConfig::Boolean_Invalid)
	    logE_ (_func, "Invalid value for config option ", opt_name);
	else
	if (opt_val == MConfig::Boolean_False)
	    wait_for_keyframe = false;
	else
	    wait_for_keyframe = true;

        logI_ (_func, opt_name, ": ", wait_for_keyframe);
    }

    {
	ConstMemory const opt_name = "mod_rtmp/start_paused";
	MConfig::BooleanValue const opt_val = config->getBoolean (opt_name);
	if (opt_val == MConfig::Boolean_Invalid)
	    logE_ (_func, "Invalid value for config option ", opt_name);
	else
	if (opt_val == MConfig::Boolean_True)
	    default_start_paused = true;
	else
	    default_start_paused = false;

	logI_ (_func, opt_name, ": ", default_start_paused);
    }

    {
	ConstMemory const opt_name = "mod_rtmp/paused_avc_interframes";
	MConfig::GetResult const res = config->getUint64_default (
		opt_name, &paused_avc_interframes, paused_avc_interframes);
	if (!res)
	    logE_ (_func, "bad value for ", opt_name);

	logI_ (_func, opt_name, ": ", paused_avc_interframes);
    }

    if (MConfig::Section * const modrtmp_section = config->getSection ("mod_rtmp")) {
        MConfig::Section::iter iter (*modrtmp_section);
        while (!modrtmp_section->iter_done (iter)) {
            MConfig::SectionEntry * const sect_entry = modrtmp_section->iter_next (iter);
            if (sect_entry->getType() == MConfig::SectionEntry::Type_Section
                && equal (sect_entry->getName(), "transcode"))
            {
                MConfig::Section * const transcode_section = static_cast <MConfig::Section*> (sect_entry);

                ConstMemory suffix;
                if (MConfig::Option * const opt = transcode_section->getOption ("suffix"))
                    if (MConfig::Value * const val = opt->getValue())
                        suffix = val->mem();

                ConstMemory chain;
                if (MConfig::Option * const opt = transcode_section->getOption ("chain"))
                    if (MConfig::Value * const val = opt->getValue())
                        chain = val->mem();

                Transcoder::TranscodingMode audio_mode = Transcoder::TranscodingMode_On;
                Transcoder::TranscodingMode video_mode = Transcoder::TranscodingMode_On;

                if (MConfig::Option * const opt = transcode_section->getOption ("direct_audio")) {
                    MConfig::BooleanValue const opt_val = opt->getBoolean();
                    if (opt_val == MConfig::Boolean_Invalid)
                        logE_ (_func, "Invalid value for config option direct_audio");
                    else
                    if (opt_val == MConfig::Boolean_True)
                        audio_mode = Transcoder::TranscodingMode_Direct;
                }

                if (MConfig::Option * const opt = transcode_section->getOption ("direct_video")) {
                    MConfig::BooleanValue const opt_val = opt->getBoolean();
                    if (opt_val == MConfig::Boolean_Invalid)
                        logE_ (_func, "Invalid value for config option direct_video");
                    else
                    if (opt_val == MConfig::Boolean_True)
                        video_mode = Transcoder::TranscodingMode_Direct;
                }

                if (MConfig::Option * const opt = transcode_section->getOption ("no_audio")) {
                    MConfig::BooleanValue const opt_val = opt->getBoolean();
                    if (opt_val == MConfig::Boolean_Invalid)
                        logE_ (_func, "Invalid value for config option no_audio");
                    else
                    if (opt_val == MConfig::Boolean_True)
                        audio_mode = Transcoder::TranscodingMode_Off;
                }

                if (MConfig::Option * const opt = transcode_section->getOption ("no_video")) {
                    MConfig::BooleanValue const opt_val = opt->getBoolean();
                    if (opt_val == MConfig::Boolean_Invalid)
                        logE_ (_func, "Invalid value for config option no_video");
                    else
                    if (opt_val == MConfig::Boolean_True)
                        video_mode = Transcoder::TranscodingMode_Off;
                }

                Ref<TranscodeEntry> const transcode_entry = grab (new (std::nothrow) TranscodeEntry);
                transcode_entry->suffix = grab (new (std::nothrow) String (suffix));
                transcode_entry->chain  = grab (new (std::nothrow) String (chain));
                transcode_entry->audio_mode = audio_mode;
                transcode_entry->video_mode = video_mode;

                transcode_list.append (transcode_entry);
            }
        }
    }

    {
	ConstMemory const opt_name = "mod_rtmp/transcode_on_demand";
	MConfig::BooleanValue const opt_val = config->getBoolean (opt_name);
	if (opt_val == MConfig::Boolean_Invalid)
	    logE_ (_func, "Invalid value for config option ", opt_name);
	else
	if (opt_val == MConfig::Boolean_False)
	    transcode_on_demand = false;
	else
	    transcode_on_demand = true;

        logI_ (_func, opt_name, ": ", transcode_on_demand);
    }

    {
	ConstMemory const opt_name = "mod_rtmp/transcode_on_demand_timeout";
	MConfig::GetResult const res = config->getUint64_default (
		opt_name, &transcode_on_demand_timeout_millisec, transcode_on_demand_timeout_millisec);
	if (!res)
	    logE_ (_func, "bad value for ", opt_name);

	logI_ (_func, opt_name, ": ", transcode_on_demand_timeout_millisec);
    }

    Time rtmp_accept_watchdog_timeout_sec = 0;
    {
	ConstMemory const opt_name = "mod_rtmp/rtmp_accept_watchdog_timeout";
	MConfig::GetResult const res = config->getUint64_default (
		opt_name, &rtmp_accept_watchdog_timeout_sec, rtmp_accept_watchdog_timeout_sec);
	if (!res)
	    logE_ (_func, "bad value for ", opt_name);

	logI_ (_func, opt_name, ": ", rtmp_accept_watchdog_timeout_sec);
    }

    {
	rtmp_module->rtmp_service.setFrontend (CbDesc<RtmpVideoService::Frontend> (
		&rtmp_video_service_frontend, NULL, NULL));

	if (!rtmp_module->rtmp_service.init (server_app->getServerContext(),
                                             moment->getPagePool(),
                                             send_delay_millisec,
                                             rtmp_ping_timeout_millisec,
                                             prechunking_enabled,
                                             rtmp_accept_watchdog_timeout_sec))
        {
	    logE_ (_func, "rtmp_service.init() failed: ", exc->toString());
	    return Result::Failure;
	}

	do {
	    ConstMemory const opt_name = "mod_rtmp/rtmp_bind";
	    ConstMemory rtmp_bind = config->getString_default (opt_name, ":1935");

	    logI_ (_func, opt_name, ": ", rtmp_bind);
	    if (!rtmp_bind.isNull ()) {
		IpAddress addr;
		if (!setIpAddress_default (rtmp_bind,
					   ConstMemory() /* default_host */,
					   1935          /* default_port */,
					   true          /* allow_any_host */,
					   &addr))
		{
		    logE_ (_func, "setIpAddress_default() failed (rtmp)");
		    return Result::Failure;
		}

		if (!rtmp_module->rtmp_service.bind (addr)) {
		    logE_ (_func, "rtmp_service.bind() failed: ", exc->toString());
		    break;
		}

		if (!rtmp_module->rtmp_service.start ()) {
		    logE_ (_func, "rtmp_service.start() failed: ", exc->toString());
		    return Result::Failure;
		}
	    } else {
		logI_ (_func, "RTMP service is not bound to any port "
		       "and won't accept any connections. "
		       "Set \"", opt_name, "\" option to bind the service.");
	    }
	} while (0);
    }

    {
	ConstMemory const opt_name = "mod_rtmp/rtmpt_from_http";
	MConfig::BooleanValue const opt_val = config->getBoolean (opt_name);
	if (opt_val == MConfig::Boolean_Invalid)
	    logE_ (_func, "Invalid value for config option ", opt_name);
	else
	if (opt_val != MConfig::Boolean_False)
	    rtmp_module->rtmpt_service.attachToHttpService (moment->getHttpService());
    }

    {
	rtmp_module->rtmpt_service.setFrontend (CbDesc<RtmpVideoService::Frontend> (
		&rtmp_video_service_frontend, NULL, NULL));

	if (!rtmp_module->rtmpt_service.init (
                                 server_app->getServerContext(),
                                 moment->getPagePool(),
                                 // TODO standalone_rtmpt enable/disable from config
                                 true /* enable_standalone_tcp_server */,
                                 rtmp_ping_timeout_millisec,
                                 rtmpt_session_timeout,
                                 // TODO Separate rtmpt_conn_keepalive_timeout
                                 rtmpt_session_timeout,
                                 rtmpt_no_keepalive_conns,
                                 prechunking_enabled))
        {
	    logE_ (_func, "rtmpt_service.init() failed: ", exc->toString());
	    return Result::Failure;
	}

	do {
	    ConstMemory const opt_name = "mod_rtmp/rtmpt_bind";
	    ConstMemory const rtmpt_bind = config->getString_default (opt_name, ":8081");
	    logI_ (_func, opt_name, ": ", rtmpt_bind);
	    if (!rtmpt_bind.isNull ()) {
		IpAddress addr;
		if (!setIpAddress_default (rtmpt_bind,
					   ConstMemory() /* default_host */,
					   8081          /* default_port */,
					   true          /* allow_any_host */,
					   &addr))
		{
		    logE_ (_func, "setIpAddress_default() failed (rtmpt)");
		    return Result::Failure;
		}

		if (!rtmp_module->rtmpt_service.bind (addr)) {
		    logE_ (_func, "rtmpt_service.bind() failed: ", exc->toString());
		    break;
		}

		if (!rtmp_module->rtmpt_service.start ()) {
		    logE_ (_func, "rtmpt_service.start() failed: ", exc->toString());
		    return Result::Failure;
		}
	    } else {
		logI_ (_func, "RTMPT service is not bound to any port "
		       "and won't accept any connections. "
		       "Set \"", opt_name, "\" option to \"y\" to bind the service.");
	    }
	} while (0);

        moment->getAdminHttpService()->addHttpHandler (
                CbDesc<HttpService::HttpHandler> (&MomentRtmpModule::admin_http_handler, rtmp_module, rtmp_module),
                "mod_rtmp");
    }

    {
        Ref<RtmpPushProtocol> const rtmp_push_proto = grab (new (std::nothrow) RtmpPushProtocol);
        rtmp_push_proto->init (moment, rtmp_ping_timeout_millisec);
        moment->addPushProtocol ("rtmp", rtmp_push_proto);
        moment->addPushProtocol ("momentrtmp", rtmp_push_proto);
    }
    {
        Ref<RtmpFetchProtocol> const rtmp_fetch_proto = grab (new (std::nothrow) RtmpFetchProtocol);
        rtmp_fetch_proto->init (moment, rtmp_ping_timeout_millisec);
        moment->addFetchProtocol ("rtmp", rtmp_fetch_proto);
        moment->addFetchProtocol ("momentrtmp", rtmp_fetch_proto);
    }

    return Result::Success;
}

void momentRtmpUnload ()
{
}

} // namespace {}

} // namespace Moment


namespace M {

void libMary_moduleInit ()
{
    logI_ (_func, "Initializing mod_rtmp");

    Moment::momentRtmpInit ();
}

void libMary_moduleUnload()
{
    logI_ (_func, "Unloading mod_rtmp");

    Moment::momentRtmpUnload ();
}

}

