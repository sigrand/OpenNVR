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


#ifndef MOMENT__RTMP_PUSH_PROTOCOL__H__
#define MOMENT__RTMP_PUSH_PROTOCOL__H__


#include <libmary/libmary.h>
#include <moment/moment_server.h>
#include <moment/push_protocol.h>
#include <moment/rtmp_connection.h>


namespace Moment {

using namespace M;

class RtmpPushConnection : public PushConnection,
                           public virtual Object
{
private:
    StateMutex mutex;

    enum ConnectionState {
        ConnectionState_Connect,
        ConnectionState_ConnectSent,
        ConnectionState_CreateStreamSent,
        ConnectionState_PublishSent,
        ConnectionState_Streaming
    };

    class Session : public Object
    {
    public:
        // Must be referenced externally to use.
        RtmpPushConnection *rtmp_push_conn;

        RtmpConnection rtmp_conn;
        TcpConnection tcp_conn;
        DeferredConnectionSender conn_sender;
        ConnectionReceiver conn_receiver;

        // Synchronized by 'rtmp_conn_frontend'.
        ConnectionState conn_state;

        AtomicInt publishing;

        mt_mutex (mutex) PollGroup::PollableKey pollable_key;

         Session ();
        ~Session ();
    };

    mt_const ServerThreadContext *thread_ctx;
    mt_const Timers *timers;
    mt_const PagePool *page_pool;

    mt_const Ref<VideoStream> video_stream;

    mt_const IpAddress server_addr;
    mt_const Ref<String> username;
    mt_const Ref<String> password;
    mt_const Ref<String> app_name;
    mt_const Ref<String> stream_name;
    mt_const Time ping_timeout_millisec;
    mt_const bool momentrtmp_proto;

    mt_mutex (mutex) Ref<Session> cur_session;
    mt_mutex (mutex) Timers::TimerKey reconnect_timer;

    mt_mutex (mutex) void destroySession  (Session * mt_nonnull session);
    mt_mutex (mutex) void startNewSession (Session *old_session);

    mt_mutex (mutex) void setReconnectTimer ();
    mt_mutex (mutex) void deleteReconnectTimer ();

    static void reconnectTimerTick (void *_self);

    void scheduleReconnect (Session *old_session);

  mt_iface (TcpConnection::Frontend)
    static TcpConnection::Frontend const tcp_conn_frontend;

    static void connected (Exception *exc_,
                           void      *_session);
  mt_iface_end

  mt_iface (RtmpConnection::Backend)
    static RtmpConnection::Backend const rtmp_conn_backend;

    static void closeRtmpConn (void *_session);
  mt_iface_end

  mt_iface (VideoStream::FrameSaver::FrameHandler)
    static VideoStream::FrameSaver::FrameHandler const saved_frame_handler;

    static Result savedAudioFrame (VideoStream::AudioMessage * mt_nonnull audio_msg,
                                   void                      *_session);

    static Result savedVideoFrame (VideoStream::VideoMessage * mt_nonnull video_msg,
                                   void                      *_session);
  mt_iface_end

  mt_iface (RtmpConnection::Frontend)
    static RtmpConnection::Frontend const rtmp_conn_frontend;

    static Result handshakeComplete (void *_session);

    static Result commandMessage (VideoStream::Message * mt_nonnull msg,
                                  Uint32                msg_stream_id,
                                  AmfEncoding           amf_encoding,
                                  RtmpConnection::ConnectionInfo * mt_nonnull conn_info,
                                  void                 *_session);

    static void closed (Exception *exc_,
                        void      *_session);
  mt_iface_end

  mt_iface (VideoStream::EventHandler)
    static VideoStream::EventHandler const video_event_handler;

    static void audioMessage (VideoStream::AudioMessage * mt_nonnull msg,
                                void                    *_self);

    static void videoMessage (VideoStream::VideoMessage * mt_nonnull msg,
                              void                      *_self);
  mt_iface_end

public:
    mt_const void init (ServerThreadContext * mt_nonnull _thread_ctx,
                        PagePool            * mt_nonnull _page_pool,
                        VideoStream         *_video_stream,
                        IpAddress            _server_addr,
                        ConstMemory          _username,
                        ConstMemory          _password,
                        ConstMemory          _app_name,
                        ConstMemory          _stream_name,
                        Time                 _ping_timeout_millisec,
                        bool                 _momentrtmp_proto);

     RtmpPushConnection ();
    ~RtmpPushConnection ();
};

class RtmpPushProtocol : public PushProtocol
{
private:
    mt_const Ref<MomentServer> moment;
    mt_const Time ping_timeout_millisec;

public:
  mt_iface (PushProtocol)
    mt_throws Ref<PushConnection> connect (VideoStream * mt_nonnull video_stream,
                                           ConstMemory  uri,
                                           ConstMemory  username,
                                           ConstMemory  password);
  mt_iface_end

    mt_const void init (MomentServer * mt_nonnull moment,
                        Time          ping_timeout_millisec);

    RtmpPushProtocol ();
};

}


#endif /* MOMENT__RTMP_PUSH_PROTOCOL__H__ */

