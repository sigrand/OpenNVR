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


#ifndef MOMENT_NVR__MEDIA_RECORDER__H__
#define MOMENT_NVR__MEDIA_RECORDER__H__


#include <moment/libmoment.h>

#include <moment-nvr/types.h>
#include <moment-nvr/naming_scheme.h>
#include "av_nvr_recorder.h"
#include "flv_file_muxer.h"


namespace MomentNvr {

using namespace M;
using namespace Moment;

class MediaRecorder : public Object
{
private:
    StateMutex mutex;

private:
    struct Recording : public Object
    {
        WeakDepRef<MediaRecorder> weak_media_recorder;

        DeferredConnectionSender vdat_sender;
        mt_mutex (MediaRecorder::mutex) Ref<Vfs::VfsFile> vdat_file;
        FileConnection vdat_conn;

        DeferredConnectionSender idx_sender;
        mt_mutex (MediaRecorder::mutex) Ref<Vfs::VfsFile> idx_file;
        FileConnection idx_conn;

        mt_mutex (MediaRecorder::mutex) Size cur_data_offset;

		mt_mutex (mutex) AvNvrRecorder recorder;

        Recording ()
            : vdat_sender (this /* coderef_container */),
              vdat_conn   (this /* coderef_container */),
              idx_sender  (this /* coderef_container */),
			  idx_conn    (this /* coderef_container */),
			  recorder    (this /* coderef_container */),
              cur_data_offset (0)
		{}

		~Recording()
		{
			if(recorder.IsStarted())
			{
				recorder.stop();
			}
		}
    };

    class PrewriteQueue_name;

    struct PrewriteQueueEntry : public IntrusiveListElement<PrewriteQueue_name>
    {
        enum EntryType {
            EntryType_Audio,
            EntryType_Video
        };

        EntryType entry_type;

        VideoStream::AudioMessage audio_msg;
        VideoStream::VideoMessage video_msg;

        Time getTimestampNanosec () const
        {
            if (entry_type == EntryType_Audio)
                return audio_msg.timestamp_nanosec;

            return video_msg.timestamp_nanosec;
        }

        ~PrewriteQueueEntry ()
        {
            if (audio_msg.page_pool)
                audio_msg.page_pool->msgUnref (audio_msg.page_list.first);

            if (video_msg.page_pool)
                video_msg.page_pool->msgUnref (video_msg.page_list.first);
        }
    };

    typedef IntrusiveList< PrewriteQueueEntry,
                           PrewriteQueue_name,
                           DeleteAction<PrewriteQueueEntry> > PrewriteQueue;

    mt_const DataDepRef<PagePool> page_pool;
    mt_const DataDepRef<ServerThreadContext> thread_ctx;
    mt_const Ref<Vfs> vfs;

    mt_const Ref<NamingScheme> naming_scheme;
    mt_const StRef<String> channel_name;

    mt_const Time  prewrite_nanosec;
    mt_const Count prewrite_num_frames_limit;
    mt_const Time  postwrite_nanosec;
    mt_const Count postwrite_num_frames_limit;

    struct StreamTicket : public Referenced
        { MediaRecorder *media_recorder; };

    mt_mutex (mutex) Ref<StreamTicket> cur_stream_ticket;
    mt_mutex (mutex) Ref<VideoStream> cur_stream;
    mt_mutex (mutex) GenericInformer::SubscriptionKey stream_sbn;
    mt_mutex (mutex) bool got_unixtime_offset;
    mt_mutex (mutex) Time unixtime_offset_nanosec;
    mt_mutex (mutex) Time prv_unixtime_timestamp_nanosec;

    mt_mutex (mutex) bool got_pending_aac_seq_hdr;
    mt_mutex (mutex) VideoStream::AudioMessage pending_aac_seq_hdr;

    mt_mutex (mutex) bool got_pending_avc_seq_hdr;
    mt_mutex (mutex) VideoStream::VideoMessage pending_avc_seq_hdr;

    mt_mutex (mutex) bool got_avc_seq_hdr;

    mt_mutex (mutex) Time next_idx_unixtime_nanosec;

    mt_mutex (mutex) Ref<Recording> recording;
    mt_mutex (mutex) Time next_file_unixtime_nanosec;

    mt_mutex (mutex) bool postwrite_active;
    mt_mutex (mutex) bool got_postwrite_start_ts;
    mt_mutex (mutex) Time postwrite_start_ts_nanosec;
    mt_mutex (mutex) Count postwrite_frame_counter;

    mt_mutex (mutex) PrewriteQueue prewrite_queue;
    mt_mutex (mutex) Count prewrite_queue_size;

    mt_mutex (mutex) void recordStreamHeaders ();
    mt_mutex (mutex) void recordPrewrite ();

    mt_mutex (mutex) void recordAudioMessage (VideoStream::AudioMessage * mt_nonnull audio_msg);
    mt_mutex (mutex) void recordVideoMessage (VideoStream::VideoMessage * mt_nonnull video_msg);

    mt_mutex (mutex) void recordMessage (VideoStream::Message * mt_nonnull msg,
                                         bool                  is_audio_msg,
                                         ConstMemory           header);

    mt_mutex (mutex) void doRecordMessage (VideoStream::Message * mt_nonnull msg,
                                           ConstMemory           header,
                                           Time                  unixtime_timestamp_nanosec);

    mt_mutex (mutex) Result openVdatFile (ConstMemory _filename,
                                          Time        start_unixtime_nanosec);

	mt_mutex (mutex) Result openFlvFile (ConstMemory _filename);

    mt_mutex (mutex) Result openIdxFile  (ConstMemory _filename);

    mt_mutex (mutex) Result doStartRecording (Time cur_unixtime_nanosec);

  mt_iface (Sender::Frontend)
    static Sender::Frontend const sender_frontend;

    static void senderStateChanged (Sender::SendState  send_state,
                                    void              *_recording);

    static void senderClosed (Exception *_exc,
                              void      *_recording);
  mt_iface_end

  mt_iface (VideoStream::EventHandler)
    static VideoStream::EventHandler const stream_event_handler;

    static void streamAudioMessage (VideoStream::AudioMessage * mt_nonnull audio_msg,
                                    void *_stream_ticket);

    static void streamVideoMessage (VideoStream::VideoMessage * mt_nonnull video_msg,
                                    void *_stream_ticket);

    static void streamClosed (void *_stream_ticket);
  mt_iface_end

public:
    void setVideoStream (VideoStream *stream);

    void startRecording ();
    void stopRecording  ();

    bool isRecording ();

    void init (PagePool            * mt_nonnull page_pool,
               ServerThreadContext * mt_nonnull thread_ctx,
               Vfs                 * mt_nonnull vfs,
               NamingScheme        * mt_nonnull naming_scheme,
               ConstMemory          channel_name,
               Time                 prewrite_nanosec,
               Count                prewrite_num_frames_limit,
               Time                 postwrite_nanosec,
               Count                postwrite_num_frames_limit);

     MediaRecorder ();
    ~MediaRecorder ();
};

}


#endif /* MOMENT_NVR__MEDIA_RECORDER__H__ */

