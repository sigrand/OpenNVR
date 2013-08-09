/*  Moment Video Server - High performance media server
    Copyright (C) 2011-2013 Dmitry Shatrov
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


#ifndef MOMENT__RECORDER__H__
#define MOMENT__RECORDER__H__


#include <libmary/libmary.h>
#include <moment/playback.h>
#include <moment/channel.h>
#include <moment/channel_set.h>
#include <moment/av_recorder.h>
#include <moment/flv_muxer.h>


namespace Moment {

using namespace M;

class Recorder : public Object
{
private:
    StateMutex mutex;

    struct RecordingTicket : public Referenced
        { Recorder *recorder; };

    mt_const MomentServer *moment;
    mt_const ChannelSet   *channel_set;
    mt_const Ref<String>   filename_prefix;
    mt_const ServerThreadContext *recorder_thread_ctx;

    mt_async Playback playback;

    mt_async AvRecorder recorder;
    mt_async FlvMuxer   flv_muxer;

    mt_mutex (mutex) bool                 recording_now;
    mt_mutex (mutex) Ref<RecordingTicket> cur_recording_ticket;
    mt_mutex (mutex) Ref<String>          cur_channel_name;
    mt_mutex (mutex) WeakRef<Channel>     weak_cur_channel;
    mt_mutex (mutex) WeakRef<VideoStream> weak_cur_video_stream;
    mt_mutex (mutex) GenericInformer::SubscriptionKey channel_sbn;

    mt_iface (Playback::Frontend)
      static Playback::Frontend playback_frontend;

      static void startPlaybackItem (Playlist::Item          *item,
				     Time                     seek,
				     Playback::AdvanceTicket *advance_ticket,
				     void                    *_self);

      static void stopPlaybackItem (void *_self);
    mt_iface_end

    mt_iface (Channel::ChannelEvents)
      static Channel::ChannelEvents channel_events;

      static void startChannelItem (VideoStream *stream,
                                    bool         stream_changed,
                                    void        *_recording_ticket);

      static void stopChannelItem  (VideoStream *stream,
                                    bool         stream_changed,
                                    void        *_recording_ticket);

      static void newVideoStream   (VideoStream *stream,
                                    void        *_recording_ticket);
    mt_iface_end

    mt_mutex (mutex) void doStartItem ();
    mt_mutex (mutex) void doStopItem  ();

public:
    void setSingleChannel (ConstMemory const channel_name)
    {
	playback.setSingleChannelRecorder (channel_name);
    }

    Result loadPlaylistFile (ConstMemory    const filename,
			     bool           const keep_cur_item,
                             PlaybackItem * const mt_nonnull default_playback_item,
			     Ref<String>  * const ret_err_msg)
    {
	return playback.loadPlaylistFile (filename, keep_cur_item, default_playback_item, ret_err_msg);
    }

    Result loadPlaylistMem (ConstMemory    const mem,
			    bool           const keep_cur_item,
                            PlaybackItem * const mt_nonnull default_playback_item,
			    Ref<String>  * const ret_err_msg)
    {
	return playback.loadPlaylistMem (mem, keep_cur_item, default_playback_item, ret_err_msg);
    }

    mt_const void init (MomentServer *moment,
			PagePool     *page_pool,
			ChannelSet   *channel_set,
			ConstMemory   filename_prefix,
                        Uint64        min_playlist_duration_sec);

     Recorder ();
    ~Recorder ();
};

}


#endif /* MOMENT__RECORDER__H__ */

