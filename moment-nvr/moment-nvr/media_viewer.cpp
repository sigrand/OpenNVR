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


#include <moment-nvr/types.h>

#include <moment-nvr/media_viewer.h>


using namespace M;
using namespace Moment;

namespace MomentNvr {

static LogGroup libMary_logGroup_viewer ("mod_nvr.media_viewer", LogLevel::I);

MediaReader::ReadFrameResult
MediaViewer::endFrame (Session              * const mt_nonnull session,
                       VideoStream::Message * const mt_nonnull msg)
{
    if (session->send_blocked.get()) {
        logD (viewer, _func, "send_blocked");
        return MediaReader::ReadFrameResult_BurstLimit;
    }

    Time const burst_high_mark = 6000000000 /* 6 sec */;

    if (session->first_frame) {
        session->first_frame = false;
        session->first_frame_ts = msg->timestamp_nanosec;
        session->first_frame_srv_time = getTimeMicroseconds() * 1000;
    }

    if (msg->timestamp_nanosec >= session->first_frame_ts) {
        Time const srv_time = getTimeMicroseconds() * 1000;
        if (srv_time >= session->first_frame_srv_time) {
            Time const ts_delta = msg->timestamp_nanosec - session->first_frame_ts;
            Time const srv_delta = srv_time - session->first_frame_srv_time;

//            logD_ (_func, "ts_delta: ", ts_delta, ", "
//                   "srv_delta: ", srv_delta, ", burst_high_mark: ", burst_high_mark);

            if (ts_delta >= srv_delta
                && ts_delta - srv_delta >= burst_high_mark)
            {
                logD (viewer, _func, "BurstLimit: ts_delta: ", ts_delta, ", "
                      "srv_delta: ", srv_delta, ", burst_high_mark: ", burst_high_mark);
                return MediaReader::ReadFrameResult_BurstLimit;
            }
        }
    }

    return MediaReader::ReadFrameResult_Success;
}

MediaReader::ReadFrameBackend const MediaViewer::read_frame_backend = {
    audioFrame,
    videoFrame
};

MediaReader::ReadFrameResult
MediaViewer::audioFrame (VideoStream::AudioMessage * const mt_nonnull msg,
                         void                      * const _session)
{
//    logD_ (_func, "ts ", msg->timestamp_nanosec, " audio ", msg->frame_type);

    Session * const session = static_cast <Session*> (_session);
    session->stream->fireAudioMessage (msg);

    return endFrame (session, msg);
}

MediaReader::ReadFrameResult
MediaViewer::videoFrame (VideoStream::VideoMessage * const mt_nonnull msg,
                         void                      * const _session)
{
//    logD_ (_func, "ts ", msg->timestamp_nanosec, " video ", msg->frame_type);

    Session * const session = static_cast <Session*> (_session);
    session->stream->fireVideoMessage (msg);

    return endFrame (session, msg);
}

void
MediaViewer::sendMoreData (Session * const mt_nonnull session)
{
    if (session->send_blocked.get()) {
        logD (viewer, _func, "send_blocked");
        return;
    }

    for (;;) {
        MediaReader::ReadFrameResult const res =
                session->media_reader.readMoreData (&read_frame_backend, session);
        if (res == MediaReader::ReadFrameResult_Failure) {
            logD_ (_func, "ReadFrameResult_Failure");
            return;
        }

        if (res == MediaReader::ReadFrameResult_BurstLimit) {
            logD (viewer, _func, "ReadFrameResult_BurstLimit");
            return;
        }

        if (res == MediaReader::ReadFrameResult_NoData) {
            logD (viewer, _func, "ReadFrameResult_NoData");
            return;
        }

        if (res == MediaReader::ReadFrameResult_Finish) {
            logD (viewer, _func, "ReadFrameResult_Finish");
            return;
        }

        assert (res == MediaReader::ReadFrameResult_Success);
    }
}

void
MediaViewer::sendTimerTick (void * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<MediaViewer> const self = session->weak_media_viewer.getRef ();
    if (!self)
        return;

    self->sendMoreData (session);
}

VideoStream::EventHandler const MediaViewer::stream_handler = {
    NULL /* audioMessage */,
    NULL /* videoMessage */,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    streamNumWatchersChanged
};

void
MediaViewer::streamNumWatchersChanged (Count   const num_watchers,
                                       void  * const _session)
{
    logD (viewer, _func, "session 0x", fmt_hex, (UintPtr) _session, ", "
          "num_watchers: ", fmt_def, num_watchers);

    Session * const session = static_cast <Session*> (_session);
    Ref<MediaViewer> const self = session->weak_media_viewer.getRef ();
    if (!self)
        return;

    session->session_mutex.lock ();
    if (session->started) {
        return;
    }
    session->started = true;
    assert (session->stream_sbn);
    session->stream->getEventInformer()->unsubscribe (session->stream_sbn);
    session->stream_sbn = NULL;

    self->sendMoreData (session);
    session->send_timer =
            self->timers->addTimer (
                    CbDesc<Timers::TimerCallback> (sendTimerTick, session, session),
                    1    /* time_seconds */,
                    true /* periodical */,
                    true /* auto_delete */);

    session->session_mutex.unlock ();
}

Sender::Frontend const MediaViewer::sender_frontend = {
    senderStateChanged,
    NULL /* closed */
};

mt_mutex (Session::session_mutex) void
MediaViewer::setSendState (Session           * const mt_nonnull session,
                           Sender::SendState   const send_state)
{
    session->send_blocked.set (send_state != Sender::SendState::ConnectionReady ? 1 : 0);
}

void
MediaViewer::senderStateChanged (Sender::SendState   const send_state,
                                 void              * const _session)
{
    Session * const session = static_cast <Session*> (_session);

    session->session_mutex.lock ();
    setSendState (session, send_state);
    session->session_mutex.unlock ();
}

MomentServer::ClientHandler const MediaViewer::client_handler = {
    rtmpClientConnected
};

void
MediaViewer::rtmpClientConnected (MomentServer::ClientSession * const client_session,
                                  ConstMemory   const app_name,
                                  ConstMemory   const full_app_name,
                                  void        * const _self)
{
    MediaViewer * const self = static_cast <MediaViewer*> (_self);

    Ref<Session> const session = grab (new (std::nothrow) Session);
    session->weak_media_viewer = self;
    session->watching = false;
    session->started = false;

    session->first_frame = true;
    session->first_frame_ts = 0;
    session->first_frame_srv_time = 0;

    session->send_blocked.set (0);

    logD (viewer, _func, "session 0x", fmt_hex, (UintPtr) session.ptr(), ", "
          "app_name: ", app_name, ", full_app_name: ", full_app_name);

    self->mutex.lock ();

    CodeDepRef<RtmpConnection> const rtmp_conn = client_session->getRtmpConnection();
    if (rtmp_conn) {
        Sender * const sender = rtmp_conn->getSender();
        sender->lock ();
        setSendState (session, sender->getSendState_unlocked());
        sender->getEventInformer()->subscribe_unlocked (
                CbDesc<Sender::Frontend> (&sender_frontend, session, session));
        sender->unlock ();
    }

    // TODO isConnected - ? (use return value?)
    client_session->isConnected_subscribe (
            // IMPORTANT: Note that we use 'session' as ref_data here.
            CbDesc<MomentServer::ClientSession::Events> (&client_session_events, session, session, session));
    client_session->setBackend (
            CbDesc<MomentServer::ClientSession::Backend> (&client_session_backend, session, session));
    self->mutex.unlock ();
}

MomentServer::ClientSession::Events const MediaViewer::client_session_events = {
    rtmpCommandMessage,
    rtmpClientDisconnected
};

void
MediaViewer::rtmpCommandMessage (RtmpConnection       * const mt_nonnull rtmp_conn,
                                 VideoStream::Message * const mt_nonnull msg,
                                 ConstMemory            const method_name,
                                 AmfDecoder           * const mt_nonnull amf_decoder,
                                 void                 * const _session)
{
    logD (viewer, _func, "session: 0x", fmt_hex, (UintPtr) _session);
}

void
MediaViewer::rtmpClientDisconnected (void * const _session)
{
    logD (viewer, _func, "session 0x", fmt_hex, (UintPtr) _session);

    Session * const session = static_cast <Session*> (_session);
    Ref<MediaViewer> const self = session->weak_media_viewer.getRef ();
    if (!self)
        return;

    session->session_mutex.lock ();

    if (session->send_timer) {
        self->timers->deleteTimer (session->send_timer);
        session->send_timer = NULL;
    }

    session->session_mutex.unlock ();

    self->mutex.lock ();
    // TODO Release the session
    self->mutex.unlock ();
}

MomentServer::ClientSession::Backend const MediaViewer::client_session_backend = {
    rtmpStartWatching,
    rtmpStartStreaming
};

void
MediaViewer::parseStreamParams_paramCallback (ConstMemory   const name,
                                              ConstMemory   const value,
                                              void        * const _stream_params)
{
    StreamParams * const stream_params = static_cast <StreamParams*> (_stream_params);

    logD (viewer, _func, "name: ", name, ", value: ", value);

    if (equal (name, "start")) {
        Time start_unixtime_sec = 0;
        if (strToUint64_safe (value, &start_unixtime_sec, 10 /* base */)) {
            stream_params->start_unixtime_sec = start_unixtime_sec;
        } else {
            logE_ (_func, "bad \"start\" stream param: ", value);
        }
    }
}

void
MediaViewer::parseStreamParams (ConstMemory    const stream_name_with_params,
                                StreamParams * const mt_nonnull stream_params)
{
    ConstMemory const stream_name = stream_name_with_params;

    Byte const * const name_sep = (Byte const *) memchr (stream_name.mem(), '?', stream_name.len());
    if (name_sep) {
        ConstMemory const params_mem = stream_name.region (name_sep + 1 - stream_name.mem());
        logD (viewer, _func, "parameters: ", params_mem);
        parseHttpParameters (params_mem,
                             parseStreamParams_paramCallback,
                             stream_params);
    }
}

bool
MediaViewer::rtmpStartWatching (ConstMemory        const stream_name,
                                ConstMemory        const stream_name_with_params,
                                IpAddress          const /* client_addr */,
                                CbDesc<MomentServer::StartWatchingCallback> const & /* cb */,
                                Ref<VideoStream> * const mt_nonnull ret_stream,
                                void             * const _session)
{
    logD (viewer, _func, "session 0x", fmt_hex, (UintPtr) _session, ", stream_name: ", stream_name_with_params);

    Session * const session = static_cast <Session*> (_session);

    Ref<VideoStream> const stream = grab (new (std::nothrow) VideoStream);
    Ref<MediaViewer> const self = session->weak_media_viewer.getRef ();
    if (!self) {
        *ret_stream = NULL;
        return true;
    }

    StreamParams stream_params;
    parseStreamParams (stream_name_with_params, &stream_params);
    logD (viewer, _func, "start_unixtime_sec: ", stream_params.start_unixtime_sec, ", getUnixtime(): ", getUnixtime());

    session->session_mutex.lock ();
    if (session->watching) {
        session->session_mutex.unlock ();
        logE_ (_func, "session 0x", fmt_hex, (UintPtr) _session, ": already watching");
        *ret_stream = NULL;
        return true;
    }
    session->watching = true;

    session->stream = stream;
    session->stream_name = st_grab (new (std::nothrow) String (stream_name));

    session->stream_sbn = stream->getEventInformer()->subscribe (
            CbDesc<VideoStream::EventHandler> (&stream_handler, session, session));

    session->media_reader.init (self->page_pool,
                                self->vfs,
                                stream_name,
                                stream_params.start_unixtime_sec,
                                0 /* 1 << 23 */ /* 8 Mb */ /* burst_size_limit */ /* TODO Config parameter */);

    session->session_mutex.unlock ();

    *ret_stream = stream;
    return true;
}

bool
MediaViewer::rtmpStartStreaming (ConstMemory     const stream_name,
                                 IpAddress       const client_addr,
                                 VideoStream   * const mt_nonnull stream,
                                 RecordingMode   const rec_mode,
                                 CbDesc<MomentServer::StartStreamingCallback> const &cb,
                                 Result        * const mt_nonnull ret_res,
                                 void          * const _session)
{
    logD (viewer, _func, "session 0x", fmt_hex, (UintPtr) _session, ", stream_name: ", stream_name);
    *ret_res = Result::Failure;
    return true;
}

void
MediaViewer::init (MomentServer * const mt_nonnull moment,
                   Vfs          * const mt_nonnull vfs)
{
    this->vfs = vfs;

    page_pool = moment->getPagePool();

    // TODO Use timers from multiple threads for sendDataTimerTick().
    timers = moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers();

    moment->addClientHandler (CbDesc<MomentServer::ClientHandler> (&client_handler, this, this),
                              "nvr");
}

MediaViewer::MediaViewer ()
    : page_pool (this /* coderef_container */),
      timers    (this /* coderef_container */)
{
}

}

