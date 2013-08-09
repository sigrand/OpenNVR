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


#ifndef MOMENT_NVR__GET_FILE_SESSION__H__
#define MOMENT_NVR__GET_FILE_SESSION__H__


#include <moment/libmoment.h>

#include <moment-nvr/media_reader.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

class GetFileSession : public Object
{
private:
    StateMutex mutex;

public:
    struct Frontend {
#warning TODO implement 'done' cb calls
        void (*done) (Result  res,
                      void   *cb_data);
    };

private:
    enum SessionState {
        SessionState_Header,
        SessionState_Data,
        SessionState_Complete
    };

    mt_const Ref<MomentServer> moment;
    mt_const DataDepRef<PagePool> page_pool;
    mt_const DataDepRef<Sender> sender;
    mt_const Cb<Frontend> frontend;

    mt_const Time start_unixtime_sec;
    mt_const Time duration_sec;

    mt_const bool octet_stream_mime;

    mt_const bool req_is_keepalive;
    mt_const IpAddress req_client_addr;
    mt_const StRef<String> req_request_line;

    // Bytes per second. 0 means no limit.
    mt_const Uint64 first_burst_size;
    mt_const Uint64 bps_limit;
    mt_sync_domain (readTask) Time transfer_start_time_millisec;
    mt_sync_domain (readTask) Uint64 bytes_transferred;

    mt_const DataDepRef<ServerThreadContext> thread_ctx;
    mt_const ServerThreadContext *reader_thread_ctx;

    MediaReader media_reader;
    mt_sync_domain (readTask) Mp4Muxer mp4_muxer;

    mt_sync_domain (readTask) SessionState session_state;
    mt_sync_domain (readTask) bool first_data_read;

    mt_sync_domain (readTask) bool got_last_audio_ts;
    mt_sync_domain (readTask) Time last_audio_ts_nanosec;
    mt_sync_domain (readTask) bool got_last_video_ts;
    mt_sync_domain (readTask) Time last_video_ts_nanosec;

    mt_sync_domain (readTask) Uint64 total_num_frames;
    mt_sync_domain (readTask) Uint64 pass2_num_frames;

    DeferredProcessor::Task read_task;
    DeferredProcessor::Task sender_closed_task;
    DeferredProcessor::Registration deferred_reg;

    mt_mutex (mutex) bool started;

    mt_mutex (mutex) Timers::TimerKey bps_limit_timer;

  mt_iface (MediaReader::ReadFrameBackend)
    static MediaReader::ReadFrameBackend const read_frame_backend;

    mt_sync_domain (readTask) static
            MediaReader::ReadFrameResult audioFrame (VideoStream::AudioMessage * mt_nonnull msg,
                                                     void                      *_self);

    mt_sync_domain (readTask) static
            MediaReader::ReadFrameResult videoFrame (VideoStream::VideoMessage * mt_nonnull msg,
                                                     void                      *_self);
  mt_iface_end

    mt_sync_domain (readTask)
            MediaReader::ReadFrameResult doSendFrame (VideoStream::Message * mt_nonnull msg,
                                                      Time                  last_ts_nanosec);

    static mt_sync_domain (readTask) bool senderClosedTask (void * const _self);

    static mt_sync_domain (readTask) bool readTask (void * const _self);

    static mt_sync_domain (readTask) void bpsLimitTimerTick (void * const _self);

    mt_sync_domain (readTask) void doReadData ();

  mt_iface (Sender::Frontend)
    static Sender::Frontend const sender_frontend;

    static void senderStateChanged (Sender::SendState  send_state,
                                    void              *_self);

    static void senderClosed (Exception *exc,
                              void      *_self);
  mt_iface_end 

public:
    void start ();

    mt_const void init (MomentServer * const mt_nonnull moment,
                        HttpRequest  * const mt_nonnull req,
                        Sender       * const mt_nonnull sender,
                        PagePool     * const page_pool,
                        Vfs          * const vfs,
                        ConstMemory    const stream_name,
                        Time           const start_unixtime_sec,
                        Time           const duration_sec,
                        bool           const octet_stream_mime,
                        CbDesc<Frontend> const &frontend);

     GetFileSession ();
    ~GetFileSession ();
};

}


#endif /* MOMENT_NVR__GET_FILE_SESSION__H__ */

