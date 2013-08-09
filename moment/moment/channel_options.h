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


#ifndef MOMENT__CHANNEL_OPTIONS__H__
#define MOMENT__CHANNEL_OPTIONS__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class PlaybackItem : public Referenced
{
public:
    struct SpecKind
    {
        enum Value {
            None,
            Chain,
            Uri,
            Playlist,
            Slave
        };
        operator Value () const { return value; }
        SpecKind (Value const value) : value (value) {}
        SpecKind () {}
        Size toString_ (Memory const &mem, Format const & /* fmt */) const
        {
            switch (value) {
                case None:     return toString (mem, "SpecKind::None");
                case Chain:    return toString (mem, "SpecKind::Chain");
                case Uri:      return toString (mem, "SpecKind::Uri");
                case Playlist: return toString (mem, "SpecKind::Playlist");
                case Slave:    return toString (mem, "SpecKind::Slave");
            }
            unreachable ();
            return 0;
        }
    private:
        Value value;
    };

  mt_const
  mt_begin
    StRef<String> stream_spec;
    SpecKind spec_kind;

    bool no_audio;
    bool no_video;

    bool force_transcode;
    bool force_transcode_audio;
    bool force_transcode_video;

    bool aac_perfect_timestamp;
    bool sync_to_clock;

    bool send_metadata;
    bool enable_prechunking;

    Uint64 default_width;
    Uint64 default_height;
    Uint64 default_bitrate;

    Time no_video_timeout;
  mt_end

    void dump ()
    {
        logLock ();
        logs->print ("    stream_spec: ", stream_spec, "\n"
                     "    spec_kind: ", spec_kind, "\n"
                     "    no_audio: ", no_audio, "\n"
                     "    no_video: ", no_video, "\n"
                     "    force_transcode: ", force_transcode, "\n",
                     "    force_transcode_audio: ", force_transcode_audio, "\n"
                     "    force_transcode_video: ", force_transcode_video, "\n"
                     "    aac_perfect_timestamp: ", aac_perfect_timestamp, "\n"
                     "    sync_to_clock: ", sync_to_clock, "\n"
                     "    send_metadata: ", send_metadata, "\n"
                     "    enable_prechunking: ", enable_prechunking, "\n"
                     "    default_width:   ", default_width, "\n"
                     "    default_height:  ", default_height, "\n"
                     "    default_bitrate: ", default_bitrate, "\n"
                     "    no_video_timeout: ", no_video_timeout, "\n");
        logUnlock ();
    }

    PlaybackItem ()
        : stream_spec (st_grab (new (std::nothrow) String)),
          spec_kind (SpecKind::None),

          no_audio (false),
          no_video (false),

          force_transcode (false),
          force_transcode_audio (false),
          force_transcode_video (false),

          aac_perfect_timestamp (false),
          sync_to_clock (true),

          send_metadata (false),
          enable_prechunking (true),

          default_width  (0),
          default_height (0),
          default_bitrate (500000),

          no_video_timeout (60)
    {
    }
};

class OverlayDesc : public Referenced
{
public:
    Ref<String> overlay_channel_name;
    bool        pause_on_overlay;
};

class OverlayOptions : public Referenced
{
public:
    List< Ref<OverlayDesc> > overlay_list;
};

class ChannelOptions : public Referenced
{
public:
  mt_const
  mt_begin
    StRef<String> channel_name;
    StRef<String> channel_title;
    StRef<String> channel_desc;

    Ref<PlaybackItem> default_item;

    Ref<OverlayOptions> overlay_opts;

    bool          keep_video_stream;
    bool          continuous_playback;

    bool          recording;
    StRef<String> record_path;

    bool          connect_on_demand;
    Time          connect_on_demand_timeout;

    Time          no_video_timeout;
    Time          min_playlist_duration_sec;
  mt_end

    void dump ()
    {
        logLock ();
        logs->print ("    channel_name:  ", channel_name, "\n"
                     "    channel_title: ", channel_title, "\n"
                     "    channel_desc:  ", channel_desc, "\n"
                     "    keep_video_stream:   ", keep_video_stream, "\n"
                     "    continuous_playback: ", continuous_playback, "\n"
                     "    recording:   ", recording, "\n"
                     "    record_path: ", record_path, "\n"
                     "    connect_on_demand: ", connect_on_demand, "\n"
                     "    connect_on_demand_timeout: ", connect_on_demand_timeout, "\n"
                     "    no_video_timeout: ", no_video_timeout, "\n"
                     "    min_playlist_duration_sec: ", min_playlist_duration_sec, "\n");
        logUnlock ();
    }

    ChannelOptions ()
        : channel_name  (st_grab (new (std::nothrow) String)),
          channel_title (st_grab (new (std::nothrow) String)),
          channel_desc  (st_grab (new (std::nothrow) String)),
            
          keep_video_stream (false),
          continuous_playback (false),

          recording (false),
          record_path (st_grab (new (std::nothrow) String)),

          connect_on_demand (false),
          connect_on_demand_timeout (60),

          no_video_timeout (60),
          min_playlist_duration_sec (10)
    {
    }
};

}


#endif /* MOMENT__CHANNEL_OPTIONS__H__ */

