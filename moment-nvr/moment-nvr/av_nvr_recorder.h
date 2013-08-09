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


#ifndef __MOMENT__AV_NVR_RECORDER__H__
#define __MOMENT__AV_NVR_RECORDER__H__


#include <libmary/libmary.h>

#include "flv_file_muxer.h"

namespace MomentNvr
{

using namespace M;
using namespace Moment;

class AvNvrRecorder : public DependentCodeReferenced
{
private:
    Mutex mutex;

public:
    struct Frontend {
	void (*error) (Exception *exc_,
		       void      *cb_data);
    };

private:
    // Tickets help to distinguish asynchronous messages from different streams.
    // The allows to ignore messages from the old stream when switching streams.
    class StreamTicket : public Referenced
    {
    public:
	AvNvrRecorder * const av_nvr_recorder;

	StreamTicket (AvNvrRecorder * const p_av_nvr_recorder)
	    : av_nvr_recorder (p_av_nvr_recorder)
	{
	}
    };

    class Recording : public Object
    {
    public:
        StateMutex mutex;

	WeakCodeRef weak_av_nvr_recorder;
	AvNvrRecorder *unsafe_av_nvr_recorder;

	Recording ()
	{
//	    logD_ (_func, "0x", fmt_hex, (UintPtr) this);
	}

	~Recording ()
	{
//	    logD_ (_func, "0x", fmt_hex, (UintPtr) this);
	}
    };

    mt_mutex (mutex) Ref<VideoStream> video_stream;

    mt_mutex (mutex) Ref<StreamTicket> cur_stream_ticket;

    mt_const ServerThreadContext *thread_ctx;

    // Muxer operations should be synchronized with 'mutex'.
    mt_const FLV::CFlvFileMuxer flv_muxer;

    mt_mutex (mutex) Ref<Recording> recording;
    mt_mutex (mutex) bool paused;

    mt_mutex (mutex) bool got_first_frame;
    mt_mutex (mutex) Time first_frame_time;
    mt_mutex (mutex) Time cur_frame_time;

    mt_const Uint64 recording_limit;	// by default Uint64_Max
    mt_mutex (mutex) Uint64 total_bytes_recorded;

    mt_const Cb<Frontend> frontend;

	mt_const Ref<Vfs> vfs;

  mt_iface (VideoStream::FrameSaver::FrameHandler)
    static VideoStream::FrameSaver::FrameHandler const saved_frame_handler;

    static Result savedAudioFrame (VideoStream::AudioMessage * mt_nonnull audio_msg,
                                   void                      *_self);

    static Result savedVideoFrame (VideoStream::VideoMessage * mt_nonnull video_msg,
                                   void                      *_self);
  mt_iface_end

    mt_mutex (mutex) void muxInitialMessages ();

    mt_mutex (mutex) void doStop ();

  mt_iface (Sender::Frontend)
    static Sender::Frontend const sender_frontend;

    static void senderSendStateChanged (Sender::SendState  send_state,
                                        void              *_recording);

    static void senderClosed (Exception *exc_,
                              void      *_recording);
  mt_iface_end

  mt_iface (VideoStream::EventHandler)
    static VideoStream::EventHandler const stream_handler;

    static void streamAudioMessage (VideoStream::AudioMessage * mt_nonnull msg,
                                    void *_stream_ticket);

    static void streamVideoMessage (VideoStream::VideoMessage * mt_nonnull msg,
                                    void *_stream_ticket);

    static void streamClosed (void *_stream_ticket);
  mt_iface_end

public:
    void start  (ConstMemory filename);
    void pause  ();
    void resume ();
    void stop   ();

	bool IsStarted(void) const { return !recording.isNull(); }

    void setVideoStream (VideoStream *stream);

    mt_const void setFrontend (CbDesc<Frontend> const &frontend)
        { this->frontend = frontend; }

    mt_const void setRecordingLimit (Uint64 recording_limit)
        { this->recording_limit = recording_limit; }

	mt_const void init (ServerThreadContext *thread_ctx,
						Vfs * const mt_nonnull vfs);

     AvNvrRecorder (Object *coderef_container);
    ~AvNvrRecorder ();
};

}	// namespace MomentNvr


#endif /* __MOMENT__AV_NVR_RECORDER__H__ */

