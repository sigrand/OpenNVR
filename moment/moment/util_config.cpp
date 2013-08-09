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


#include <moment/libmoment.h>

#include <moment/util_config.h>


using namespace M;

namespace Moment {

Result
parseOverlayConfig (MConfig::Section * const mt_nonnull channel_section,
                    ChannelOptions   * const mt_nonnull opts)
{
    MConfig::Section * const overlay_section = channel_section->getSection ("overlay");
    if (!overlay_section)
        return Result::Success;

    if (!opts->overlay_opts)
        opts->overlay_opts = grab (new (std::nothrow) OverlayOptions);

    MConfig::Section::iterator iter (*overlay_section);
    while (!iter.done()) {
        bool pause_on_overlay = true;

        MConfig::SectionEntry * const sect_entry = iter.next ();
        if (sect_entry->getType() == MConfig::SectionEntry::Type_Section) {
            MConfig::Section * const section = static_cast <MConfig::Section*> (sect_entry);

            if (!configSectionGetBoolean (section, "pause", &pause_on_overlay, pause_on_overlay))
                return Result::Failure;
        }

        Ref<OverlayDesc> const overlay_desc = grab (new (std::nothrow) OverlayDesc);
        overlay_desc->overlay_channel_name = grab (new (std::nothrow) String (sect_entry->getName()));
        overlay_desc->pause_on_overlay = pause_on_overlay;

        opts->overlay_opts->overlay_list.append (overlay_desc);
    }

    return Result::Success;
}

static ConstMemory itemToStreamName (ConstMemory const item_name)
{
    ConstMemory stream_name = item_name;
    for (Size i = stream_name.len(); i > 0; --i) {
        if (stream_name.mem() [i - 1] == '.') {
            stream_name = stream_name.region (0, i - 1);
            break;
        }
    }

    return stream_name;
}

Result
parseChannelConfig (MConfig::Section * const mt_nonnull section,
                    ConstMemory        const config_item_name,
                    ChannelOptions   * const default_opts,
                    ChannelOptions   * const mt_nonnull opts,
                    PlaybackItem     * const mt_nonnull item)
{
    char const opt_name__name []                     = "name";
    char const opt_name__title[]                     = "title";
    char const opt_name__desc []                     = "desc";
    char const opt_name__chain[]                     = "chain";
    char const opt_name__uri  []                     = "uri";
    char const opt_name__playlist[]                  = "playlist";
    char const opt_name__master[]                    = "master";
    char const opt_name__keep_video_stream[]         = "keep_video_stream";
    char const opt_name__continuous_playback[]       = "continous_playback";
    char const opt_name__record_path[]               = "record_path";
    char const opt_name__connect_on_demand[]         = "connect_on_demand";
    char const opt_name__connect_on_demand_timeout[] = "connect_on_demand_timeout";
    char const opt_name__no_video_timeout[]          = "no_video_timeout";
    char const opt_name__min_playlist_duration[]     = "min_playlist_duration";
    char const opt_name__no_audio[]                  = "no_audio";
    char const opt_name__no_video[]                  = "no_video";
    char const opt_name__force_transcode[]           = "force_transcode";
    char const opt_name__force_transcode_audio[]     = "force_transcode_audio";
    char const opt_name__force_transcode_video[]     = "force_transcode_video";
    char const opt_name__aac_perfect_timestamp[]     = "aac_perfect_timestamp";
    char const opt_name__sync_to_clock[]             = "sync_to_clock";
    char const opt_name__send_metadata[]             = "send_metadata";
    char const opt_name__enable_prechunking[]        = "enable_prechunking";
// TODO resize and transcoding settings
#if 0
    char const opt_name__width[]                     = "width";
    char const opt_name__height[]                    = "height";
    char const opt_name__bitrate[]                   = "bitrate";
#endif

    ConstMemory channel_name = itemToStreamName (config_item_name);
    {
        MConfig::Section::attribute_iterator attr_iter (*section);
        MConfig::Attribute *name_attr = NULL;
        if (!attr_iter.done()) {
            MConfig::Attribute * const attr = attr_iter.next ();
            if (!attr->hasValue()) {
                channel_name = attr->getName();
                name_attr = attr;
            }
        }

        if (MConfig::Attribute * const attr = section->getAttribute (opt_name__name)) {
            if (attr != name_attr)
                channel_name = attr->getValue ();
        }
    }
    logD_ (_func, opt_name__name, ": ", channel_name);

    ConstMemory channel_title = channel_name;
    if (MConfig::Option * const opt = section->getOption (opt_name__title)) {
        if (opt->getValue())
            channel_title = opt->getValue()->mem();
    }
    logD_ (_func, opt_name__title, ": ", channel_title);

    ConstMemory channel_desc = default_opts->channel_desc->mem();
    if (MConfig::Option * const opt = section->getOption (opt_name__desc)) {
        if (opt->getValue())
            channel_desc = opt->getValue()->mem();
    }
    logD_ (_func, opt_name__desc, ": ", channel_desc);

    PlaybackItem::SpecKind spec_kind = PlaybackItem::SpecKind::None;
    ConstMemory stream_spec;
    {
        int num_set_opts = 0;

        if (MConfig::Option * const opt = section->getOption (opt_name__chain)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Chain;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__chain, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__uri)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Uri;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__uri, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__playlist)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Playlist;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__playlist, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__master)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Slave;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__master, ": ", stream_spec);
        }

        if (num_set_opts > 1)
            logW_ (_func, "only one of uri/chain/playlist options should be specified");
    }

    bool keep_video_stream = default_opts->keep_video_stream;
    if (!configSectionGetBoolean (section,
                                  opt_name__keep_video_stream,
                                  &keep_video_stream,
                                  keep_video_stream))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__keep_video_stream, ": ", keep_video_stream);

    bool continuous_playback = default_opts->continuous_playback;
    if (!configSectionGetBoolean (section,
                                  opt_name__continuous_playback,
                                  &continuous_playback,
                                  continuous_playback))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__continuous_playback, ": ", continuous_playback);

    ConstMemory record_path = default_opts->record_path->mem();
    bool got_record_path = false;
    if (MConfig::Option * const opt = section->getOption (opt_name__record_path)) {
        if (opt->getValue()) {
            record_path = opt->getValue()->mem();
            got_record_path = true;
        }
    }
    logD_ (_func, opt_name__record_path, ": ", record_path);

    bool connect_on_demand = default_opts->connect_on_demand;
    if (!configSectionGetBoolean (section,
                                  opt_name__connect_on_demand,
                                  &connect_on_demand,
                                  connect_on_demand))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__connect_on_demand, ": ", connect_on_demand);

    Uint64 connect_on_demand_timeout = default_opts->connect_on_demand_timeout;
    if (!configSectionGetUint64 (section,
                                 opt_name__connect_on_demand_timeout,
                                 &connect_on_demand_timeout,
                                 connect_on_demand_timeout))
    {
        return Result::Failure;
    }

    Uint64 no_video_timeout = default_opts->no_video_timeout;
    if (!configSectionGetUint64 (section,
                                 opt_name__no_video_timeout,
                                 &no_video_timeout,
                                 no_video_timeout))
    {
        return Result::Failure;
    }

    Uint64 min_playlist_duration = default_opts->min_playlist_duration_sec;
    if (!configSectionGetUint64 (section,
                                 opt_name__min_playlist_duration,
                                 &min_playlist_duration,
                                 min_playlist_duration))
    {
        return Result::Failure;
    }

// TODO PushAgent    ConstMmeory push_uri;

    bool no_audio = default_opts->default_item->no_audio;
    if (!configSectionGetBoolean (section, opt_name__no_audio, &no_audio, no_audio))
        return Result::Failure;
    logD_ (_func, opt_name__no_audio, ": ", no_audio);

    bool no_video = default_opts->default_item->no_video;
    if (!configSectionGetBoolean (section, opt_name__no_video, &no_video, no_video))
        return Result::Failure;
    logD_ (_func, opt_name__no_video, ": ", no_video);

    bool force_transcode = default_opts->default_item->force_transcode;
    if (!configSectionGetBoolean (section,
                                  opt_name__force_transcode,
                                  &force_transcode,
                                  force_transcode))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__force_transcode, ": ", force_transcode);

    bool force_transcode_audio = default_opts->default_item->force_transcode_audio;
    if (!configSectionGetBoolean (section,
                                  opt_name__force_transcode_audio,
                                  &force_transcode_audio,
                                  force_transcode_audio))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__force_transcode_audio, ": ", force_transcode_audio);

    bool force_transcode_video = default_opts->default_item->force_transcode_video;
    if (!configSectionGetBoolean (section,
                                  opt_name__force_transcode_video,
                                  &force_transcode_video,
                                  force_transcode_video))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__force_transcode_video, ": ", force_transcode_video);

    bool aac_perfect_timestamp = default_opts->default_item->aac_perfect_timestamp;
    if (!configSectionGetBoolean (section,
                                  opt_name__aac_perfect_timestamp,
                                  &aac_perfect_timestamp,
                                  aac_perfect_timestamp))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__aac_perfect_timestamp, ": ", aac_perfect_timestamp);

    bool sync_to_clock = default_opts->default_item->sync_to_clock;
    if (!configSectionGetBoolean (section,
                                  opt_name__sync_to_clock,
                                  &sync_to_clock,
                                  sync_to_clock))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__sync_to_clock, ": ", sync_to_clock);

    bool send_metadata = default_opts->default_item->send_metadata;
    if (!configSectionGetBoolean (section,
                                  opt_name__send_metadata,
                                  &send_metadata,
                                  send_metadata))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__send_metadata, ": ", send_metadata);

    bool enable_prechunking = default_opts->default_item->enable_prechunking;
    if (!configSectionGetBoolean (section,
                                  opt_name__enable_prechunking,
                                  &enable_prechunking,
                                  enable_prechunking))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__enable_prechunking, ": ", enable_prechunking);

    opts->channel_name  = st_grab (new (std::nothrow) String (channel_name));
    opts->channel_title = st_grab (new (std::nothrow) String (channel_title));
    opts->channel_desc  = st_grab (new (std::nothrow) String (channel_desc));

    opts->keep_video_stream  = keep_video_stream;
    opts->continuous_playback = continuous_playback;

    opts->recording = got_record_path;
    opts->record_path = st_grab (new (std::nothrow) String (record_path));

    opts->connect_on_demand = connect_on_demand;
    opts->connect_on_demand_timeout = connect_on_demand_timeout;

    opts->no_video_timeout = no_video_timeout;
    opts->min_playlist_duration_sec = min_playlist_duration;

    item->stream_spec = st_grab (new (std::nothrow) String (stream_spec));
    item->spec_kind = spec_kind;

    item->no_audio = no_audio;
    item->no_video = no_video;

    item->force_transcode = force_transcode;
    item->force_transcode_audio = force_transcode_audio;
    item->force_transcode_video = force_transcode_video;

    item->aac_perfect_timestamp = aac_perfect_timestamp;
    item->sync_to_clock = sync_to_clock;

    item->send_metadata = /* TODO send_metadata */ false;
    item->enable_prechunking = enable_prechunking;

    if (!parseOverlayConfig (section, opts ))
        return Result::Failure;

    return Result::Success;
}

}

