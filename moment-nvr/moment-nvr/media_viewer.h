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


#ifndef MOMENT_NVR__MEDIA_VIEWER__H__
#define MOMENT_NVR__MEDIA_VIEWER__H__


#include <moment/libmoment.h>

#include <moment-nvr/media_reader.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

class MediaViewer : public Object
{
private:
    StateMutex mutex;

    class Session : public Object
    {
    public:
        StateMutex session_mutex;

        mt_const WeakRef<MediaViewer> weak_media_viewer;
        mt_const Ref<VideoStream> stream;

        MediaReader media_reader;

        mt_mutex (session_mutex) bool watching;
        mt_mutex (session_mutex) StRef<String> stream_name;

        mt_mutex (session_mutex) bool started;
        mt_mutex (session_mutex) GenericInformer::SubscriptionKey stream_sbn;

        // TODO Use sendMoreData as a synchronization domain
        //      Currently, synchronization is not fully correct for Session.
        mt_mutex (session_mutex) bool first_frame;
        mt_mutex (session_mutex) Time first_frame_ts;
        mt_mutex (session_mutex) Time first_frame_srv_time;

        AtomicInt send_blocked;

        mt_mutex (session_mutex) Timers::TimerKey send_timer;

        Session ()
            : media_reader (this /* coderef_container */)
        {}
    };

    mt_const DataDepRef<PagePool> page_pool;
    mt_const DataDepRef<Timers> timers;

    mt_const Ref<Vfs> vfs;

    static MediaReader::ReadFrameResult endFrame (Session              * mt_nonnull session,
                                                  VideoStream::Message * mt_nonnull msg);

  mt_iface (MediaReader::ReadFrameBackend)
    static MediaReader::ReadFrameBackend const read_frame_backend;

    static MediaReader::ReadFrameResult audioFrame (VideoStream::AudioMessage * mt_nonnull msg,
                                                    void                      *_session);

    static MediaReader::ReadFrameResult videoFrame (VideoStream::VideoMessage * mt_nonnull msg,
                                                    void                      *_session);
  mt_iface_end

    void sendMoreData (Session * mt_nonnull session);

    static void sendTimerTick (void *_session);

  mt_iface (VideoStream::EventHandler)
    static VideoStream::EventHandler const stream_handler;

    static void streamNumWatchersChanged (Count  num_watchers,
                                          void  *_session);
  mt_iface_end

    static mt_mutex (Session::session_mutex) void setSendState (Session           * mt_nonnull session,
                                                                Sender::SendState  send_state);

  mt_iface (Sender::Frontend)
    static Sender::Frontend const sender_frontend;

    static void senderStateChanged (Sender::SendState  send_state,
                                    void              *_session);
  mt_iface_end

  mt_iface (MomentServer::ClientHandler)
    static MomentServer::ClientHandler const client_handler;

    static void rtmpClientConnected (MomentServer::ClientSession *client_session,
                                     ConstMemory  app_name,
                                     ConstMemory  full_app_name,
                                     void        *_self);
  mt_iface_end

  mt_iface (MomentServer::ClientSession::Events)
    static MomentServer::ClientSession::Events const client_session_events;

    static void rtmpCommandMessage (RtmpConnection       * mt_nonnull rtmp_conn,
                                    VideoStream::Message * mt_nonnull msg,
                                    ConstMemory           method_name,
                                    AmfDecoder           * mt_nonnull amf_decoder,
                                    void                 *_session);

    static void rtmpClientDisconnected (void *_session);
  mt_iface_end

    struct StreamParams
    {
        Time start_unixtime_sec;

        StreamParams ()
            : start_unixtime_sec (0)
        {}
    };

    static void parseStreamParams_paramCallback (ConstMemory  name,
                                                 ConstMemory  value,
                                                 void        *_stream_params);

    static void parseStreamParams (ConstMemory   stream_name_with_params,
                                   StreamParams * mt_nonnull stream_params);

  mt_iface (MomentServer::ClientSession::Backend)
    static MomentServer::ClientSession::Backend const client_session_backend;

    static bool rtmpStartWatching (ConstMemory       stream_name,
                                   ConstMemory       stream_name_with_params,
                                   IpAddress         client_addr,
                                   CbDesc<MomentServer::StartWatchingCallback> const &cb,
                                   Ref<VideoStream> * mt_nonnull ret_stream,
                                   void             *_session);

    static bool rtmpStartStreaming (ConstMemory    stream_name,
                                    IpAddress      client_addr,
                                    VideoStream   * mt_nonnull stream,
                                    RecordingMode  rec_mode,
                                    CbDesc<MomentServer::StartStreamingCallback> const &cb,
                                    Result        * mt_nonnull ret_res,
                                    void          *_session);
  mt_iface_end

public:
    void init (MomentServer * mt_nonnull moment,
               Vfs          * mt_nonnull vfs);

    MediaViewer ();
};

}


#endif /* MOMENT_NVR__MEDIA_VIEWER__H__ */

