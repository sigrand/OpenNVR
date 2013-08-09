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


#ifndef MOMENT__PLAYBACK__H__
#define MOMENT__PLAYBACK__H__


#include <libmary/libmary.h>
#include <moment/playlist.h>


namespace Moment {

using namespace M;

class Playback : public DependentCodeReferenced
{
private:
    StateMutex mutex;

public:
    class AdvanceTicket : public Referenced
    {
	friend class Playback;

    private:
	Playback * const playback;
	AdvanceTicket (Playback * const playback) : playback (playback) {}
    };

    struct Frontend
    {
	void (*startItem) (Playlist::Item *item,
			   Time            seek,
			   AdvanceTicket  *advance_ticket,
			   void           *cb_data);

	void (*stopItem) (void *cb_data);
    };

private:
    mt_const Cb<Frontend> frontend;

    mt_const Timers *timers;

    mt_const Uint64 min_playlist_duration_sec;

    mt_mutex (mutex)
    mt_begin
      Playlist playlist;

      StRef<Playlist::Item> cur_item;

      Timers::TimerKey playback_timer;

      bool got_next;
      StRef<Playlist::Item> next_item;
      Time next_start_rel;
      Time next_seek;
      Time next_duration;
      bool next_duration_full;

      Time last_playlist_end_time;

      Ref<AdvanceTicket> advance_ticket;

      bool advancing;
    mt_end

    mt_unlocks_locks (mutex) void advancePlayback ();

    static void playbackTimerTick (void *_advance_ticket);

    mt_mutex (mutex) void doSetPosition (Playlist::Item *item,
					 Time            seek);

    Result doLoadPlaylist (ConstMemory   src,
			   bool          keep_cur_item,
                           PlaybackItem * mt_nonnull default_playback_item,
			   Ref<String>  *ret_err_msg,
			   bool          is_file,
                           bool          is_dir,
                           bool          dir_re_read);

public:
    void advance (AdvanceTicket *user_advance_ticket);

    void stop ();

    Result setPosition_Id (ConstMemory id,
			   Time        seek);

    Result setPosition_Index (Count idx,
			      Time  seek);

    void setSingleItem (PlaybackItem *item);

    void setSingleChannelRecorder (ConstMemory channel_name);

    Result loadPlaylistFile (ConstMemory   filename,
			     bool          keep_cur_item,
                             PlaybackItem * mt_nonnull default_playback_item,
			     Ref<String>  *ret_err_msg);

    Result loadPlaylistMem (ConstMemory   mem,
			    bool          keep_cur_item,
                            PlaybackItem * mt_nonnull default_playback_item,
			    Ref<String>  *ret_err_msg);

    Result loadPlaylistDirectory (ConstMemory   dirname,
                                  bool          re_read,
                                  bool          keep_cur_item,
                                  PlaybackItem * mt_nonnull default_playback_item);

    mt_const void init (CbDesc<Frontend> const &frontend,
                        Timers *timers,
                        Uint64  min_playlist_duration_sec);

     Playback (Object *coderef_container);
    ~Playback ();
};

}


#endif /* MOMENT__PLAYBACK__H__ */

