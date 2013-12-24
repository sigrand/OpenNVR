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


#include <moment-ffmpeg/get_file_session.h>
#include <moment-ffmpeg/time_checker.h>
#include <fstream>


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static LogGroup libMary_logGroup_getfile ("mod_ffmpeg.get_file", LogLevel::D);

MediaReader::ReadFrameBackend const GetFileSession::read_frame_backend = {
    audioFrame,
    videoFrame
};

mt_sync_domain (readTask) MediaReader::ReadFrameResult
GetFileSession::audioFrame (VideoStream::AudioMessage * const mt_nonnull msg,
                            void                      * const _self)
{
    logD_(_func_, "&&&& audioFrame BEGIN");
    GetFileSession * const self = static_cast <GetFileSession*> (_self);
    assert (msg->prechunk_size == 0);

//    logD (getfile, _func, " ts ", msg->timestamp_nanosec, " ", msg->frame_type);

//    if (self->session_state == SessionState_Header) {
//        if (msg->timestamp_nanosec > (self->start_unixtime_sec + self->duration_sec) * 1000000000) {
//            logD (getfile, _func, "Finish");
//            return MediaReader::ReadFrameResult_Finish;
//        }

//        if (msg->frame_type == VideoStream::AudioFrameType::AacSequenceHeader) {
//            self->mp4_muxer.pass1_aacSequenceHeader (msg->page_pool,
//                                                     msg->page_list.first,
//                                                     msg->msg_offset,
//                                                     msg->msg_len);
//        } else
//        if (msg->frame_type.isAudioData()) {
//            self->last_audio_ts_nanosec = msg->timestamp_nanosec;
//            self->got_last_audio_ts = true;

//            self->mp4_muxer.pass1_frame (Mp4Muxer::FrameType_Audio,
//                                         msg->timestamp_nanosec,
//                                         msg->msg_len,
//                                         false /* is_sync_sample */);

//            ++self->total_num_frames;
//        }
//    } else {
//        if (msg->frame_type.isAudioData()) {
//            if (!self->got_last_audio_ts) {
//                logD (getfile, _func, "Finish");
//                return MediaReader::ReadFrameResult_Finish;
//            }

//            return self->doSendFrame (msg, self->last_audio_ts_nanosec);
//        }
//    }

//    if (self->session_state == SessionState_Header)
//    {
//        if (msg->frame_type.isAudioData())
//        {
//            //self->last_audio_ts_nanosec = msg->timestamp_nanosec;
//            self->got_last_audio_ts = true;

//            ++self->total_num_frames;
//        }
//    }
//    else
    {
//        if (msg->frame_type.isAudioData())
//        {
//            if (!self->got_last_audio_ts)
//            {
//                logD_(_func_, "Finish");
//                return MediaReader::ReadFrameResult_Finish;
//            }
logD_(_func_, "&&&& audioFrame END doSendFrame");

            return self->doSendFrame (msg, self->last_audio_ts_nanosec);
//        }
    }

    return MediaReader::ReadFrameResult_Success;
}

mt_sync_domain (readTask) MediaReader::ReadFrameResult
GetFileSession::videoFrame (VideoStream::VideoMessage * const mt_nonnull msg,
                            void                      * const _self)
{
    logD_(_func_, "&&&& videoFrame BEGIN");
    GetFileSession * const self = static_cast <GetFileSession*> (_self);
    assert (msg->prechunk_size == 0);

    logD (getfile, _func, (self->session_state == SessionState_Header ? "HEADER" : "DATA"),
           " ts ", msg->timestamp_nanosec, " ", msg->frame_type, " len ", msg->msg_len);

    assert (self->session_state != SessionState_Complete);

//    if (self->session_state == SessionState_Header) {
//        logD (getfile, _func, "self->start_unixtime_sec: ", self->start_unixtime_sec, ", self->duration_sec: ", self->duration_sec);
//        if (msg->timestamp_nanosec > (self->start_unixtime_sec + self->duration_sec) * 1000000000) {
//            logD (getfile, _func, "Finish");
//            return MediaReader::ReadFrameResult_Finish;
//        }

//        if (msg->frame_type == VideoStream::VideoFrameType::AvcSequenceHeader) {
//            self->mp4_muxer.pass1_avcSequenceHeader (msg->page_pool,
//                                                     msg->page_list.first,
//                                                     msg->msg_offset,
//                                                     msg->msg_len);
//        } else
//        if (msg->frame_type.isVideoData()) {
//            self->last_video_ts_nanosec = msg->timestamp_nanosec;
//            self->got_last_video_ts = true;

//            self->mp4_muxer.pass1_frame (Mp4Muxer::FrameType_Video,
//                                         msg->timestamp_nanosec,
//                                         msg->msg_len,
//                                         msg->frame_type.isKeyFrame());

//            ++self->total_num_frames;
//        }
//    } else {
//        if (msg->frame_type.isVideoData()) {
//            if (!self->got_last_video_ts) {
//                logD (getfile, _func, "Finish");
//                return MediaReader::ReadFrameResult_Finish;
//            }

//            return self->doSendFrame (msg, self->last_video_ts_nanosec);
//        }
//    }
//    return MediaReader::ReadFrameResult_Success;

//    if (self->session_state == SessionState_Header)
//    {
//        //self->last_video_ts_nanosec = msg->timestamp_nanosec;
//        self->got_last_video_ts = true;

//        ++self->total_num_frames;

//bytes_transferred += msg->msg_len;
//    }
//    else
    {
//        if (msg->frame_type.isVideoData())
//        {
//            if (!self->got_last_video_ts)
//            {
//                logD_(_func_, "Finish");
//                return MediaReader::ReadFrameResult_Finish;
//            }
logD_(_func_, "&&&& videoFrame END doSendFrame");
            return self->doSendFrame (msg, self->last_video_ts_nanosec /*useless param*/);
//        }
    }

    return MediaReader::ReadFrameResult_Success;
}

mt_sync_domain (readTask) MediaReader::ReadFrameResult
GetFileSession::doSendFrame (VideoStream::Message * const mt_nonnull msg,
                             Time                   const last_ts_nanosec)
{
    logD_(_func_, "&&&& doSendFrame BEGIN");
    Sender::MessageEntry_Pages * const msg_pages = Sender::MessageEntry_Pages::createNew ();
    msg->page_pool->msgRef (msg->page_list.first);

    msg_pages->page_pool = msg->page_pool;
    msg_pages->setFirstPage (msg->page_list.first);
    msg_pages->msg_offset = msg->msg_offset;
    msg_pages->header_len = 0;

    bool burst_limit = false;
    bool sender_limit = false;

    sender->lock ();
    {
        Sender::SendState const send_state = sender->getSendState_unlocked();
        if (send_state != Sender::SendState::ConnectionReady &&
            send_state != Sender::SendState::ConnectionOverloaded)
        {
            logD (getfile, _func, "ConnecdoSendFrametion overloaded: ", (unsigned) send_state);

            if (bps_limit_timer) {
                thread_ctx->getTimers()->deleteTimer (bps_limit_timer);
                bps_limit_timer = NULL;
            }

            burst_limit = true;
            sender_limit = true;
        }
    }
    sender->unlock ();

    bytes_transferred += msg->msg_len;

//    if (msg->timestamp_nanosec >= last_ts_nanosec) {
//    ++pass2_num_frames;
//    if (pass2_num_frames >= total_num_frames) {
//        logD (getfile, _func, "Finish");
//        logD_(_func_, "&&&& doSendFrame END ReadFrameResult_Finish");
//        return MediaReader::ReadFrameResult_Finish;
//    }

    if (!burst_limit) {
        if (first_data_read) {
            if (first_burst_size
                && bytes_transferred >= first_burst_size)
            {
                logD (getfile, _func, "first_burst_size limit");
                burst_limit = true;
            }
        } else
        if (bps_limit) {
            Time const cur_time_millisec = getTimeMilliseconds();
            if (cur_time_millisec <= transfer_start_time_millisec
                || (double) bytes_transferred /
                           ((double) (cur_time_millisec - transfer_start_time_millisec) / 1000.0)
                       >= (double) bps_limit)
            {
                logD (getfile, _func, "bps_limit");
                burst_limit = true;
            }
        }
    }

    if (burst_limit) {
        if (!sender_limit) {
            assert (!bps_limit_timer);
            bps_limit_timer =
                    thread_ctx->getTimers()->addTimer (
                            CbDesc<Timers::TimerCallback> (bpsLimitTimerTick, this, this),
                            1     /* time_seconds */,
                            false /* periodical */,
                            true  /* auto_delete */);
        }
        logD_(_func_, "&&&& doSendFrame END ReadFrameResult_BurstLimit");
        return MediaReader::ReadFrameResult_BurstLimit;
    }

    logD_(_func_, "&&&& doSendFrame END ReadFrameResult_Success");
    return MediaReader::ReadFrameResult_Success;
}

mt_sync_domain (readTask) bool
GetFileSession::senderClosedTask (void * const _self)
{
    logD_(_func_, "&&&& senderClosedTask");
    GetFileSession * const self = static_cast <GetFileSession*> (_self);
    logD_ (_func_);
    self->session_state = SessionState_Complete;
    return false /* do not reschedule */;
}

mt_sync_domain (readTask) bool
GetFileSession::readTask (void * const _self)
{
TimeChecker tc1;tc1.Start();
    logD_(_func_, "&&&& readTask BEGIN");
    GetFileSession * const self = static_cast <GetFileSession*> (_self);

    logD (getfile, _func_);

    if (self->session_state == SessionState_Header) {
        MOMENT_SERVER__HEADERS_DATE

//        for (;;) {
//            MediaReader::ReadFrameResult const res = self->media_reader.readMoreData (&read_frame_backend, self);
//            if (res == MediaReader::ReadFrameResult_Failure) {
//                logE_ (_func, "ReadFrameResult_Failure");

//                ConstMemory msg = "Data retrieval error";
//                self->sender->send (self->page_pool,
//                                    true /* do_flush */,
//                                    // TODO No cache
//                                    MOMENT_SERVER__500_HEADERS (msg.len()),
//                                    "\r\n",
//                                    msg);

//                if (!self->req_is_keepalive)
//                    self->sender->closeAfterFlush ();

//                logA_ ("mod_nvr 500 ", self->req_client_addr, " ", self->req_request_line);
//                return false /* do not reschedule */;
//            }

//            bool header_done = false;
//            if (res == MediaReader::ReadFrameResult_NoData) {
//                logD (getfile, _func, "ReadFrameResult_NoData");

//                if (!self->got_last_audio_ts &&
//                    !self->got_last_video_ts)
//                {
//                    ConstMemory msg = "Requested video data not found";
//                    self->sender->send (self->page_pool,
//                                        true /* do_flush */,
//                                        // TODO No cache
//                                        MOMENT_SERVER__404_HEADERS (msg.len()),
//                                        "\r\n",
//                                        msg);

//                    if (!self->req_is_keepalive)
//                        self->sender->closeAfterFlush ();

//                    logA_ ("mod_nvr 404 ", self->req_client_addr, " ", self->req_request_line);
//                    return false /* do not reschedule */;
//                }

//                header_done = true;
//            } else
//            if (res == MediaReader::ReadFrameResult_Finish) {
//                logD (getfile, _func, "ReadFrameResult_Finish");
//                header_done = true;
//            }

//            if (header_done) {
//                self->session_state = SessionState_Data;
//                self->media_reader.reset ();
//                break;
//            }

//            assert (res != MediaReader::ReadFrameResult_BurstLimit);
//            assert (res == MediaReader::ReadFrameResult_Success);
//        }

//        if(!self->media_reader.isInit())
//        {
//            logE_ (_func, "ReadFrameResult_Failure");

//            ConstMemory msg = "Data retrieval error";
//            self->sender->send (self->page_pool,
//                                true /* do_flush */,
//                                // TODO No cache
//                                MOMENT_SERVER__500_HEADERS (msg.len()),
//                                "\r\n",
//                                msg);

//            if (!self->req_is_keepalive)
//                self->sender->closeAfterFlush ();

//            logA_ ("mod_nvr 500 ", self->req_client_addr, " ", self->req_request_line);
//            return false /* do not reschedule */;
//        }
        logD_(_func_, "&&&& readTask SessionState_Header");
        Size const totalDataSize = self->media_reader.GetTotalSize(self->fileName);
        self->sender->send (self->page_pool,
                                true /* do_flush */,
                                // TODO No cache
                                MOMENT_SERVER__OK_HEADERS (
                                        ConstMemory ("application/octet-stream"),
                                        totalDataSize),
                                "\r\n");
        logD_ (_func, "&&&& CONTENT-LENGTH: ", totalDataSize);

        logD_(_func_, "&&&& readTask status sended");
        if (!self->req_is_keepalive){logD_(_func_, "&&&& readTask closed after flush");
            self->sender->closeAfterFlush ();}

        logA_ ("mod_nvr 200 ", self->req_client_addr, " ", self->req_request_line);

        bool header_done = true;
        if (header_done) {
            self->session_state = SessionState_Data;
            self->media_reader.reset ();
        }

        self->transfer_start_time_millisec = getTimeMilliseconds();
        //self->bytes_transferred += mp4_header_len;

        self->sender->getEventInformer()->subscribe (
                CbDesc<Sender::Frontend> (&sender_frontend, self, self));
        logD_(_func_, "&&&& readTask after doReadData()");
    }


    if (self->session_state == SessionState_Data) {
        logD_(_func_, "&&&&! readTask SessionState_Data");
        self->doSendFile ();
        logD_(_func_, "&&&&! readTask after doReadData()");
        self->first_data_read = false;
    }
logD_(_func_, "&&&& readTask END");
Time t1;tc1.Stop(&t1);
logD_(_func_, ">>>> Sending is [", t1, "]");
    return false /* do not reschedule */;
}

mt_sync_domain (readTask) void
GetFileSession::bpsLimitTimerTick (void * const _self)
{
    logD_(_func_, "&&&& bpsLimitTimerTick TICK");
    GetFileSession * const self = static_cast <GetFileSession*> (_self);

    self->thread_ctx->getTimers()->deleteTimer (self->bps_limit_timer);
    self->bps_limit_timer = NULL;

    logD_(_func_, "&&&& bpsLimitTimerTick BEFORE doReadData");
    //self->doReadData ();
    self->doSendFile();
    logD_(_func_, "&&&& bpsLimitTimerTick AFTER doReadData");
}

mt_sync_domain (readTask) void
GetFileSession::doReadData ()
{
    logD_(_func_, "&&&& doReadData BEGIN");
    if (session_state == SessionState_Complete) {
        logD_(_func_, "SessionState_Complete");
        logD_(_func_, "&&&& doReadData SessionState_Complete so return");
        return;
    }

    for (;;) {
        logD_(_func_, "&&&& doReadData read in cycle");
        MediaReader::ReadFrameResult const res = media_reader.readMoreData (&read_frame_backend, this);
        if (res == MediaReader::ReadFrameResult_Failure) {
            logD_(_func_, "&&&& doReadData cycle ReadFrameResult_Failure");
            logE_ (_func, "ReadFrameResult_Failure");
            session_state = SessionState_Complete;
            if (!req_is_keepalive) {
                sender->closeAfterFlush ();
            }
            media_reader.reset ();
            return;
        }

        if (res == MediaReader::ReadFrameResult_BurstLimit) {
            logD (getfile, _func, "ReadFrameResult_BurstLimit");
            logD_(_func_, "&&&& doReadData cycle ReadFrameResult_BurstLimit");
            return;
        }

        bool data_done = false;
        if (res == MediaReader::ReadFrameResult_NoData) {
            logD (getfile, _func, "ReadFrameResult_NoData");
            logD_(_func_, "&&&& doReadData cycle ReadFrameResult_NoData");
            data_done = true;
        } else
        if (res == MediaReader::ReadFrameResult_Finish) {
            logD (getfile, _func, "ReadFrameResult_Finish");
            logD_(_func_, "&&&& doReadData cycle ReadFrameResult_Finish");
            data_done = true;
        }

        if (data_done) {
            logD_(_func_, "&&&& doReadData cycle data_done is true");
            session_state = SessionState_Complete;
            if (!req_is_keepalive) {
                sender->closeAfterFlush ();
            }
            media_reader.reset ();
            break;
        }

        assert (res == MediaReader::ReadFrameResult_Success);
    }
    logD_(_func_, "&&&& doReadData END");
}

mt_sync_domain (readTask) void
GetFileSession::doSendFile ()
{
    logD_(_func_, "&&&& doReadData BEGIN");
    if (session_state == SessionState_Complete)
    {
        logD_(_func_, "SessionState_Complete");
        logD_(_func_, "&&&& doReadData SessionState_Complete so return");
        return;
    }

    std::ifstream is (fileName->cstr(), std::ifstream::binary);
    if (is)
    {
        is.seekg (0, is.end);
        int length = is.tellg();
        is.seekg (0, is.beg);

        int chunkSize = 1024*20; // 20 kB
        char * buffer = new char [length];
        is.read (buffer,length);

        int ccount = 0;
        for(int i=0;i<length;i+=chunkSize)
        {
            PagePool::PageListHead page_list;
            page_pool->getFillPages (&page_list, ConstMemory (buffer+i, std::min(chunkSize, length-i)));

            Sender::MessageEntry_Pages * const msg_pages = Sender::MessageEntry_Pages::createNew ();
            msg_pages->page_pool = page_pool;
            msg_pages->setFirstPage (page_list.first);
            msg_pages->msg_offset = 0;
            msg_pages->header_len = 0;

            sender->lock ();

            sender->sendMessage_unlocked (msg_pages, true /* do_flush */);

            sender->unlock ();
            ccount++;
        }
        logD_(_func_, "&&&&! ccount = ", ccount);

        is.close();
        delete [] buffer;

        session_state = SessionState_Complete;
        if (!req_is_keepalive)
        {
            sender->closeAfterFlush ();
        }
    }

    logD_(_func_, "&&&& doReadData END");
}

Sender::Frontend const GetFileSession::sender_frontend = {
    senderStateChanged,
    senderClosed
};

void
GetFileSession::senderStateChanged (Sender::SendState   const send_state,
                                    void              * const _self)
{
    logD_(_func_, "&&&& senderStateChanged BEGIN");
    GetFileSession * const self = static_cast <GetFileSession*> (_self);

    if (self->bps_limit_timer){logD_(_func_, "&&&& senderStateChanged bps_limit_timer is teru");
        return;}

    if (send_state == Sender::SendState::ConnectionReady)
        self->deferred_reg.scheduleTask (&self->read_task, false /* permanent */);
    logD_(_func_, "&&&& senderStateChanged END");
}

void
GetFileSession::senderClosed (Exception * const exc,
                              void      * const _self)
{
    logD_(_func_, "&&&& senderClosed BEGIN");
    GetFileSession * const self = static_cast <GetFileSession*> (_self);
    self->deferred_reg.scheduleTask (&self->sender_closed_task, false /* permanent */);
    logD_(_func_, "&&&& senderClosed END");
}

void
GetFileSession::start ()
{
    logD_(_func_, "&&&& START BEGIN");
    mutex.lock ();
    started = true;
    mutex.unlock ();

    logD_(_func_, "&&&& START BEFORE SCHEDULE");
    deferred_reg.scheduleTask (&read_task, false /* permanent */);
    logD_(_func_, "&&&& TASK SCHEDULED");
}

mt_const void
GetFileSession::init (MomentServer * const mt_nonnull moment,
                      HttpRequest  * const mt_nonnull req,
                      Sender       * const mt_nonnull sender,
                      PagePool     * const page_pool,
                      Vfs          * const vfs,
                      ConstMemory    const stream_name,
                      Time           const start_unixtime_sec,
                      Time           const duration_sec,
                      bool           const octet_stream_mime,
                      CbDesc<Frontend> const &frontend,
                      StRef<String>  const fileName)
{
    logD_(_func_, "&&&& INIT");
    this->moment = moment;
    this->page_pool = page_pool;
    this->sender = sender;
    this->frontend = frontend;

    this->start_unixtime_sec = start_unixtime_sec;
    this->duration_sec = duration_sec;

    this->octet_stream_mime = octet_stream_mime;

    this->req_is_keepalive = req->getKeepalive();
    this->req_client_addr = req->getClientAddress();
    this->req_request_line = st_grab (new (std::nothrow) String (req->getRequestLine()));

    this->fileName = fileName;

    media_reader.init (page_pool, vfs, stream_name, start_unixtime_sec, 0 /* burst_size_limit */, st_makeString(""), fileName, true);
    //mp4_muxer.init (page_pool, duration_sec * 1000);

    thread_ctx = moment->getReaderThreadPool()->grabThreadContext (stream_name);
    if (thread_ctx) {
        reader_thread_ctx = thread_ctx;
    } else {
        logE_ (_func, "Could not get reader thread context: ", exc->toString());
        reader_thread_ctx = NULL;
        thread_ctx = moment->getServerApp()->getServerContext()->getMainThreadContext();
    }

    deferred_reg.setDeferredProcessor (thread_ctx->getDeferredProcessor());

    logD_(_func_, "&&&& INITED");
}

GetFileSession::GetFileSession ()
    : page_pool          (this /* coderef_container */),
      sender             (this /* coderef_container */),
      start_unixtime_sec (0),
      duration_sec       (0),
      octet_stream_mime  (false),
      req_is_keepalive   (false),
      first_burst_size   (1 << 20 /* 1 MB */),
      bps_limit          (0),
      transfer_start_time_millisec (0),
      bytes_transferred  (0),
      thread_ctx         (this /* coderef_container */),
      reader_thread_ctx  (NULL),
      media_reader       (this /* coderef_container */),
      session_state      (SessionState_Header),
      first_data_read    (false),
      got_last_audio_ts  (false),
      last_audio_ts_nanosec (0),
      got_last_video_ts  (false),
      last_video_ts_nanosec (0),
      total_num_frames   (0),
      pass2_num_frames   (0),
      started            (false)
{
    read_task.cb  = CbDesc<DeferredProcessor::TaskCallback> (readTask,  this, this);
    sender_closed_task.cb = CbDesc<DeferredProcessor::TaskCallback> (senderClosedTask, this, this);
}

GetFileSession::~GetFileSession ()
{
    deferred_reg.release ();

    if (reader_thread_ctx) {
        moment->getReaderThreadPool()->releaseThreadContext (reader_thread_ctx);
        reader_thread_ctx = NULL;
    }
}

}

