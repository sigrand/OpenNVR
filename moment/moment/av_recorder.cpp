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


#include <moment/av_recorder.h>


using namespace M;

namespace Moment {

static LogGroup libMary_logGroup_recorder ("av_recorder", LogLevel::I);
static LogGroup libMary_logGroup_recorder_frames ("av_recorder_frames", LogLevel::I);

Sender::Frontend const AvRecorder::sender_frontend = {
    senderSendStateChanged,
    senderClosed
};

VideoStream::FrameSaver::FrameHandler const AvRecorder::saved_frame_handler = {
    savedAudioFrame,
    savedVideoFrame
};

Result
AvRecorder::savedAudioFrame (VideoStream::AudioMessage * const mt_nonnull audio_msg,
                             void                      * const _self)
{
    AvRecorder * const self = static_cast <AvRecorder*> (_self);
    VideoStream::AudioMessage tmp_audio_msg = *audio_msg;
    tmp_audio_msg.timestamp_nanosec = 0;

    if (!self->muxer->muxAudioMessage (&tmp_audio_msg)) {
        logE (recorder, _func, "muxer->muxAudioMessage() failed (aac seq hdr): ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

Result
AvRecorder::savedVideoFrame (VideoStream::VideoMessage * const mt_nonnull video_msg,
                             void                      * const _self)
{
    AvRecorder * const self = static_cast <AvRecorder*> (_self);
    VideoStream::VideoMessage tmp_video_msg = *video_msg;
    tmp_video_msg.timestamp_nanosec = 0;

    if (!self->muxer->muxVideoMessage (&tmp_video_msg)) {
        logE (recorder, _func, "muxer->muxVideoMessage() failed (metadata): ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

mt_mutex (mutex) void
AvRecorder::muxInitialMessages ()
{
    logD (recorder_frames, _func_);

    if (!video_stream)
	return;

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
AvRecorder::doStop ()
{
    cur_stream_ticket = NULL;

    if (recording) {
	// Note that muxer->endMuxing() implies recording->sender.closeAfterFlush().
	if (!muxer->endMuxing ())
	    logE (recorder, _func, "muxer->endMuxing() failed: ", exc->toString());

      // The file can't be released at this point, because it is used by
      // deferred sender. The file is released later in senderClosed().

	recording = NULL;
    }
}

void
AvRecorder::senderSendStateChanged (Sender::SendState   const send_state,
				    void              * const _recording)
{
  // TODO Start dropping frames when destination is overloaded.
  //      This doesn't matter for local files, because local write operations
  //      always block till completion.
}

void
AvRecorder::senderClosed (Exception * const exc_,
			  void      * const _recording)
{
    Recording * const recording = static_cast <Recording*> (_recording);

    if (exc_)
	logE (recorder, _func, "exception: ", exc_->toString());

    CodeRef const self_ref = recording->weak_av_recorder;
    if (!self_ref) {
	return;
    }
    AvRecorder * const self = recording->unsafe_av_recorder;

    recording->mutex.lock ();
    recording->storage_file = NULL;
    recording->mutex.unlock ();
}

VideoStream::EventHandler const AvRecorder::stream_handler = {
    streamAudioMessage,
    streamVideoMessage,
    NULL /* rtmpCommandMessage */,
    streamClosed,
    NULL /* numWatchersChanged */
};

void
AvRecorder::streamAudioMessage (VideoStream::AudioMessage * const mt_nonnull msg,
				void * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    AvRecorder * const self = stream_ticket->av_recorder;

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

    if (!self->muxer->muxAudioMessage (&alt_msg)) {
	logE (recorder, _func, "muxer->muxAudioMessage() failed: ", exc->toString());
	self->doStop ();
    }

    self->total_bytes_recorded += alt_msg.msg_len;

    self->mutex.unlock ();
}

void
AvRecorder::streamVideoMessage (VideoStream::VideoMessage * const mt_nonnull msg,
				void * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    AvRecorder * const self = stream_ticket->av_recorder;

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

    if (!self->muxer->muxVideoMessage (&alt_msg)) {
	logE (recorder, _func, "muxer->muxVideoMessage() failed: ", exc->toString());
	self->doStop ();
    }

    self->total_bytes_recorded += alt_msg.msg_len;

    self->mutex.unlock ();
}

void
AvRecorder::streamClosed (void * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    AvRecorder * const self = stream_ticket->av_recorder;

    self->mutex.lock ();
    if (self->cur_stream_ticket != stream_ticket) {
	self->mutex.unlock ();
	return;
    }

    self->doStop ();
    self->mutex.unlock ();
}

void
AvRecorder::start (ConstMemory const filename)
{
    mutex.lock ();

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
    muxer->setSender (&recording->sender);

    recording->storage_file = storage->openFile (filename, thread_ctx->getDeferredProcessor());
    if (!recording->storage_file) {
	logE (recorder, _func, "storage->openFile() failed for filename ",
	      filename, ": ", exc->toString());
	recording = NULL;
	mutex.unlock ();
	return;
    }
    recording->conn = recording->storage_file->getConnection();

    recording->weak_av_recorder = this;
    recording->unsafe_av_recorder = this;

    recording->sender.setConnection (recording->conn);
    recording->sender.setQueue (thread_ctx->getDeferredConnectionSenderQueue());
    recording->sender.setFrontend (
	    CbDesc<Sender::Frontend> (&sender_frontend,
				      recording /* cb_data */,
				      recording /* coderef_container */));

    paused = false;

    if (!muxer->beginMuxing ()) {
	logE_ (_func, "muxer->beginMuxing() failed: ", exc->toString());
	// TODO Fail?
    }

    muxInitialMessages ();

    mutex.unlock ();
}

void
AvRecorder::pause ()
{
    mutex.lock ();
    paused = true;
    // TODO Track keyframes.
    mutex.unlock ();
}

void
AvRecorder::resume ()
{
    mutex.lock ();
    paused = false;
    mutex.unlock ();
}

void
AvRecorder::stop ()
{
    mutex.lock ();
    doStop ();
    mutex.unlock ();
}

void
AvRecorder::setVideoStream (VideoStream * const stream)
{
    mutex.lock ();

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
AvRecorder::setMuxer (AvMuxer * const muxer)
{
    this->muxer = muxer;
}

mt_const void
AvRecorder::init (ServerThreadContext * const thread_ctx,
		  Storage             * const storage)
{
    this->thread_ctx = thread_ctx;
    this->storage = storage;
}

AvRecorder::AvRecorder (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      thread_ctx (NULL),
      storage (NULL),
      paused (false),
      got_first_frame (false),
      first_frame_time (0),
      cur_frame_time (0),
      total_bytes_recorded (0)
{
}

AvRecorder::~AvRecorder ()
{
    mutex.lock ();
    doStop ();
    mutex.unlock ();
}

}

