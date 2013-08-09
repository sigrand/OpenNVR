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


#ifndef MOMENT__RTMP_CLIENT__H__
#define MOMENT__RTMP_CLIENT__H__


#include <libmary/libmary.h>
#include <moment/rtmp_connection.h>


namespace Moment {

using namespace M;

class RtmpClient : public DependentCodeReferenced
{
private:
    StateMutex mutex;

public:
    struct Frontend
    {
        void (*closed) (void *_cb_data);
    };

private:
    enum ConnectionState {
        ConnectionState_Connect,
        ConnectionState_ConnectSent,
        ConnectionState_CreateStreamSent,
        ConnectionState_PlaySent,
        ConnectionState_Streaming
    };

    mt_const DataDepRef<ServerThreadContext> thread_ctx;

    RtmpConnection           rtmp_conn;
    TcpConnection            tcp_conn;
    DeferredConnectionSender conn_sender;
    ConnectionReceiver       conn_receiver;

    mt_const Ref<VideoStream> stream;

    mt_const IpAddress     server_addr;
    mt_const StRef<String> app_name;
    mt_const StRef<String> stream_name;
    mt_const bool          momentrtmp_proto;

    mt_const Cb<Frontend> frontend;

    mt_mutex (mutex) PollGroup::PollableKey pollable_key;

    mt_sync_domain (rtmp_conn_frontend) ConnectionState conn_state;

  mt_iface (TcpConnection::Frontend)
    static TcpConnection::Frontend const tcp_conn_frontend;

    static void connected (Exception *exc_,
                           void      *_rtmp_conn);
  mt_iface_end

  mt_iface (RtmpConnection::Backend)
    static RtmpConnection::Backend const rtmp_conn_backend;

    static void closeRtmpConn (void *cb_data);
  mt_iface_end

  mt_iface (RtmpConnection::Frontend)
    static RtmpConnection::Frontend const rtmp_conn_frontend;

    static mt_sync_domain (rtmp_conn_frontend)
            Result handshakeComplete (void *cb_data);

    static mt_sync_domain (rtmp_conn_frontend)
            Result commandMessage (VideoStream::Message * mt_nonnull msg,
                                   Uint32                msg_stream_id,
                                   AmfEncoding           amf_encoding,
                                   RtmpConnection::ConnectionInfo * mt_nonnull conn_info,
                                   void                 *_self);

    static mt_sync_domain (rtmp_conn_frontend)
            Result audioMessage (VideoStream::AudioMessage * mt_nonnull audio_msg,
                                 void                      *_self);

    static mt_sync_domain (rtmp_conn_frontend)
            Result videoMessage (VideoStream::VideoMessage * mt_nonnull video_msg,
                                 void                      *_self);

    static void closed (Exception *exc_,
                        void      *_self);
  mt_iface_end

public:
    // Should be called only once.
    Result start ();

    mt_const void init (ServerThreadContext * mt_nonnull thread_ctx,
                        PagePool            * mt_nonnull page_pool,
                        VideoStream         *stream,
                        IpAddress            server_addr,
                        ConstMemory          app_name,
                        ConstMemory          stream_name,
                        bool                 momentrtmp_proto,
                        Time                 ping_timeout_millisec,
                        Time                 send_delay_millisec,
                        CbDesc<Frontend> const &frontend);

     RtmpClient (Object *coderef_container);
    ~RtmpClient ();
};

}


#endif /* MOMENT__RTMP_CLIENT__H__ */

