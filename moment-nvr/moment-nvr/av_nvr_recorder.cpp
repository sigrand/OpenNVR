/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov
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


#include "av_nvr_recorder.h"


using namespace M;
using namespace Moment;

namespace MomentNvr
{

static LogGroup libMary_logGroup_recorder ("av_nvr_recorder", LogLevel::I);
static LogGroup libMary_logGroup_recorder_frames ("av_nvr_recorder_frames", LogLevel::I);


VideoStream::FrameSaver::FrameHandler const AvNvrRecorder::saved_frame_handler = {
    savedAudioFrame,
    savedVideoFrame
};

Result
AvNvrRecorder::savedAudioFrame (VideoStream::AudioMessage * const mt_nonnull audio_msg,
                             void                      * const _self)
{
	logD_(_func, "savedAudioFrame:: MsgType(", (Int32)audio_msg->msg_type, ")Time(", (double)audio_msg->timestamp_nanosec / 1000000000.0,
		")MsgSize(", audio_msg->msg_len, ")MsgOffset(", audio_msg->msg_offset, ")frame_type(",
		audio_msg->frame_type, ")codec_id(", audio_msg->codec_id, ")rate(", audio_msg->rate,
		")channels(", audio_msg->channels, ")");

    AvNvrRecorder * const self = static_cast <AvNvrRecorder*> (_self);
    VideoStream::AudioMessage tmp_audio_msg = *audio_msg;
    tmp_audio_msg.timestamp_nanosec = 0;

    if (!self->flv_muxer.muxAudioMessage (&tmp_audio_msg)) {
        logE (recorder, _func, "flv_muxer.muxAudioMessage() failed (aac seq hdr): ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

Result
AvNvrRecorder::savedVideoFrame (VideoStream::VideoMessage * const mt_nonnull video_msg,
                             void                      * const _self)
{
	logD_(_func, "MsgType(", (Int32)video_msg->msg_type, ")Time(", (double)video_msg->timestamp_nanosec / 1000000000.0,
		")MsgSize(", video_msg->msg_len, ")MsgOffset(", video_msg->msg_offset, ")frame_type(",
		video_msg->frame_type, ")codec_id(", video_msg->codec_id, ")");

    AvNvrRecorder * const self = static_cast <AvNvrRecorder*> (_self);
    VideoStream::VideoMessage tmp_video_msg = *video_msg;
    tmp_video_msg.timestamp_nanosec = 0;

    if (!self->flv_muxer.muxVideoMessage (&tmp_video_msg)) {
        logE (recorder, _func, "flv_muxer.muxVideoMessage() failed (metadata): ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

mt_mutex (mutex) void
AvNvrRecorder::muxInitialMessages ()
{
    logD (recorder_frames, _func_);

    if (!video_stream)
	return;

	VideoStream::FrameSaver * pFrameSaver = video_stream->getFrameSaver();
	VideoStream::AudioMessage * pAACHeader = pFrameSaver->getAacSequenceHeader();
	VideoStream::VideoMessage * pAVCHeader = pFrameSaver->getAvcSequenceHeader();

	logD_(_func, "AAC size(", pAACHeader ? pAACHeader->msg_len : 0, "), AVC size(", pAVCHeader ? pAVCHeader->msg_len : 0, ")");

    VideoStream::SavedFrame saved_frame;
    VideoStream::SavedAudioFrame saved_audio_frame;

    video_stream->lock ();

    VideoStream::FrameSaver * const frame_saver = video_stream->getFrameSaver ();
    if (!frame_saver)
	goto _return;

    if (!frame_saver->reportSavedFrames (&saved_frame_handler, this)) {
        doStop ();
        goto _return;
    }

_return:
    video_stream->unlock ();
}

mt_mutex (mutex) void
AvNvrRecorder::doStop ()
{
    cur_stream_ticket = NULL;

    if (recording) {
	// Note that flv_muxer.endMuxing() implies recording->sender.closeAfterFlush().
	if (!flv_muxer.endMuxing ())
	    logE (recorder, _func, "flv_muxer.endMuxing() failed: ", exc->toString());

      // The file can't be released at this point, because it is used by
      // deferred sender. The file is released later in senderClosed().

	recording = NULL;
    }
}

VideoStream::EventHandler const AvNvrRecorder::stream_handler = {
    streamAudioMessage,
    streamVideoMessage,
    NULL /* rtmpCommandMessage */,
    streamClosed,
    NULL /* numWatchersChanged */
};

void
AvNvrRecorder::streamAudioMessage (VideoStream::AudioMessage * const mt_nonnull msg,
				void * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    AvNvrRecorder * const self = stream_ticket->av_nvr_recorder;

    self->mutex.lock ();
    if (self->cur_stream_ticket != stream_ticket) {
	self->mutex.unlock ();
	return;
    }

    if (!self->recording) {
	self->mutex.unlock ();
	return;
    }

    if (self->total_bytes_recorded >= self->recording_limit) {
	self->doStop ();
	self->mutex.unlock ();
	return;
    }

    if (!self->got_first_frame & msg->frame_type.isAudioData()) {
	logD (recorder_frames, _func, "first frame (audio)");
	self->got_first_frame = true;
	// Note that integer overflows are possible here and that's fine.
	self->first_frame_time = msg->timestamp_nanosec /* - self->cur_frame_time */;
    }
    self->cur_frame_time = msg->timestamp_nanosec - self->first_frame_time;
    logD (recorder_frames, _func, "cur_frame_time: ", self->cur_frame_time, ", got_first_frame: ", self->got_first_frame);

    VideoStream::AudioMessage alt_msg = *msg;
    alt_msg.timestamp_nanosec = self->cur_frame_time;

    if (!self->flv_muxer.muxAudioMessage (&alt_msg)) {
	logE (recorder, _func, "flv_muxer.muxAudioMessage() failed: ", exc->toString());
	self->doStop ();
    }

    self->total_bytes_recorded += alt_msg.msg_len;

    self->mutex.unlock ();
}

void
AvNvrRecorder::streamVideoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
				void * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    AvNvrRecorder * const self = stream_ticket->av_nvr_recorder;

    self->mutex.lock ();
    if (self->cur_stream_ticket != stream_ticket) {
	self->mutex.unlock ();
	return;
    }

    if (!self->recording) {
	self->mutex.unlock ();
	return;
    }

    if (self->total_bytes_recorded >= self->recording_limit) {
	self->doStop ();
	self->mutex.unlock ();
	return;
    }

    if (!self->got_first_frame && msg->frame_type.isVideoData()) {
	logD (recorder_frames, _func, "first frame (video)");
	self->got_first_frame = true;
	// Note that integer overflows are possible here and that's fine.
	self->first_frame_time = msg->timestamp_nanosec /* - self->cur_frame_time */;
    }
    self->cur_frame_time = msg->timestamp_nanosec - self->first_frame_time;
    logD (recorder_frames, _func, "cur_frame_time: ", self->cur_frame_time, ", got_first_frame: ", self->got_first_frame);

    VideoStream::VideoMessage alt_msg = *msg;
    alt_msg.timestamp_nanosec = self->cur_frame_time;

    if (!self->flv_muxer.muxVideoMessage (&alt_msg)) {
	logE (recorder, _func, "flv_muxer.muxVideoMessage() failed: ", exc->toString());
	self->doStop ();
    }

    self->total_bytes_recorded += alt_msg.msg_len;

    self->mutex.unlock ();
}

void
AvNvrRecorder::streamClosed (void * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    AvNvrRecorder * const self = stream_ticket->av_nvr_recorder;

    self->mutex.lock ();
    if (self->cur_stream_ticket != stream_ticket) {
	self->mutex.unlock ();
	return;
    }

    self->doStop ();
    self->mutex.unlock ();
}

void
AvNvrRecorder::start (ConstMemory const filename)
{
	mutex.lock ();
	logD_(_func);

    if (recording) {
      // TODO Stop current recording and start a new one.
	logW (recorder, _func, "Already recording");
	mutex.unlock ();
	return;
    }

    got_first_frame = false;
    first_frame_time = 0;
    cur_frame_time = 0;

    recording = grab (new Recording);

	StRef<String> const strfilename = st_makeString (filename);

    Ref<Vfs::VfsFile> vfs_file = vfs->openFile (strfilename->mem(), FileOpenFlags::Create, FileAccessMode::ReadWrite);
    if (!vfs_file) {
	logE (recorder, _func, "vfs->openFile() failed for filename ",
	      filename, ": ", exc->toString());
	recording = NULL;
	mutex.unlock ();
	return;
    }

	{
		ConstMemory memNoAudio = video_stream->getParam("no_audio", NULL);
		logD_(_func, "memNoAudio = ", memNoAudio.isEmpty() ? "is not presented" : (const char *)memNoAudio.mem());
		ConstMemory memNoVideo = video_stream->getParam("no_video", NULL);
		logD_(_func, "memNoVideo = ", memNoVideo.isEmpty() ? "is not presented" : (const char *)memNoVideo.mem());

		VideoStream::FrameSaver * pFrameSaver = video_stream->getFrameSaver();
		VideoStream::AudioMessage * pAACHeader = pFrameSaver->getAacSequenceHeader();
		VideoStream::VideoMessage * pAVCHeader = pFrameSaver->getAvcSequenceHeader();

		logD_(_func, "AAC size(", pAACHeader ? pAACHeader->msg_len : 0, "), AVC size(", pAVCHeader ? pAVCHeader->msg_len : 0, ")");
	}

	FLV::CFlvFileMuxerInitParams initParams(true, true);

	//logD_(_func, "before init, flv_muxer.Init(&initParams, vfs_file)");
	flv_muxer.Init(&initParams, vfs_file);

    recording->weak_av_nvr_recorder = this;
    recording->unsafe_av_nvr_recorder = this;

    paused = false;

	//logD_(_func, "before init, flv_muxer.beginMuxing()");
    if (!flv_muxer.beginMuxing ()) {
	logE_ (_func, "flv_muxer.beginMuxing() failed: ", exc->toString());
	// TODO Fail?
    }

    muxInitialMessages ();

    mutex.unlock ();

	logD_(_func, "EXIT");
}

void
AvNvrRecorder::pause ()
{
    mutex.lock ();
    paused = true;
    // TODO Track keyframes.
    mutex.unlock ();
}

void
AvNvrRecorder::resume ()
{
    mutex.lock ();
    paused = false;
    mutex.unlock ();
}

void
AvNvrRecorder::stop ()
{
    mutex.lock ();
    doStop ();
    mutex.unlock ();
}

void
AvNvrRecorder::setVideoStream (VideoStream * const stream)
{
	mutex.lock ();

	if (this->video_stream == stream) {
		logD_ (_this_func, "same stream");
		mutex.unlock ();
		return;
	}

    this->video_stream = stream;

    cur_stream_ticket = grab (new StreamTicket (this));

  // TODO Unsubsribe from the previous stream's events.

    got_first_frame = false;

    // TODO Fix race condition with stream_closed() (What if the stream has just been closed?)
    stream->getEventInformer()->subscribe (
	    CbDesc<VideoStream::EventHandler> (
		    &stream_handler,
		    cur_stream_ticket /* cb_data */,
		    getCoderefContainer(),
		    cur_stream_ticket));

    mutex.unlock ();
}

mt_const void
AvNvrRecorder::init (	ServerThreadContext * const thread_ctx,
						Vfs * const mt_nonnull vfs)
{
    this->thread_ctx = thread_ctx;
	this->vfs = vfs;
}

AvNvrRecorder::AvNvrRecorder (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      thread_ctx (NULL),
      paused (false),
      got_first_frame (false),
      first_frame_time (0),
      cur_frame_time (0),
      total_bytes_recorded (0),
	  recording_limit(Uint64_Max)
{
}

AvNvrRecorder::~AvNvrRecorder ()
{
    mutex.lock ();
    doStop ();
    mutex.unlock ();
}

}	// namespace MomentNvr

