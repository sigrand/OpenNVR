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


#include <moment/util_moment.h>

#include <moment/rtmp_push_protocol.h>


using namespace M;

namespace Moment {

RtmpPushConnection::Session::Session ()
            : rtmp_conn     (this /* coderef_container */),
              tcp_conn      (this /* coderef_container */),
              conn_sender   (this /* coderef_container */),
              conn_receiver (this /* coderef_container */),
              conn_state    (ConnectionState_Connect),
              publishing    (0),
              pollable_key  (NULL)
{
    logD_ (_this_func_);
}

RtmpPushConnection::Session::~Session ()
{
    logD_ (_this_func_);
}

mt_mutex (mutex) void
RtmpPushConnection::destroySession (Session * const mt_nonnull session)
{
    session->rtmp_conn.close_noBackendCb ();

    if (session->pollable_key) {
        thread_ctx->getPollGroup()->removePollable (session->pollable_key);
        session->pollable_key = NULL;
    }
}

mt_mutex (mutex) void
RtmpPushConnection::startNewSession (Session * const old_session)
{
    logD_ (_func_);

    if (old_session) {
        if (old_session != cur_session) {
            logD_ (_func, "session mismatch: 0x", fmt_hex, (UintPtr) old_session, ", 0x", (UintPtr) cur_session.ptr());
            return;
        }

        destroySession (old_session);
    }

    logD_ (_func, "calling deleteReconnectTimer()");
    deleteReconnectTimer ();

    Ref<Session> const session = grab (new (std::nothrow) Session);
    cur_session = session;

    session->rtmp_push_conn = this;

    session->conn_sender.setConnection (&session->tcp_conn);
    session->conn_sender.setQueue (thread_ctx->getDeferredConnectionSenderQueue());
    // RtmpConnection sets sender frontend.

    session->conn_receiver.init (&session->tcp_conn,
                                 thread_ctx->getDeferredProcessor());
    session->conn_receiver.setFrontend (session->rtmp_conn.getReceiverFrontend());

    session->rtmp_conn.init (timers,
                             page_pool,
                             // It is expected that there will be few push RTMP conns.
                             // Using non-zero send delay gives negligible performance
                             // increase in this case.
                             0    /* send_delay_millisec */,
                             ping_timeout_millisec,
                             /* TODO Control prechunking, it's not always desirable here. */
                             true /* prechunking_enabled */,
                             momentrtmp_proto);

    // 'session' is surely referenced when a callback is called, because it serves
    // as a coderef container for 'rtmp_conn'. Same for 'tcp_conn'.
    session->rtmp_conn.setBackend (CbDesc<RtmpConnection::Backend> (&rtmp_conn_backend,
                                                                    session /* cb_data */,
                                                                    this /* coderef_container */));
    session->rtmp_conn.setFrontend (CbDesc<RtmpConnection::Frontend> (&rtmp_conn_frontend,
                                                                      session /* cb_data */,
                                                                      this /* coderef_container */));
    session->rtmp_conn.setSender (&session->conn_sender);

    session->tcp_conn.setFrontend (CbDesc<TcpConnection::Frontend> (&tcp_conn_frontend,
                                                                    session /* cb_data */,
                                                                    this /* coderef_container */));

    if (!session->tcp_conn.open ()) {
        logE_ (_func, "tcp_conn.open() failed: ", exc->toString());
        goto _failure;
    }

    if (!thread_ctx->getPollGroup()->addPollable_beforeConnect (session->tcp_conn.getPollable(),
                                                                &session->pollable_key))
    {
        logE_ (_func, "addPollable() failed: ", exc->toString());
        goto _failure;
    }

    {
        TcpConnection::ConnectResult const connect_res = session->tcp_conn.connect (server_addr);
        if (connect_res == TcpConnection::ConnectResult_Error) {
            logE_ (_func, "Could not connect to server: ", exc->toString());
            goto _failure;
        }

        if (!thread_ctx->getPollGroup()->addPollable_afterConnect (session->tcp_conn.getPollable(),
                                                                   &session->pollable_key))
        {
            logE_ (_func, "addPollable() failed: ", exc->toString());
            goto _failure;
        }

        if (connect_res == TcpConnection::ConnectResult_Connected) {
            session->rtmp_conn.startClient ();
            session->conn_receiver.start ();
        } else
            assert (connect_res == TcpConnection::ConnectResult_InProgress);
    }

    return;

_failure:
    destroySession (session);
    cur_session = NULL;

    setReconnectTimer ();
}

mt_mutex (mutex) void
RtmpPushConnection::setReconnectTimer ()
{
    logD_ (_func_);

    logD_ (_func, "calling deleteReconnectTimer()");
    deleteReconnectTimer ();

    reconnect_timer = timers->addTimer (CbDesc<Timers::TimerCallback> (reconnectTimerTick,
                                                                       this /* cb_data */,
                                                                       this /* coderef_container */),
                                        // TODO Config parameter for the timeout.
                                        1     /* time_seconds */,
                                        false /* periodical */,
                                        false /* auto_delete */);
}

mt_mutex (mutex) void
RtmpPushConnection::deleteReconnectTimer ()
{
    logD_ (_func_);

    if (reconnect_timer) {
        timers->deleteTimer (reconnect_timer);
        reconnect_timer = NULL;
    }
}

void
RtmpPushConnection::reconnectTimerTick (void * const _self)
{
    logD_ (_func_);

    RtmpPushConnection * const self = static_cast <RtmpPushConnection*> (_self);

    self->mutex.lock ();
    logD_ (_func, "calling deleteReconnectTimer()");
    self->deleteReconnectTimer ();

    if (self->cur_session) {
        self->mutex.unlock ();
        return;
    }

    self->startNewSession (NULL /* old_session */);
    self->mutex.unlock ();
}

void
RtmpPushConnection::scheduleReconnect (Session * const old_session)
{
    logD_ (_func, "session 0x", fmt_hex, (UintPtr) old_session);

    mutex.lock ();
    if (old_session != cur_session) {
        logD_ (_func, "session mismatch: 0x", fmt_hex, (UintPtr) old_session, ", 0x", (UintPtr) cur_session.ptr());
        mutex.unlock ();
        return;
    }

    destroySession (old_session);
    cur_session = NULL;

    setReconnectTimer ();

    mutex.unlock ();
}

TcpConnection::Frontend const RtmpPushConnection::tcp_conn_frontend = {
    connected
};

void
RtmpPushConnection::connected (Exception * const exc_,
                               void      * const _session)
{
    logD_ (_func_);

    Session * const session = static_cast <Session*> (_session);
    RtmpPushConnection * const self = session->rtmp_push_conn;

    if (exc_) {
        logE_ (_func, "Could not connect to server: ", exc_->toString());
        self->scheduleReconnect (session);
        return;
    }

    session->rtmp_conn.startClient ();
    session->conn_receiver.start ();
}

RtmpConnection::Backend const RtmpPushConnection::rtmp_conn_backend = {
    closeRtmpConn
};

void
RtmpPushConnection::closeRtmpConn (void * const _session)
{
    logD_ (_func_);

    Session * const session = static_cast <Session*> (_session);
    RtmpPushConnection * const self = session->rtmp_push_conn;

    self->scheduleReconnect (session);
}

RtmpConnection::Frontend const RtmpPushConnection::rtmp_conn_frontend = {
    handshakeComplete,
    commandMessage,
    NULL /* audioMessage */,
    NULL /* videoMessage */,
    NULL /* sendStateChanged */,
    closed
};

Result
RtmpPushConnection::handshakeComplete (void * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    RtmpPushConnection * const self = session->rtmp_push_conn;

    session->conn_state = ConnectionState_ConnectSent;
    session->rtmp_conn.sendConnect (self->app_name->mem());

    return Result::Success;
}

VideoStream::FrameSaver::FrameHandler const RtmpPushConnection::saved_frame_handler = {
    savedAudioFrame,
    savedVideoFrame
};

Result
RtmpPushConnection::savedAudioFrame (VideoStream::AudioMessage * const mt_nonnull audio_msg,
                                     void                      * const _session)
{
    Session * const session = static_cast <Session*> (_session);

//    VideoStream::AudioMessage tmp_audio_msg = *audio_msg;
//    tmp_audio_msg.timestamp = 0;
//
//    session->rtmp_conn.sendAudioMessage (&tmp_audio_msg);

    session->rtmp_conn.sendAudioMessage (audio_msg);
    return Result::Success;
}

Result
RtmpPushConnection::savedVideoFrame (VideoStream::VideoMessage * const mt_nonnull video_msg,
                                     void                      * const _session)
{
    Session * const session = static_cast <Session*> (_session);

//    VideoStream::VideoMessage tmp_video_msg = *video_msg;
//    tmp_video_msg.timestamp = 0;
//
//    session->rtmp_conn.sendVideoMessage (&tmp_video_msg);

    session->rtmp_conn.sendVideoMessage (video_msg);
    return Result::Success;
}

Result
RtmpPushConnection::commandMessage (VideoStream::Message * const mt_nonnull msg,
                                    Uint32                 const /* msg_stream_id */,
                                    AmfEncoding            const /* amf_encoding */,
                                    RtmpConnection::ConnectionInfo * const mt_nonnull /* conn_info */,
                                    void                 * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    RtmpPushConnection * const self = session->rtmp_push_conn;

    if (msg->msg_len == 0)
        return Result::Success;

    PagePool::PageListArray pl_array (msg->page_list.first, msg->msg_len);
    AmfDecoder decoder (AmfEncoding::AMF0, &pl_array, msg->msg_len);

    Byte method_name [256];
    Size method_name_len;
    if (!decoder.decodeString (Memory::forObject (method_name),
                               &method_name_len,
                               NULL /* ret_full_len */))
    {
        logE_ (_func, "Could not decode method name");
        return Result::Failure;
    }

    ConstMemory method (method_name, method_name_len);
    if (!compare (method, "_result")) {
        switch (session->conn_state) {
            case ConnectionState_ConnectSent: {
                session->rtmp_conn.sendCreateStream ();
                session->conn_state = ConnectionState_CreateStreamSent;
            } break;
            case ConnectionState_CreateStreamSent: {
                double stream_id;
                if (!decoder.decodeNumber (&stream_id)) {
                    logE_ (_func, "Could not decode stream_id");
                    return Result::Failure;
                }

                session->rtmp_conn.sendPublish (self->stream_name->mem());

                session->conn_state = ConnectionState_Streaming;

                self->video_stream->lock ();
                self->video_stream->getFrameSaver()->reportSavedFrames (&saved_frame_handler, session);
                session->publishing.set (1);
                self->video_stream->unlock ();
            } break;
            case ConnectionState_PublishSent: {
              // Unused
            } break;
            default:
              // Ignoring
                ;
        }
    } else
    if (!compare (method, "_error")) {
        switch (session->conn_state) {
            case ConnectionState_ConnectSent:
            case ConnectionState_CreateStreamSent:
            case ConnectionState_PublishSent: {
                logE_ (_func, "_error received, returning Failure");
                return Result::Failure;
            } break;
            default:
              // Ignoring
                ;
        }
    } else {
        logW_ (_func, "unknown method: ", method);
    }

    return Result::Success;
}

void
RtmpPushConnection::closed (Exception * const exc_,
                            void      * const /* _session */)
{
    if (exc_) {
        logE_ (_func, exc_->toString());
    } else
        logE_ (_func_);
}

VideoStream::EventHandler const RtmpPushConnection::video_event_handler = {
    audioMessage,
    videoMessage,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    NULL /* numWatchersChanged */
};

void
RtmpPushConnection::audioMessage (VideoStream::AudioMessage * const mt_nonnull msg,
                                  void                      * const _self)
{
    RtmpPushConnection * const self = static_cast <RtmpPushConnection*> (_self);

    self->mutex.lock ();
    Ref<Session> const session = self->cur_session;
    self->mutex.unlock ();

    if (!session)
        return;

    if (session->publishing.get() == 1)
        session->rtmp_conn.sendAudioMessage (msg);
}

void
RtmpPushConnection::videoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
                                  void                      * const _self)
{
    RtmpPushConnection * const self = static_cast <RtmpPushConnection*> (_self);

    self->mutex.lock ();
    Ref<Session> const session = self->cur_session;
    self->mutex.unlock ();

    if (!session)
        return;

    if (session->publishing.get() == 1) {
      // TODO Wait for keyframe. Move keyframe awaiting logics
      //      from mod_rtmp to RtmpConnection.
      //      The trickier part is making this work while
      //      sending saved frames in advance.
        session->rtmp_conn.sendVideoMessage (msg);
    }
}

mt_const void
RtmpPushConnection::init (ServerThreadContext * const mt_nonnull _thread_ctx,
                          PagePool            * const mt_nonnull _page_pool,
                          VideoStream         * const _video_stream,
                          IpAddress             const _server_addr,
                          ConstMemory           const _username,
                          ConstMemory           const _password,
                          ConstMemory           const _app_name,
                          ConstMemory           const _stream_name,
                          Time                  const _ping_timeout_millisec,
                          bool                  const _momentrtmp_proto)
{
    thread_ctx = _thread_ctx;
    timers = thread_ctx->getTimers();
    page_pool = _page_pool;

    video_stream = _video_stream;

    server_addr = _server_addr;
    username = grab (new (std::nothrow) String (_username));
    password = grab (new (std::nothrow) String (_password));
    app_name = grab (new (std::nothrow) String (_app_name));
    stream_name = grab (new (std::nothrow) String (_stream_name));
    ping_timeout_millisec = _ping_timeout_millisec;
    momentrtmp_proto = _momentrtmp_proto;

    mutex.lock ();
    startNewSession (NULL /* old_session */);
    mutex.unlock ();

    video_stream->getEventInformer()->subscribe (
            CbDesc<VideoStream::EventHandler> (&video_event_handler,
                                               this /* cb_data */,
                                               this /* coderef_container */));
}

RtmpPushConnection::RtmpPushConnection ()
    : momentrtmp_proto (false),
      reconnect_timer (NULL)
{
}

RtmpPushConnection::~RtmpPushConnection ()
{
    mutex.lock ();

    logD_ (_func, "calling deleteReconnectTimer()");
    deleteReconnectTimer ();

    if (cur_session) {
        destroySession (cur_session);
        cur_session = NULL;
    }

    mutex.unlock ();
}

mt_throws Ref<PushConnection>
RtmpPushProtocol::connect (VideoStream * const video_stream,
                           ConstMemory   const uri,
                           ConstMemory   const username,
                           ConstMemory   const password)
{
    logD_ (_func, "uri: ", uri);

    IpAddress   server_addr;
    ConstMemory app_name;
    ConstMemory stream_name;
    bool        momentrtmp_proto;
    if (!parseMomentRtmpUri (uri, &server_addr, &app_name, &stream_name, &momentrtmp_proto)) {
        logE_ (_func, "Could not parse uri: ", uri);
        goto _failure;
    }
    logD_ (_func, "app_name: ", app_name, ", stream_name: ", stream_name);

  {
    Ref<RtmpPushConnection> const rtmp_push_conn = grab (new (std::nothrow) RtmpPushConnection);
    rtmp_push_conn->init (moment->getServerApp()->getServerContext()->selectThreadContext(),
                          moment->getPagePool(),
                          video_stream,
                          server_addr,
                          username,
                          password,
                          app_name,
                          stream_name,
                          ping_timeout_millisec,
                          momentrtmp_proto);

    return rtmp_push_conn;
  }

_failure:
    exc_throw (InternalException, InternalException::BadInput);
    return NULL;
}

mt_const void
RtmpPushProtocol::init (MomentServer * const mt_nonnull moment,
                        Time           const ping_timeout_millisec)
{
    this->moment = moment;
    this->ping_timeout_millisec = ping_timeout_millisec;
}

RtmpPushProtocol::RtmpPushProtocol ()
    : ping_timeout_millisec (5 * 60 * 1000)
{
}

}

