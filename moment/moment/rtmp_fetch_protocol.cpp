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


#include <moment/util_moment.h>

#include <moment/rtmp_fetch_protocol.h>


using namespace M;

namespace Moment {

void
RtmpFetchConnection::startNewSession (VideoStream * const stream,
                                      bool          const start_now)
{
    logD_ (_func_);

    Ref<Session> const session = grab (new (std::nothrow) Session);
    session->valid = true;
    session->weak_rtmp_fetch_conn = this;
    session->connect_begin_time = getTimeMilliseconds();

    session->client.init (thread_ctx,
                          page_pool,
                          stream,
                          server_addr,
                          app_name->mem(),
                          stream_name->mem(),
                          momentrtmp_proto,
                          ping_timeout_millisec,
                          0 /* send_delay_millisec */,
                          CbDesc<RtmpClient::Frontend> (&rtmp_client_frontend, session, this));

    mutex.lock ();
    if (reconnect_timer) {
        timers->deleteTimer (reconnect_timer);
        reconnect_timer = NULL;
    }

    cur_session = session;
    mutex.unlock ();

    if (start_now) {
        if (!session->client.start ())
            deferred_reg.scheduleTask (&disconnected_task, false /* permanent */);
    }
}

void
RtmpFetchConnection::reconnectTimerTick (void * const _self)
{
    RtmpFetchConnection * const self = static_cast <RtmpFetchConnection*> (_self);

    self->mutex.lock ();

    assert (self->reconnect_timer);
    self->timers->deleteTimer (self->reconnect_timer);
    self->reconnect_timer = NULL;

    assert (self->next_stream);
    Ref<VideoStream> const stream = self->next_stream;
    self->next_stream = NULL;

    self->mutex.unlock ();

    self->startNewSession (stream, true /* start_now */);
}

void
RtmpFetchConnection::doDisconnected (Session * const mt_nonnull session)
{
    logD_ (_func_);

    mutex.lock ();
    if (session != cur_session) {
        logD_ (_func, "session mismatch");
        mutex.unlock ();
        return;
    }

    if (!session->valid) {
        logD_ (_func, "invalid session");
        mutex.unlock ();
        return;
    }
    session->valid = false;

    cur_session = NULL;
    mutex.unlock ();

    if (frontend) {
        bool               reconnect = false;
        Time               reconnect_after_millisec = 1000;
        Ref<VideoStream>   new_stream;
        if (!frontend.call (frontend->disconnected,
                            &reconnect,
                            &reconnect_after_millisec,
                            &new_stream))
        {
          // frontend gone
            logD_ (_func, "frontend gone");
            return;
        }

        if (reconnect) {
            Time time_delta = 0;
            if (reconnect_after_millisec) {
                Time const cur_time_millisec = getTimeMilliseconds();
                if (cur_time_millisec > session->connect_begin_time) {
                    Time const elapsed = cur_time_millisec - session->connect_begin_time;
                    if (elapsed < reconnect_after_millisec)
                        time_delta = reconnect_after_millisec - elapsed;
                }
            }

            if (time_delta == 0) {
                startNewSession (new_stream, true /* start_now */);
            } else {
                logD_ (_func, "setting timer: ", time_delta, " millisec");

                mutex.lock ();
                next_stream = new_stream;
                reconnect_timer =
                        timers->addTimer_microseconds (
                                CbDesc<Timers::TimerCallback> (reconnectTimerTick, this, this),
                                time_delta * 1000,
                                false /* periodical */,
                                true  /* auto_delete */,
                                false /* delete_after_tick */);
                mutex.unlock ();
            }
        }
    } else {
        logD_ (_func, "no frontend");
    }
}

bool
RtmpFetchConnection::disconnectedTask (void * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<RtmpFetchConnection> const self = session->weak_rtmp_fetch_conn.getRef();

    self->doDisconnected (session);
    return false /* do not reschedule */;
}

RtmpClient::Frontend const RtmpFetchConnection::rtmp_client_frontend = {
    rtmpClientClosed
};

void
RtmpFetchConnection::rtmpClientClosed (void * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    RtmpFetchConnection * const self = session->weak_rtmp_fetch_conn.getTypedWeakPtr();

    logD_ (_func_);

    self->doDisconnected (session);
}

void
RtmpFetchConnection::start ()
{
    assert (cur_session);
    if (!cur_session->client.start ())
        deferred_reg.scheduleTask (&disconnected_task, false /* permanent */);
}

mt_const void
RtmpFetchConnection::init (ServerThreadContext * const mt_nonnull thread_ctx,
                           PagePool            * const mt_nonnull page_pool,
                           VideoStream         * const stream,
                           IpAddress             const server_addr,
                           ConstMemory           const app_name,
                           ConstMemory           const stream_name,
                           bool                  const momentrtmp_proto,
                           Time                  const ping_timeout_millisec,
                           CbDesc<FetchConnection::Frontend> const &frontend)
{
    this->thread_ctx       = thread_ctx;
    this->timers           = thread_ctx->getTimers();
    this->page_pool        = page_pool;
    this->server_addr      = server_addr;
    this->app_name         = st_grab (new (std::nothrow) String (app_name));
    this->stream_name      = st_grab (new (std::nothrow) String (stream_name));
    this->momentrtmp_proto = momentrtmp_proto;
    this->ping_timeout_millisec = ping_timeout_millisec;
    this->frontend         = frontend;

    deferred_reg.setDeferredProcessor (thread_ctx->getDeferredProcessor());

    startNewSession (stream, false /* start_now */);
}

Ref<FetchConnection>
RtmpFetchProtocol::connect (VideoStream * const stream,
                            ConstMemory   const uri,
                            CbDesc<FetchConnection::Frontend> const &frontend)
{
    IpAddress   server_addr;
    ConstMemory app_name;
    ConstMemory stream_name;
    bool        momentrtmp_proto;
    if (!parseMomentRtmpUri (uri, &server_addr, &app_name, &stream_name, &momentrtmp_proto)) {
        logE_ (_func, "Could not parse uri: ", uri);
        return NULL;
    }

    logD_ (_func, "server_addr: ", server_addr, ", "
           "app_name: ", app_name, ", stream_name: ", stream_name, ", "
           "momentrtmp_proto: ", momentrtmp_proto, ", ping_timeout_millisec: ", ping_timeout_millisec);

    Ref<RtmpFetchConnection> const rtmp_fetch_conn = grab (new (std::nothrow) RtmpFetchConnection);
    rtmp_fetch_conn->init (moment->getServerApp()->getServerContext()->selectThreadContext(),
                           moment->getPagePool(),
                           stream,
                           server_addr,
                           app_name,
                           stream_name,
                           momentrtmp_proto,
                           ping_timeout_millisec,
                           frontend);
    rtmp_fetch_conn->start ();

    return rtmp_fetch_conn;
}

mt_const void
RtmpFetchProtocol::init (MomentServer * const mt_nonnull moment,
                         Time           const ping_timeout_millisec)
{
    this->moment = moment;
    this->ping_timeout_millisec = ping_timeout_millisec;
}

RtmpFetchProtocol::RtmpFetchProtocol ()
    : ping_timeout_millisec (0)
{}

}

