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


#ifndef MOMENT__RTMP_FETCH_PROTOCOL__H__
#define MOMENT__RTMP_FETCH_PROTOCOL__H__


#include <libmary/libmary.h>
#include <moment/fetch_protocol.h>
#include <moment/moment_server.h>
#include <moment/rtmp_client.h>


namespace Moment {

using namespace M;

class RtmpFetchConnection : public FetchConnection
{
private:
    StateMutex mutex;

    class Session : public Object
    {
    public:
        WeakRef<RtmpFetchConnection> weak_rtmp_fetch_conn;
        Time connect_begin_time;
        RtmpClient client;
        mt_mutex (RtmpFetchConnection::mutex) bool valid;
        Session () : client (this /* coderef_container */) {}
    };

    mt_const DataDepRef<ServerThreadContext> thread_ctx;
    mt_const DataDepRef<PagePool> page_pool;
    mt_const DataDepRef<Timers>   timers;

    mt_const IpAddress     server_addr;
    mt_const StRef<String> app_name;
    mt_const StRef<String> stream_name;
    mt_const bool          momentrtmp_proto;

    mt_const Time ping_timeout_millisec;

    mt_const Cb<FetchConnection::Frontend> frontend;

    DeferredProcessor::Task disconnected_task;
    DeferredProcessor::Registration deferred_reg;

    mt_mutex (mutex) Ref<Session> cur_session;
    mt_mutex (mutex) Timers::TimerKey reconnect_timer;

    mt_mutex (mutex) Ref<VideoStream> next_stream;

    void startNewSession (VideoStream *stream,
                          bool         start_now);

    static void reconnectTimerTick (void *_self);

    void doDisconnected (Session * mt_nonnull session);

    static bool disconnectedTask (void *_self);

  mt_iface (RtmpClient::Frontend)
    static RtmpClient::Frontend const rtmp_client_frontend;

    static void rtmpClientClosed (void *_session);
  mt_iface_end

public:
    // Must be called only once.
    void start ();

    mt_const void init (ServerThreadContext * mt_nonnull thread_ctx,
                        PagePool            * mt_nonnull page_pool,
                        VideoStream         *stream,
                        IpAddress            server_addr,
                        ConstMemory          app_name,
                        ConstMemory          stream_name,
                        bool                 momentrtmp_proto,
                        Time                 ping_timeout_millisec,
                        CbDesc<FetchConnection::Frontend> const &frontend);

    RtmpFetchConnection ()
        : thread_ctx (this /* coderef_container */),
          page_pool  (this /* coderef_container */),
          timers     (this /* coderef_container */),
          momentrtmp_proto (false),
          ping_timeout_millisec (0)
    {
        disconnected_task.cb = CbDesc<DeferredProcessor::TaskCallback> (disconnectedTask, this, this);
    }

    ~RtmpFetchConnection ()
    {
        deferred_reg.release ();
    }
};

class RtmpFetchProtocol : public FetchProtocol
{
private:
    mt_const Ref<MomentServer> moment;
    mt_const Time ping_timeout_millisec;

public:
  mt_iface (FetchProtocol)
    Ref<FetchConnection> connect (VideoStream *stream,
                                  ConstMemory  uri,
                                  CbDesc<FetchConnection::Frontend> const &frontend);
  mt_iface_end

    mt_const void init (MomentServer * mt_nonnull moment,
                        Time          ping_timeout_millisec);

    RtmpFetchProtocol ();
};

}


#endif /* MOMENT__RTMP_FETCH_PROTOCOL__H__ */

