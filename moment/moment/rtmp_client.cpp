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


#include <moment/rtmp_client.h>


using namespace M;

namespace Moment {

TcpConnection::Frontend const RtmpClient::tcp_conn_frontend = {
    connected
};

void
RtmpClient::connected (Exception * const exc_,
		       void      * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    if (exc_) {
        if (self->frontend)
            self->frontend.call (self->frontend->closed);
        return;
    }

    logD_ (_func, "connected successfully");

    self->rtmp_conn.startClient ();
    self->conn_receiver.start ();
}

RtmpConnection::Backend const RtmpClient::rtmp_conn_backend = {
    closeRtmpConn
};

void
RtmpClient::closeRtmpConn (void * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    logD_ (_func_);

    if (self->frontend)
        self->frontend.call (self->frontend->closed);
}

RtmpConnection::Frontend const RtmpClient::rtmp_conn_frontend = {
    handshakeComplete,
    commandMessage,
    audioMessage,
    videoMessage,
    NULL /* sendStateChanged */,
    closed
};

mt_sync_domain (rtmp_conn_frontend) Result
RtmpClient::handshakeComplete (void * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    logD_ (_func_);

    self->conn_state = ConnectionState_ConnectSent;
    self->rtmp_conn.sendConnect (self->app_name->mem());

    return Result::Success;
}

mt_sync_domain (rtmp_conn_frontend) Result
RtmpClient::commandMessage (VideoStream::Message   * const mt_nonnull msg,
                            Uint32                   const /* msg_stream_id */,
                            AmfEncoding              const /* amf_encoding */,
                            RtmpConnection::ConnectionInfo * const mt_nonnull /* conn_info */,
                            void                   * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

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
	switch (self->conn_state) {
	    case ConnectionState_ConnectSent: {
		self->rtmp_conn.sendCreateStream ();
		self->conn_state = ConnectionState_CreateStreamSent;
	    } break;
	    case ConnectionState_CreateStreamSent: {
		double stream_id;
		if (!decoder.decodeNumber (&stream_id)) {
		    logE_ (_func, "Could not decode stream_id");
		    return Result::Failure;
		}

#if 0
// publish
                    self->rtmp_conn.sendPublish (options.channel->mem());

                    Ref<VideoStream> const video_stream = grab (new (std::nothrow) VideoStream);
                    video_stream->getEventInformer()->subscribe (
                            CbDesc<VideoStream::EventHandler> (&gen_stream_handler,
                                                               self,
                                                               self->getCoderefContainer()));
#endif

                self->rtmp_conn.sendPlay (self->stream_name->mem());
		self->conn_state = ConnectionState_Streaming;
	    } break;
	    case ConnectionState_PlaySent: {
		// Unused
	    } break;
	    default:
	      // Ignoring
		;
	}
    } else
    if (!compare (method, "_error")) {
	switch (self->conn_state) {
	    case ConnectionState_ConnectSent:
	    case ConnectionState_CreateStreamSent:
	    case ConnectionState_PlaySent: {
		logE_ (_func, "_error received, returning Failure");
		return Result::Failure;
	    } break;
	    default:
	      // Ignoring
		;
	}
    } else
    if (!compare (method, "onMetaData")) {
      // No-op
    } else
    if (!compare (method, "onStatus")) {
      // No-op
    } else
    if (equal (method, "|RtmpSampleAccess")) {
      // No-op
    } else {
        if (logLevelOn_ (LogLevel::Warning)) {
            logLock ();
            logW_unlocked_ (_func, "unknown method: ", method);
            PagePool::dumpPages (logs, &msg->page_list);
            logUnlock ();
        }
    }

    return Result::Success;
}

mt_sync_domain (rtmp_conn_frontend) Result
RtmpClient::audioMessage (VideoStream::AudioMessage * const mt_nonnull audio_msg,
			  void                      * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    if (self->stream)
        self->stream->fireAudioMessage (audio_msg);

    return Result::Success;
}

mt_sync_domain (rtmp_conn_frontend) Result
RtmpClient::videoMessage (VideoStream::VideoMessage * const mt_nonnull video_msg,
			  void                      * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    if (self->stream)
        self->stream->fireVideoMessage (video_msg);

    return Result::Success;
}

void
RtmpClient::closed (Exception * const exc_,
		    void      * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    if (exc_)
	logD_ (_func, exc_->toString());
    else
	logD_ (_func_);

    if (self->frontend)
        self->frontend.call (self->frontend->closed);
}

Result
RtmpClient::start ()
{
    if (!tcp_conn.open ()) {
        logE_ (_func, "tcp_conn.open() failed: ", exc->toString());
        return Result::Failure;
    }

    mutex.lock ();

    if (!thread_ctx->getPollGroup()->addPollable_beforeConnect (tcp_conn.getPollable(),
                                                                &pollable_key))
    {
        mutex.unlock ();
        logE_ (_func, "addPollable() failed: ", exc->toString());
        return Result::Failure;
    }

    TcpConnection::ConnectResult const connect_res = tcp_conn.connect (server_addr);
    if (connect_res == TcpConnection::ConnectResult_Error) {
        if (pollable_key) {
            thread_ctx->getPollGroup()->removePollable (pollable_key);
            pollable_key = NULL;
        }
        mutex.unlock ();
	logE_ (_func, "tcp_conn.connect() failed: ", exc->toString());
	return Result::Failure;
    }

    if (!thread_ctx->getPollGroup()->addPollable_afterConnect (tcp_conn.getPollable(),
                                                               &pollable_key))
    {
        mutex.unlock ();
        logE_ (_func, "addPollable() failed: ", exc->toString());
        return Result::Failure;
    }

    mutex.unlock ();

    if (connect_res == TcpConnection::ConnectResult_Connected) {
        rtmp_conn.startClient ();
        conn_receiver.start ();
    } else
        assert (connect_res == TcpConnection::ConnectResult_InProgress);

    return Result::Success;
}

mt_const void
RtmpClient::init (ServerThreadContext * const mt_nonnull thread_ctx,
                  PagePool            * const mt_nonnull page_pool,
                  VideoStream         * const stream,
                  IpAddress             const server_addr,
                  ConstMemory           const app_name,
                  ConstMemory           const stream_name,
                  bool                  const momentrtmp_proto,
                  Time                  const ping_timeout_millisec,
                  Time                  const send_delay_millisec,
                  CbDesc<Frontend> const &frontend)
{
    this->thread_ctx       = thread_ctx;
    this->stream           = stream;
    this->server_addr      = server_addr;
    this->app_name         = st_grab (new (std::nothrow) String (app_name));
    this->stream_name      = st_grab (new (std::nothrow) String (stream_name));
    this->momentrtmp_proto = momentrtmp_proto;
    this->frontend         = frontend;

    conn_sender.setConnection (&tcp_conn);
    conn_sender.setQueue (thread_ctx->getDeferredConnectionSenderQueue());

    conn_receiver.init (&tcp_conn, thread_ctx->getDeferredProcessor());
    conn_receiver.setFrontend (rtmp_conn.getReceiverFrontend());

    rtmp_conn.setBackend  (CbDesc<RtmpConnection::Backend>  (&rtmp_conn_backend,  this, getCoderefContainer()));
    rtmp_conn.setFrontend (CbDesc<RtmpConnection::Frontend> (&rtmp_conn_frontend, this, getCoderefContainer()));
    rtmp_conn.setSender   (&conn_sender);

    tcp_conn.setFrontend (CbDesc<TcpConnection::Frontend> (&tcp_conn_frontend, this, getCoderefContainer()));

    rtmp_conn.init (thread_ctx->getTimers(),
                    page_pool,
                    send_delay_millisec,
                    ping_timeout_millisec,
                    true  /* prechunking_enabled */,
                    momentrtmp_proto);
}

RtmpClient::RtmpClient (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      thread_ctx       (coderef_container),
      rtmp_conn        (coderef_container),
      tcp_conn         (coderef_container),
      conn_sender      (coderef_container),
      conn_receiver    (coderef_container),
      momentrtmp_proto (false),
      conn_state       (ConnectionState_Connect)
{
}

RtmpClient::~RtmpClient ()
{
    mutex.lock ();
    thread_ctx->getPollGroup()->removePollable (pollable_key);
    mutex.unlock ();
}

}

