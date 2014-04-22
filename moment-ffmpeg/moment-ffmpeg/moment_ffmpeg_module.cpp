/*  Moment-FFmpeg - FFmpeg support module for Moment Video Server
    Copyright (C) 2011-2013 Dmitry Shatrov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include <cctype>
#ifndef PLATFORM_WIN32
#include <mntent.h>
#include <sys/statvfs.h>
#endif

#include <libmary/types.h>
#include <moment/libmoment.h>

#include <moment-ffmpeg/memory_dispatcher.h>
#include <moment-ffmpeg/moment_ffmpeg_module.h>
#include <moment-ffmpeg/video_part_maker.h>


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

#define OLDPOINTS_LIVE 864000            // 10 days
#define TIMER_REFRESH_STAT 300          // 5 minutes
#define TIMER_STATMEASURER 5             // 5 seconds
#define DOWNLOAD_LIMIT 3600             // 1 hour
#define TIMER_UPDATE_SOURCES_TIMES 2    // 2 seconds

static LogGroup libMary_logGroup_ffmpeg_module ("mod_ffmpeg.ffmpeg_module", LogLevel::E);

extern std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
extern std::vector<std::string> split(const std::string &s, char delim);

static StRef<String> this_rtmp_server_addr;
static StRef<String> this_rtmpt_server_addr;
static StRef<String> this_hls_server_addr;

Result
MomentFFmpegModule::updatePlaylist (ConstMemory   const channel_name,
				 bool          const keep_cur_item,
				 Ref<String> * const mt_nonnull ret_err_msg)
{
    logD(ffmpeg_module, _func, "channel_name: ", channel_name);

    if(ret_err_msg == NULL)
    {
        logE_(_func_, "ret_err_msg is null");
        return Result::Failure;
    }

    m_mutex.lock ();

    ChannelEntry * const channel_entry = m_channel_entry_hash.lookup (channel_name);
    if (!channel_entry) {
    m_mutex.unlock ();
	Ref<String> const err_msg = makeString ("Channel not found: ", channel_name);
	logE_ (_func, err_msg);
	*ret_err_msg = err_msg;
	return Result::Failure;
    }

    if (!channel_entry->playlist_filename) {
    m_mutex.unlock ();
	Ref<String> const err_msg = makeString ("No playlist for channel \"", channel_name, "\"");
	logE_ (_func, err_msg);
	*ret_err_msg = err_msg;
	return Result::Failure;
    }

    Ref<String> err_msg;
    if (!channel_entry->channel->getPlayback()->loadPlaylistFile (
		channel_entry->playlist_filename->mem(),
                keep_cur_item,
                channel_entry->channel_opts->default_item,
                &err_msg))
    {
    m_mutex.unlock ();
	logE_ (_func, "channel->loadPlaylistFile() failed: ", err_msg);
	*ret_err_msg = makeString ("Playlist parsing error: ", err_msg->mem());
	return Result::Failure;
    }

    m_mutex.unlock ();

    return Result::Success;
}

Result
MomentFFmpegModule::setPosition (ConstMemory const channel_name,
			      ConstMemory const item_name,
			      bool        const item_name_is_id,
			      ConstMemory const seek_str)
{
    Time seek;
    if (!parseDuration (seek_str, &seek)) {
	logE_ (_func, "Couldn't parse seek time: ", seek_str);
	return Result::Failure;
    }

    m_mutex.lock ();

    ChannelEntry * const channel_entry = m_channel_entry_hash.lookup (channel_name);
    if (!channel_entry) {
    m_mutex.unlock ();
	logE_ (_func, "Channel not found: ", channel_name);
	return Result::Failure;
    }

    Result res;
    if (item_name_is_id) {
	res = channel_entry->channel->getPlayback()->setPosition_Id (item_name, seek);
    } else {
	Uint32 item_idx;
	if (!strToUint32_safe (item_name, &item_idx)) {
        m_mutex.unlock ();
	    logE_ (_func, "Failed to parse item index");
	    return Result::Failure;
	}

	res = channel_entry->channel->getPlayback()->setPosition_Index (item_idx, seek);
    }

    if (!res) {
    m_mutex.unlock ();
	logE_ (_func, "Item not found: ", item_name, item_name_is_id ? " (id)" : " (idx)", ", channel: ", channel_name);
	return Result::Failure;
    }

    m_mutex.unlock ();

    return Result::Success;
}

void
MomentFFmpegModule::createPlaylistChannel (ConstMemory      const playlist_filename,
                                        bool             const is_dir,
                                        bool             const dir_re_read,
                                        ChannelOptions * const channel_opts,
                                        PushAgent      * const push_agent,
                                        FetchAgent     * const fetch_agent)
{
    if(!channel_opts || !push_agent || !fetch_agent)
    {
        logE_(_func_, "channel_opts is ", channel_opts != NULL);
        logE_(_func_, "push_agent is ", push_agent != NULL);
        logE_(_func_, "fetch_agent is ", fetch_agent != NULL);
        return;
    }
	
    ChannelEntry * const channel_entry = new (std::nothrow) ChannelEntry;
    assert (channel_entry);
    if(!channel_entry){logE_(_func_, "cannot allocate ChannelEntry");return;}
	
    channel_entry->channel_opts = channel_opts;

    channel_entry->channel_name  = grab (new (std::nothrow) String (channel_opts->channel_name->mem()));
    if(!channel_entry->channel_name){logE_(_func_, "cannot allocate channel_name");return;}
    channel_entry->channel_title = grab (new (std::nothrow) String (channel_opts->channel_title->mem()));
    if(!channel_entry->channel_title){logE_(_func_, "cannot allocate channel_title");return;}
    channel_entry->channel_desc  = grab (new (std::nothrow) String (channel_opts->channel_desc->mem()));
    if(!channel_entry->channel_desc){logE_(_func_, "cannot allocate channel_desc");return;}
    channel_entry->playlist_filename = grab (new (std::nothrow) String (playlist_filename));
    if(!channel_entry->playlist_filename){logE_(_func_, "cannot allocate playlist_filename");return;}

    channel_entry->push_agent  = push_agent;
    channel_entry->fetch_agent = fetch_agent;

    Ref<Channel> const channel = grab (new (std::nothrow) Channel);
    if(!channel){logE_(_func_, "cannot allocate channel");return;}
    channel_entry->channel = channel;

    channel->init (m_pMoment, channel_opts);

    m_mutex.lock ();
    m_channel_entry_hash.add (channel_entry);
    m_mutex.unlock ();
    m_channel_set.addChannel (channel, channel_opts->channel_name->mem());

    if (is_dir) {
        if (!channel->getPlayback()->loadPlaylistDirectory (playlist_filename,
                                                            dir_re_read,
                                                            false /* keep_cur_item */,
                                                            channel_opts->default_item))
        {
            logE_ (_func, "Could not create playlist for directory \"", playlist_filename, "\"");
        }
    } else {
	Ref<String> err_msg;
	if (!channel->getPlayback()->loadPlaylistFile (playlist_filename,
                                                       false /* keep_cur_item */,
                                                       channel_opts->default_item,
                                                       &err_msg))
        {
	    logE_ (_func, "Could not parse playlist file \"", playlist_filename, "\":\n", err_msg);
        }
    }

    if (channel_opts->recording) {
	createChannelRecorder (channel_opts->channel_name->mem(),
                               channel_opts->channel_name->mem(),
                               channel_opts->record_path->mem());
    }
}

void
MomentFFmpegModule::createStreamChannel (ChannelOptions * const channel_opts,
                                      PlaybackItem   * const playback_item,
                                      PushAgent      * const push_agent,
                                      FetchAgent     * const fetch_agent)
{
    if(!channel_opts || !playback_item || !push_agent || !fetch_agent)
    {
        logE_(_func_, "channel_opts is ", channel_opts != NULL);
        logE_(_func_, "push_agent is ", push_agent != NULL);
        logE_(_func_, "playback_item is ", playback_item != NULL);
        logE_(_func_, "fetch_agent is ", fetch_agent != NULL);
        return;
    }
	
    ChannelEntry * const channel_entry = new (std::nothrow) ChannelEntry;
    assert (channel_entry);
    if(!channel_entry){logE_(_func_, "cannot allocate ChannelEntry");return;}
	
    channel_entry->channel_opts = channel_opts;

    channel_entry->channel_name  = grab (new (std::nothrow) String (channel_opts->channel_name->mem()));
    if(!channel_entry->channel_name){logE_(_func_, "cannot allocate channel_entry->channel_name");return;}
    channel_entry->channel_title = grab (new (std::nothrow) String (channel_opts->channel_title->mem()));
    if(!channel_entry->channel_title){logE_(_func_, "cannot allocate channel_entry->channel_title");return;}
    channel_entry->channel_desc  = grab (new (std::nothrow) String (channel_opts->channel_desc->mem()));
    if(!channel_entry->channel_desc){logE_(_func_, "cannot allocate channel_entry->channel_desc");return;}
    channel_entry->playlist_filename = NULL;

    channel_entry->push_agent  = push_agent;
    channel_entry->fetch_agent = fetch_agent;

    Ref<Channel> const channel = grab (new (std::nothrow) Channel);
    channel_entry->channel = channel;

    channel->init (m_pMoment, channel_opts);

    m_mutex.lock ();
    m_channel_entry_hash.add (channel_entry);
    m_mutex.unlock ();
    m_channel_set.addChannel (channel, channel_opts->channel_name->mem());

    channel->getPlayback()->setSingleItem (playback_item);

    if (channel_opts->recording) {
	createChannelRecorder (channel_opts->channel_name->mem(),
                               channel_opts->channel_name->mem(),
                               channel_opts->record_path->mem());
    }
}

void
MomentFFmpegModule::createDummyChannel (ConstMemory   const channel_name,
                                     ConstMemory   const channel_title,
				     ConstMemory   const channel_desc,
                                     PushAgent   * const push_agent,
                                     FetchAgent  * const fetch_agent)
{
    if(!push_agent || !fetch_agent)
        return;
	
    ChannelEntry * const channel_entry = new (std::nothrow) ChannelEntry;
    assert (channel_entry);
    if(!channel_entry){logE_(_func_, "cannot allocate ChannelEntry");return;}

    channel_entry->channel_name  = grab (new (std::nothrow) String (channel_name));
    if(!channel_entry->channel_name){logE_(_func_, "cannot allocate channel_entry->channel_name");return;}
    channel_entry->channel_title = grab (new (std::nothrow) String (channel_title));
    if(!channel_entry->channel_title){logE_(_func_, "cannot allocate channel_entry->channel_title");return;}
    channel_entry->channel_desc  = grab (new (std::nothrow) String (channel_desc));
    if(!channel_entry->channel_desc){logE_(_func_, "cannot allocate channel_entry->channel_desc");return;}
    channel_entry->playlist_filename = NULL;

    channel_entry->push_agent  = push_agent;
    channel_entry->fetch_agent = fetch_agent;

    Ref<Channel> const channel = grab (new (std::nothrow) Channel);
    if(!channel){logE_(_func_, "cannot allocate channel");return;}
    channel_entry->channel = channel;

    Ref<ChannelOptions> const opts = grab (new (std::nothrow) ChannelOptions);
    if(!opts){logE_(_func_, "cannot allocate opts");return;}
    channel_entry->channel_opts = opts;
    {
        *opts = *m_default_channel_opts;
        opts->channel_name  = st_grab (new (std::nothrow) String (channel_name));
        opts->channel_title = st_grab (new (std::nothrow) String (channel_title));
        opts->channel_desc  = st_grab (new (std::nothrow) String (channel_desc));
    }

    Ref<PlaybackItem> const item = grab (new (std::nothrow) PlaybackItem);
    if(!item){logE_(_func_, "cannot allocate item");return;}
    opts->default_item = item;
    {
        *item = *m_default_channel_opts->default_item;

        item->stream_spec = st_grab (new (std::nothrow) String);
        item->spec_kind = PlaybackItem::SpecKind::None;

        item->force_transcode = false;
        item->force_transcode_video = false;
        item->force_transcode_audio = false;
    }

    channel->init (m_pMoment, opts);

    m_mutex.lock ();
    m_channel_entry_hash.add (channel_entry);
    m_mutex.unlock ();
    m_channel_set.addChannel (channel, channel_name);

    if (!fetch_agent)
        channel->getPlayback()->setSingleItem (item);
}

void
MomentFFmpegModule::createPlaylistRecorder (ConstMemory const recorder_name,
					 ConstMemory const playlist_filename,
					 ConstMemory const filename_prefix)
{
    RecorderEntry * const recorder_entry = new (std::nothrow) RecorderEntry;
    if(!recorder_entry){logE_(_func_, "cannot allocate recorder_entry");return;}

    recorder_entry->recorder_name = grab (new (std::nothrow) String (recorder_name));
    if(!recorder_entry->recorder_name){logE_(_func_, "cannot allocate recorder_entry->recorder_name");return;}
    recorder_entry->playlist_filename  = grab (new (std::nothrow) String (playlist_filename));
    if(!recorder_entry->playlist_filename){logE_(_func_, "cannot allocate recorder_entry->playlist_filename");return;}

    Ref<Recorder> const recorder = grab (new Recorder);
    if(!recorder){logE_(_func_, "cannot allocate recorder");return;}
    recorder_entry->recorder = recorder;

    recorder->init (m_pMoment,
            m_pPage_pool,
            &m_channel_set,
		    filename_prefix,
                    m_default_channel_opts->min_playlist_duration_sec);

    m_mutex.lock ();
    m_recorder_entry_hash.add (recorder_entry);
    m_mutex.unlock ();

    {
	Ref<String> err_msg;
	if (!recorder->loadPlaylistFile (playlist_filename,
                                         false /* keep_cur_item */,
                                         m_default_channel_opts->default_item,
                                         &err_msg))
        {
	    logE_ (_func, "Could not parse recorder playlist file \"", playlist_filename, "\":\n", err_msg);
        }
    }
}

void
MomentFFmpegModule::createChannelRecorder (ConstMemory const recorder_name,
					ConstMemory const channel_name,
					ConstMemory const filename_prefix)
{
    RecorderEntry * const recorder_entry = new RecorderEntry;

    recorder_entry->recorder_name = grab (new String (recorder_name));
    recorder_entry->playlist_filename = NULL;

    Ref<Recorder> const recorder = grab (new Recorder);
    recorder_entry->recorder = recorder;

    recorder->init (m_pMoment,
            m_pPage_pool,
            &m_channel_set,
		    filename_prefix,
                    m_default_channel_opts->min_playlist_duration_sec);

    m_mutex.lock ();
    m_recorder_entry_hash.add (recorder_entry);
    m_mutex.unlock ();

    recorder->setSingleChannel (channel_name);
}

void
MomentFFmpegModule::printChannelInfoJson (PagePool::PageListHead * const page_list,
				       ChannelEntry           * const channel_entry)
{
    if(!page_list || !channel_entry)
    {
        logE_(_func_, "page_list is ", page_list != NULL);
        logE_(_func_, "channel_entry is ", channel_entry != NULL);
        return;
    }
	
    static char const open_str []  = "[ \"";
    m_pPage_pool->getFillPages (page_list, open_str);

    m_pPage_pool->getFillPages (page_list, channel_entry->channel_name->mem());

    static char const sep_a [] = "\", \"";
    m_pPage_pool->getFillPages (page_list, sep_a);

    m_pPage_pool->getFillPages (page_list, channel_entry->channel_desc->mem());

    m_pPage_pool->getFillPages (page_list, sep_a);

    {
	if (channel_entry->channel
	    && channel_entry->channel->isSourceOnline())
	{
        m_pPage_pool->getFillPages (page_list, "ONLINE");
	} else {
        m_pPage_pool->getFillPages (page_list, "OFFLINE");
	}
    }

    static char const close_str [] = "\" ]";
    m_pPage_pool->getFillPages (page_list, close_str);
}

StRef<String>
MomentFFmpegModule::channelFilesExistenceToJson (
        ChannelChecker::ChannelFileDiskTimes * const mt_nonnull chFileDiskTimes)
{
    if(!chFileDiskTimes)
    {
        logE_(_func_, "chFileDiskTimes is NULL");
        return NULL;
    }
	
     std::ostringstream s;
	 
     for(ChannelChecker::ChannelFileDiskTimes::iterator it = chFileDiskTimes->begin(); it != chFileDiskTimes->end(); ++it)
     {
         s << "{\n\"path\":\"";
         s << (*it).first;
         s <<"\",\n\"start\":";
         s << (*it).second.times.timeStart;
         s << ",\n\"end\":";
         s << (*it).second.times.timeEnd;
         s << "\n}";
        ChannelChecker::ChannelFileDiskTimes::iterator it1 = it;
        it1++;
         if(it1 != chFileDiskTimes->end())
             s << ",\n";
         else
             s << "\n";
     }

     return st_makeString("{\n"
                          "\"filesSummary\" : [\n",
                          s.str().c_str(),
                          "]\n"
                          "}\n");
}

StRef<String>
MomentFFmpegModule::statisticsToJson (
        std::map<time_t, StatMeasure> * const mt_nonnull statPoints)
{
    if(!statPoints)
    {
        logE_(_func_, "statPoints is NULL");
        return NULL;
    }

     std::ostringstream s;
	 
     for(std::map<time_t, StatMeasure>::iterator it = statPoints->begin(); it != statPoints->end(); ++it)
     {
         s << "{\n\"time\":\"";
         struct tm * timeinfo;
         timeinfo = localtime (&(*it).first);
         s << timeinfo->tm_year + 1900 << "." \
                         << timeinfo->tm_mon + 1 << "." << timeinfo->tm_mday \
                         << " " << timeinfo->tm_hour << ":" << timeinfo->tm_min \
                         << ":" << timeinfo->tm_sec << "\",\n";

         if((*it).second.packetAmountInOut != 0)
         {
             s << "\"packet time from ffmpeg to restreamer\":{\n";
             s << "\"min\":\"" << (*it).second.minInOut << "\",\n";
             s << "\"max\":\"" << (*it).second.maxInOut << "\",\n";
             s << "\"avg\":\"" << (*it).second.avgInOut << "\",\n";
             s << "\"avg amount\":\"" << (*it).second.packetAmountInOut << "\"},\n";
         }

         if((*it).second.packetAmountInNvr != 0)
         {
             s << "\"packet time from ffmpeg to nvr\":{\n";
             s << "\"min\":\"" << (*it).second.minInNvr << "\",\n";
             s << "\"max\":\"" << (*it).second.maxInNvr << "\",\n";
             s << "\"avg\":\"" << (*it).second.avgInNvr << "\",\n";
             s << "\"avg amount\":\"" << (*it).second.packetAmountInNvr << "\"},\n";
         }

         s << "\"RAM utilization\":{\n";
         s << "\"min\":\"" << (*it).second.minRAM << "\",\n";
         s << "\"max\":\"" << (*it).second.maxRAM << "\",\n";
         s << "\"avg\":\"" << (*it).second.avgRAM << "\"},\n";

         s << "\"CPU utilization\":{\n";
         s << "\"min\":\"" << (*it).second.user_util_min << "\",\n";
         s << "\"max\":\"" << (*it).second.user_util_max << "\",\n";
         s << "\"avg\":\"" << (*it).second.user_util_avg << "\"},\n";

         s << "\"HDD utilization\":[\n";
         for(int i=0;i<(*it).second.devnames.size();i++)
         {
             s << "{\"devname\":\"" << (*it).second.devnames[i] << "\",\n";
             s << "\"util\":\"" << (*it).second.hdd_utils[i] << "\"}";
             if(i+1 != (*it).second.devnames.size())
             {
                 s << ",\n";
             }
             else
             {
                 s << "]\n";
             }
         }

         std::map<time_t, StatMeasure>::iterator it1 = it;
         it1++;
         if(it1 != statPoints->end())
             s << "},\n";
         else
             s << "}\n";
     }

     return st_makeString("{\n"
                          "\"statistics\" : [\n",
                          s.str().c_str(),
                          "]\n"
                          "}\n");
}

Json::Value
MomentFFmpegModule::sourceInfoToJson (const std::string source_name, const stSourceInfo & si, Ref<ChannelChecker> pChannelChecker)
{
    if(!pChannelChecker || !source_name.size())
    {
        return Json::Value();
    }

    Json::Value json_source;

    json_source["name"] = si.sourceName;
    json_source["uri"] = si.uri;
    json_source["title"] = si.title;

    // stream info
    Json::Value json_streams;
    for(int i=0;i<si.videoStreams.size();i++)
    {
        Json::Value json_stream;
        json_stream["type"] = si.videoStreams[i].streamInfo.streamType;
        json_stream["codec name"] = si.videoStreams[i].streamInfo.codecName;
        json_stream["profile"] = si.videoStreams[i].streamInfo.profile;
        json_stream["fps"] = si.videoStreams[i].fps;
        json_stream["width"] = si.videoStreams[i].width;
        json_stream["height"] = si.videoStreams[i].height;
        json_streams.append(json_stream);
    }
    for(int i=0;i<si.audioStreams.size();i++)
    {
        Json::Value json_stream;
        json_stream["type"] = si.audioStreams[i].streamInfo.streamType;
        json_stream["codec name"] = si.audioStreams[i].streamInfo.codecName;
        json_stream["profile"] = si.audioStreams[i].streamInfo.profile;
        json_stream["sample rate"] = si.audioStreams[i].sampleRate;
        json_streams.append(json_stream);
    }
    for(int i=0;i<si.otherStreams.size();i++)
    {
        Json::Value json_stream;
        json_stream["type"] = si.otherStreams[i].streamInfo.streamType;
        json_stream["codec name"] = si.otherStreams[i].streamInfo.codecName;
        json_stream["profile"] = si.otherStreams[i].streamInfo.profile;
        json_streams.append(json_stream);
    }
    json_source["stream info"] = json_streams;

    // times
    SourceStateTimes sst = m_sourceStateTimes[source_name];
    Json::Value json_times;

    Json::Value json_live_times;
    for(int i=0;i<sst.times.live.size();i++)
    {
        Json::Value json_live_time;
        json_live_time["start time"] = Json::Int(sst.times.live[i].timeStart);
        json_live_time["end time"] = Json::Int(sst.times.live[i].timeEnd);
        json_live_times.append(json_live_time);
    }
    json_times["live times"] = json_live_times;

    Json::Value json_restr_times;
    for(int i=0;i<sst.times.restr.size();i++)
    {
        Json::Value json_restr_time;
        json_restr_time["start time"] = Json::Int(sst.times.restr[i].timeStart);
        json_restr_time["end time"] = Json::Int(sst.times.restr[i].timeEnd);
        json_restr_times.append(json_restr_time);
    }
    json_times["restr times"] = json_restr_times;

    Json::Value json_write_times;
    for(int i=0;i<sst.times.write.size();i++)
    {
        Json::Value json_write_time;
        json_write_time["start time"] = Json::Int(sst.times.write[i].timeStart);
        json_write_time["end time"] = Json::Int(sst.times.write[i].timeEnd);
        json_write_times.append(json_write_time);
    }
    json_times["write times"] = json_write_times;

    json_source["times"] = json_times;

    // disk occupation
    ChannelChecker::DiskSizes diskSizes = pChannelChecker->GetDiskSizes ();
    Json::Value json_disks;

    for(ChannelChecker::DiskSizes::iterator itr = diskSizes.begin(); itr != diskSizes.end(); itr++)
    {
        if(itr->second > 0)
        {
            Json::Value json_disk;
            json_disk["name"] = itr->first;
            json_disk["size"] = Json::UInt64(itr->second);
            json_disks.append(json_disk);
        }
    }
    json_source["disks"] = json_disks;

    return json_source;
}

std::string
MomentFFmpegModule::GetAllSourcesInfo ()
{
    logD(ffmpeg_module, _func_);

    m_mutex.lock();

    std::map<std::string, WeakRef<FFmpegStream> >::iterator itFFStream = m_streams.begin();
    Json::Value json_root;
    Json::Value json_sources;

    for(itFFStream; itFFStream != m_streams.end(); itFFStream++)
    {
        stSourceInfo si = itFFStream->second.getRefPtr()->GetSourceInfo();
        Ref<ChannelChecker> channelChecker = itFFStream->second.getRefPtr()->GetChannelChecker();

        Json::Value json_source = sourceInfoToJson(itFFStream->first, si, channelChecker);

        if(!json_source.empty())
            json_sources.append(json_source);
    }

    json_root["sources"] = json_sources;

    m_mutex.unlock();

    Json::StyledWriter json_writer_styled;
    std::string json_respond = json_writer_styled.write(json_root);

    return json_respond;
}

std::string
MomentFFmpegModule::GetSourceInfo (const std::string & source_name)
{
    logD(ffmpeg_module, _func_);

    m_mutex.lock();

    if(m_streams.find(source_name) != m_streams.end())
    {
        stSourceInfo si = m_streams[source_name].getRefPtr()->GetSourceInfo();
        Ref<ChannelChecker> channelChecker = m_streams[source_name].getRefPtr()->GetChannelChecker();

        Json::Value json_source = sourceInfoToJson(source_name, si, channelChecker);

        m_mutex.unlock();

        Json::StyledWriter json_writer_styled;
        std::string json_respond = json_writer_styled.write(json_source);

        return json_respond;
    }
    else
    {
        m_mutex.unlock();

        return std::string();
    }
}


#ifndef PLATFORM_WIN32
int
MomentFFmpegModule::getDiskInfo(std::map<std::string, DiskSizes > & diskInfo)
{
    char const *table = "/etc/mtab";
    FILE *fp;

    fp = setmntent (table, "r");
    if (fp == NULL)
    {
        logE_(_func_, "fail to get disk info from ", table);
        return DiskInfoRetVal::ERR;
    }

    struct mntent *mnt;
    while ((mnt = getmntent (fp)))
    {
        struct statvfs info;
        if(statvfs (mnt->mnt_dir, &info))
        {
            logE_(_func_, "fail statvfs for ", mnt->mnt_dir);
            return DiskInfoRetVal::ERR;
        }

        if(info.f_frsize > 0 && info.f_blocks > 0)
        {
            DiskSizes diskSizes;
            diskSizes.allSize = (info.f_frsize * info.f_blocks) / 1024; // in KBytes
            diskSizes.freeSize = (info.f_frsize * info.f_bavail) / 1024; // in KBytes
            diskSizes.usedSize = diskSizes.allSize - (info.f_frsize * info.f_bfree) / 1024; // in KBytes

            diskInfo[std::string(mnt->mnt_dir)] = diskSizes;
        }
    }

    if (endmntent (fp) == 0)
    {
        logE_(_func_, "fail to close ", table);
        return DiskInfoRetVal::ERR;
    }

    return DiskInfoRetVal::NOERR;
}
#else /*PLATFORM_WIN32*/
int
MomentFFmpegModule::getDiskInfo(std::map<std::string, std::vector<int> > & diskInfo)
{
    // TODO: IMPLEMENT
    return -1;
}
#endif /*PLATFORM_WIN32*/


bool
MomentFFmpegModule::getDiskInfo (std::string & json_respond)
{	
    std::map<std::string, DiskSizes > diskInfo; // [diskName, [allSize, freeSize, usedSize]]
    int nRes = getDiskInfo(diskInfo);

    if(nRes == DiskInfoRetVal::NOTIMPLEMENTED)
    {
        logE_(_func_, "getDiskInfo isn't supported for current platform");
        return false;
    }
    else if(nRes == DiskInfoRetVal::ERR)
    {
        return false;
    }

    // writing to json

    Json::Value json_value_disks;

    Json::FastWriter json_writer_fast;
    Json::StyledWriter json_writer_styled;

    std::map<std::string, DiskSizes >::iterator it = diskInfo.begin();

    for(it; it != diskInfo.end(); ++it)
    {
        Json::Value json_value_disk;
        Json::Value json_value_diskInfo;

        json_value_disk["disk name"] = it->first;

        json_value_diskInfo["all size"] = Json::Value::UInt64(it->second.allSize);
        json_value_diskInfo["free size"] = Json::Value::UInt64(it->second.freeSize);
        json_value_diskInfo["used size"] = Json::Value::UInt64(it->second.usedSize);

        json_value_disk["disk info"] = json_value_diskInfo;

        logD(ffmpeg_module, _func_, "playlist.json line: ", json_writer_fast.write(json_value_disk).c_str());

        json_value_disks.append(json_value_disk);
    }

    json_respond = json_writer_styled.write(json_value_disks);

    return true;
}

bool
MomentFFmpegModule::removeVideoFiles(StRef<String> const channel_name,
                                 Time const startTime, Time const endTime)
{
    bool bRes = false;

    m_mutex.lock();

    std::map<std::string, WeakRef<FFmpegStream> >::iterator itFFStream = m_streams.find(std::string(channel_name->cstr()));
    if(itFFStream == m_streams.end())
    {
        logE_(_func_, "there is no ", channel_name, " in m_streams");
        return false;
    }
    Ref<ChannelChecker> channelChecker = itFFStream->second.getRefPtr()->GetChannelChecker();

    m_mutex.unlock();

    ChannelChecker::ChannelFileDiskTimes chFileDiskTimes = channelChecker->GetChannelFileDiskTimes();
    ChannelChecker::ChannelFileDiskTimes::iterator itr = chFileDiskTimes.begin();

    for(itr; itr != chFileDiskTimes.end(); itr++)
    {	
        if(itr->second.times.timeStart > startTime && itr->second.times.timeEnd < endTime)
        {
            StRef<String> dir_name = st_makeString(itr->second.diskName.c_str());
            Ref<Vfs> vfs = Vfs::createDefaultLocalVfs (dir_name->mem());

            StRef<String> const filenameFull = st_makeString(itr->first.c_str(), ".flv");

            logD(ffmpeg_module, _func_, "remove by request: [", filenameFull, "]");

            vfs->removeFile (filenameFull->mem());
            vfs->removeSubdirsForFilename (filenameFull->mem());

            channelChecker->DeleteFromCache(itr->second.diskName, itr->first);

            bRes = true;
        }
    }

    logD(ffmpeg_module,_func_, "end of removing");


    return bRes;
}

bool
MomentFFmpegModule::adminHttpRequest (HTTPServerRequest &req, HTTPServerResponse &resp, void * _self)
{
    MomentFFmpegModule * const self = static_cast <MomentFFmpegModule*> (_self);

    logD(ffmpeg_module, _func_);

    URI uri(req.getURI());
    std::vector < std::string > segments;
    uri.getPathSegments(segments);

    if (segments.size() == 2 &&
        (segments[1].compare("rec_on") == 0 || segments[1].compare("rec_off") == 0) )
    {
        HTMLForm form( req );

        NameValueCollection::ConstIterator channel_name_iter = form.find("stream");
        std::string channel_name = (channel_name_iter != form.end()) ? channel_name_iter->second: "";

        NameValueCollection::ConstIterator seq_iter = form.find("seq");
        std::string seq = (seq_iter != form.end()) ? seq_iter->second: "";

        StRef<String> st_channel_name = st_makeString(channel_name.c_str());

        bool channel_state = false; // false - isn't writing
        bool channelIsFound = false;
        bool const set_on = (segments[1].compare("rec_on") == 0);

        self->m_mutex.lock();

        std::map<std::string, WeakRef<FFmpegStream> >::iterator it = self->m_streams.find(st_channel_name->cstr());
        if(it != self->m_streams.end() && it->second.getRefPtr())
            channelIsFound = true;

        if (!channelIsFound)
        {
            self->m_mutex.unlock();

            resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
            std::ostream& out = resp.send();
            out << "404 Channel Not Found (mod_nvr_admin)";
            out.flush();

            logA(ffmpeg_module, _func_, "mod_nvr_admin 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        it->second.getRefPtr()->SetRecordingState(set_on);
        it->second.getRefPtr()->GetRecordingState(channel_state);

        self->m_mutex.unlock();

        bool bDisableRecordFound = false;
        StRef<String> channelFullPath = st_makeString(self->m_confd_dir, "/", st_channel_name);
        std::string line;
        std::string allLines = "";
        std::ifstream channelPath (channelFullPath->cstr());
        logD(ffmpeg_module, _func_, "channelFullPath = ", channelFullPath);
        if (channelPath.is_open())
        {
            logD(ffmpeg_module, _func_, "opened");
            while ( std::getline (channelPath, line) )
            {
                logD(ffmpeg_module, _func_, "got line: [", line.c_str(), "]");
                if(line.find("disable_record") == 0)
                {
                    std::string strLower = line;
                    std::transform(strLower.begin(), strLower.end(), strLower.begin(), ::tolower);
                    if(strLower.find("true") != std::string::npos)
                        bDisableRecordFound = true;
                }
                else
                    allLines += line + "\n";
            }
            channelPath.close();
        }

        if(bDisableRecordFound && set_on)
        {
            logD(ffmpeg_module, _func_, "bDisableRecordFound && set_on");
            std::ofstream fout;
            fout.open(channelFullPath->cstr());
            if (fout.is_open())
            {
                logD(ffmpeg_module, _func_, "CHANGED");
                fout << allLines;
                fout << "disable_record = \"false\"" << std::endl;
                fout.close();
            }
        }
        else if (!bDisableRecordFound && !set_on)
        {
            std::ofstream fout;
            fout.open(channelFullPath->cstr());
            if (fout.is_open())
            {
                logD(ffmpeg_module, _func_, "!bDisableRecordFound && !set_on");
                fout << allLines;
                fout << "disable_record = \"true\"" << std::endl;
                fout.close();
            }
        }

        StRef<String> const reply_body = st_makeString ("{ \"seq\": \"", seq.c_str(), "\", \"recording\": ", channel_state, " }");
        logD(ffmpeg_module, _func, "reply: ", reply_body);

        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        out << reply_body->cstr();
        out.flush();

        logA(ffmpeg_module, _func_, "mod_nvr_admin 200 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
    }
    else if(segments.size() == 2 &&
            (segments[1].compare("files_existence") == 0))
    {
        HTMLForm form( req );

        NameValueCollection::ConstIterator channel_name_iter = form.find("stream");
        std::string channel_name = (channel_name_iter != form.end()) ? channel_name_iter->second: "";

        StRef<String> st_channel_name = st_makeString(channel_name.c_str());

        self->m_mutex.lock();

        std::map<std::string, WeakRef<FFmpegStream> >::iterator itFFStream = self->m_streams.find(channel_name);

        if (itFFStream == self->m_streams.end())
        {
            self->m_mutex.unlock();

            resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
            std::ostream& out = resp.send();
            out << "404 Channel Not Found (mod_nvr_admin)";
            out.flush();
            logA(ffmpeg_module, _func_, "mod_nvr_admin 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        Ref<ChannelChecker> channelChecker = itFFStream->second.getRefPtr()->GetChannelChecker();
        ChannelChecker::ChannelFileDiskTimes chFileDiskTimes = channelChecker->GetChannelFileDiskTimes ();

        self->m_mutex.unlock();

        StRef<String> const reply_body = channelFilesExistenceToJson (&chFileDiskTimes);
        logD(ffmpeg_module, _func, "reply: ", reply_body);

        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        out << reply_body->cstr();
        out.flush();
    }
    else if(segments.size() == 2 && (segments[1].compare("statistics") == 0))
    {
        StRef<String> reply_body = statisticsToJson(&self->m_statPoints);
        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        out << reply_body->cstr();
        out.flush();
    }
    else if(segments.size() == 2 && (segments[1].compare("disk_info") == 0))
    {
        std::string jsonRespond;

        bool bRes = self->getDiskInfo(jsonRespond);

        if(bRes)
        {
            resp.setStatus(HTTPResponse::HTTP_OK);
            resp.setContentType("text/html");
            std::ostream& out = resp.send();
            out << jsonRespond;
            out.flush();
        }
        else
        {
            logD(ffmpeg_module, _func, "reply: 500 Internal server error");
            resp.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            std::ostream& out = resp.send();
            out << "500 Internal Server Error: fail to get disk info";
            out.flush();
        }
    }
    else if(segments.size() == 2 && (segments[1].compare("source_info") == 0))
    {
        HTMLForm form( req );

        NameValueCollection::ConstIterator channel_name_iter = form.find("stream");
        std::string channel_name = (channel_name_iter != form.end()) ? channel_name_iter->second: "";

        logE_(_func_, "channel_name = ", channel_name.c_str());

        if(!channel_name.size()) // get all sources
        {
            std::string source_info = self->GetAllSourcesInfo();

            logD(ffmpeg_module, _func, "OK");

            resp.setStatus(HTTPResponse::HTTP_OK);
            resp.setContentType("text/html");
            std::ostream& out = resp.send();
            out << source_info;
            out.flush();
        }
        else // get particlular source info
        {
            std::string source_info = self->GetSourceInfo(channel_name);

            if(source_info.size())
            {
                logD(ffmpeg_module, _func, "OK");

                resp.setStatus(HTTPResponse::HTTP_OK);
                resp.setContentType("text/html");
                std::ostream& out = resp.send();
                out << source_info;
                out.flush();
            }
            else
            {
                logE_ (_func, "Bad Request: stream not found");
                resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
                std::ostream& out = resp.send();
                out << "400 Bad Request: no conf_file parameter";
                out.flush();
                logA(ffmpeg_module, _func_, "mod_nvr_admin 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            }
        }
    }
    else if(segments.size() == 2 && (segments[1].compare("alive") == 0))
    {
        HTMLForm form( req );

        NameValueCollection::ConstIterator channel_name_iter = form.find("stream");
        std::string channel_name = (channel_name_iter != form.end()) ? channel_name_iter->second: "";

        logE_(_func_, "channel_name = ", channel_name.c_str());

        self->m_mutex.lock();

        std::map<std::string, WeakRef<FFmpegStream> >::iterator itFFStream = self->m_streams.find(channel_name);

        if (itFFStream == self->m_streams.end())
        {
            self->m_mutex.unlock();

            resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
            std::ostream& out = resp.send();
            out << -1; // channel not found
            out.flush();
            logA(ffmpeg_module, _func_, "mod_nvr_admin 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        bool bIsRestreaming = itFFStream->second.getRefPtr()->IsRestreaming();

        self->m_mutex.unlock();

        logD(ffmpeg_module, _func, "OK");

        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        out << bIsRestreaming? 1: 0;
        out.flush();
    }
    else if(segments.size() == 2 && (segments[1].compare("resolution") == 0))
    {
        HTMLForm form( req );

        NameValueCollection::ConstIterator channel_name_iter = form.find("stream");
        std::string channel_name = (channel_name_iter != form.end()) ? channel_name_iter->second: "";

        logE_(_func_, "channel_name = ", channel_name.c_str());

        self->m_mutex.lock();

        std::map<std::string, WeakRef<FFmpegStream> >::iterator itFFStream = self->m_streams.find(channel_name);

        if (itFFStream == self->m_streams.end())
        {
            self->m_mutex.unlock();

            resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
            std::ostream& out = resp.send();
            out << 0 << " " << 0; // channel not found
            out.flush();
            logA(ffmpeg_module, _func_, "mod_nvr_admin 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        stSourceInfo si = itFFStream->second.getRefPtr()->GetSourceInfo();

        self->m_mutex.unlock();

        logD(ffmpeg_module, _func, "OK");

        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        for(int i = 0; i < si.videoStreams.size(); i++)
        {
            int width = si.videoStreams[i].width;
            int height = si.videoStreams[i].height;
            out << width << " " << height << " ";
        }
        out.flush();
    }
    else if(segments.size() == 2 && (segments[1].compare("remove_video") == 0))
    {
        HTMLForm form( req );

        NameValueCollection::ConstIterator channel_name_iter = form.find("conf_file");
        std::string channel_name = (channel_name_iter != form.end()) ? channel_name_iter->second: "";

        if (channel_name.size() == 0)
        {
            logE_ (_func, "Bad Request: no conf_file parameter");
            resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            std::ostream& out = resp.send();
            out << "400 Bad Request: no conf_file parameter";
            out.flush();
            logA(ffmpeg_module, _func_, "mod_nvr_admin 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }
        StRef<String> st_channel_name = st_makeString(channel_name.c_str());

        Uint64 start_unixtime_sec = 0;
        NameValueCollection::ConstIterator start_time_iter = form.find("start");
        std::string start_time = (start_time_iter != form.end()) ? start_time_iter->second: "";

        if (!strToUint64_safe (start_time.c_str(), &start_unixtime_sec, 10 /* base */))
        {
            logE_ (_func, "Bad \"start\" request parameter value");
            resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            std::ostream& out = resp.send();
            out << "400 Bad \"start\" request parameter value";
            out.flush();
            logA(ffmpeg_module, _func_, "mod_nvr_admin 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        Uint64 end_unixtime_sec = 0;
        NameValueCollection::ConstIterator end_time_iter = form.find("end");
        std::string end_time = (end_time_iter != form.end()) ? end_time_iter->second: "";

        if (!strToUint64_safe (end_time.c_str(), &end_unixtime_sec, 10 /* base */)) {
            logE_ (_func, "Bad \"end\" request parameter value");
            resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            std::ostream& out = resp.send();
            out << "400 Bad \"end\" request parameter value";
            out.flush();
            logA(ffmpeg_module, _func_, "mod_nvr_admin 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        if(start_unixtime_sec >= end_unixtime_sec)
        {
            logE_ (_func, "start_unixtime_sec >= end_unixtime_sec");
            resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            std::ostream& out = resp.send();
            out << "400 start_unixtime_sec >= end_unixtime_sec";
            out.flush();
            logA(ffmpeg_module, _func_, "mod_nvr_admin 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        bool bRes = self->removeVideoFiles(st_channel_name, start_unixtime_sec, end_unixtime_sec);

        if(bRes)
        {
            logD(ffmpeg_module, _func, "reply: OK");
            resp.setStatus(HTTPResponse::HTTP_OK);
            resp.setContentType("text/html");
            std::ostream& out = resp.send();
            out << "OK";
            out.flush();
        }
        else
        {
            logD(ffmpeg_module, _func, "reply: 500 Internal server error");
            resp.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            std::ostream& out = resp.send();
            out << "500 Internal Server Error: fail to remove video files";
            out.flush();
        }

    }
    else if(segments.size() == 2 && (segments[1].compare("reload_recpath") == 0))
    {
        bool bRes = self->m_recpath_config.LoadConfig(self->m_recpath_conf->cstr());

        if(bRes)
        {
            resp.setStatus(HTTPResponse::HTTP_OK);
            resp.setContentType("text/html");
            std::ostream& out = resp.send();
            out << "recpath.conf is reloaded";
            out << self->m_recpath_config.GetConfigJson();
            out.flush();
        }
        else
        {
            logD(ffmpeg_module, _func_, "reply: 500 Internal server error");
            resp.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            std::ostream& out = resp.send();
            out << "500 Internal Server Error: fail to reload recpath.conf";
            out.flush();
        }
    }
    else
    {
        logE_ (_func, "Unknown request: ", req.getURI().c_str());

        logA(ffmpeg_module, _func_, "mod_nvr_admin 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());

        return false;
    }

_return:
    return true;
}

StRef<String>
MomentFFmpegModule::channelExistenceToJson(ChannelChecker::ChannelTimes * const mt_nonnull existence)
{
    std::ostringstream s;
    for(int i = 0; i < existence->size(); i++)
    {
        s << "{\n\"start\":";
        s << (*existence)[i].timeStart;
        s << ",\n\"end\":";
        s << (*existence)[i].timeEnd;
        s << "\n}";
        if(i < existence->size() - 1)
            s << ",\n";
        else
            s << "\n";
    }
    return st_makeString("{\n"
                         "\"timings\" : [\n",
                         s.str().c_str(),
                         "]\n"
                         "}\n");
}


void
MomentFFmpegModule::clearEmptyChannels()
{
    logD(ffmpeg_module, _func);

    m_mutex.lock();

    std::map<std::string, WeakRef<FFmpegStream> >::iterator it1 = m_streams.begin();

    while(it1 != m_streams.end())
    {
        if(!it1->second.getRef() || it1->second.getRef()->IsClosed())
        {
            m_streams.erase(it1++);
        }
        else
            it1++;
    }

    m_mutex.unlock();
}

bool
MomentFFmpegModule::httpRequest (HTTPServerRequest &req, HTTPServerResponse &resp, void * _self)
{
    MomentFFmpegModule * const self = static_cast <MomentFFmpegModule*> (_self);

    logD(ffmpeg_module, _func, req.getURI().c_str());

    URI uri(req.getURI());
    std::vector < std::string > segments;
    uri.getPathSegments(segments);

    if (segments.size() == 2 && segments[1].compare("unixtime") == 0)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);

        StRef<String> const unixtime_str = st_makeString (tv.tv_sec);
        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        out << unixtime_str->cstr();
        out.flush();
        logA(ffmpeg_module, _func_, "mod_nvr 200 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
    }
    else if (segments.size() == 2 && segments[1].compare("channel_state") == 0)
    {
        HTMLForm form( req );

        NameValueCollection::ConstIterator channel_name_iter = form.find("stream");
        std::string channel_name = (channel_name_iter != form.end()) ? channel_name_iter->second: "";

        NameValueCollection::ConstIterator seq_iter = form.find("seq");
        std::string seq = (seq_iter != form.end()) ? seq_iter->second: "";

        bool channel_state = false; // false - isn't writing
        bool channelIsFound = false;

        self->m_mutex.lock();

        std::map<std::string, WeakRef<FFmpegStream> >::iterator it = self->m_streams.find(channel_name);
        if(it != self->m_streams.end() && it->second.getRefPtr())
            channelIsFound = true;

        if (!channelIsFound)
        {
            self->m_mutex.unlock();

            logE_ (_func, "Channel Not Found (mod_nvr): ", channel_name.c_str());
            resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
            std::ostream& out = resp.send();
            out << "404 Channel Not Found (mod_nvr)";
            out.flush();
            logA(ffmpeg_module, _func_, "mod_nvr 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        it->second.getRefPtr()->GetRecordingState(channel_state);

        self->m_mutex.unlock();

        StRef<String> const reply_body = st_makeString ("{ \"seq\": \"", seq.c_str(), "\", \"recording\": ", channel_state, " }");
        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        out << reply_body->cstr();
        out.flush();
        logA(ffmpeg_module, _func_, "mod_nvr 200 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
    }
    else if (segments.size() == 2 && (segments[1].compare("file") == 0 || segments[1].rfind(".mp4") != std::string::npos))
    {	
        HTMLForm form( req );

        NameValueCollection::ConstIterator channel_name_iter = form.find("stream");
        std::string channel_name_str = (channel_name_iter != form.end()) ? channel_name_iter->second: "";

        ConstMemory const channel_name = ConstMemory(channel_name_str.c_str(), channel_name_str.size());

        Uint64 start_unixtime_sec = 0;
        NameValueCollection::ConstIterator start_time_iter = form.find("start");
        std::string start_time = (start_time_iter != form.end()) ? start_time_iter->second: "";

        if (!strToUint64_safe (start_time.c_str(), &start_unixtime_sec, 10 /* base */))
        {
            logE_ (_func, "Bad \"start\" request parameter value");
            resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            std::ostream& out = resp.send();
            out << "Bad \"start\" request parameter value";
            out.flush();
            logA(ffmpeg_module, _func_, "mod_nvr 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        Uint64 end_unixtime_sec = 0;
        NameValueCollection::ConstIterator end_time_iter = form.find("end");
        std::string end_time = (end_time_iter != form.end()) ? end_time_iter->second: "";

        if (!strToUint64_safe (end_time.c_str(), &end_unixtime_sec, 10 /* base */))
        {
            logE_ (_func, "Bad \"end\" request parameter value");
            resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            std::ostream& out = resp.send();
            out << "Bad \"end\" request parameter value";
            out.flush();
            logA(ffmpeg_module, _func_, "mod_nvr 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        if(end_unixtime_sec - start_unixtime_sec >= self->m_nDownloadLimit)
        {
            logE_ (_func, "Requested file length is more than limit in [", self->m_nDownloadLimit, "] seconds");
            resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            std::ostream& out = resp.send();
            out << "Requested file length is more than limit in [";
            out << self->m_nDownloadLimit;
            out << "] seconds";
            out.flush();
            logA(ffmpeg_module, _func_, "mod_nvr 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        StRef<String> const pathToFile = self->doGetFile (channel_name, start_unixtime_sec, end_unixtime_sec);

        if(pathToFile != NULL) // is it correct?
        {
            resp.sendFile(pathToFile->cstr(), "application/octet-stream");
        }
        else
        {
            logE_(_func_, "pathToFile is NULL");
            resp.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            std::ostream& out = resp.send();
            out << "500 Internal Server Error: download file is failed";
            out.flush();
        }
    }
    else if (segments.size() == 2 && segments[1].compare("existence") == 0)
    {	
        HTMLForm form( req );

        NameValueCollection::ConstIterator channel_name_iter = form.find("stream");
        std::string channel_name = (channel_name_iter != form.end()) ? channel_name_iter->second: "";

        logD(ffmpeg_module, _func_, "channel_name: [", channel_name.c_str(), "]");

        self->m_mutex.lock();

        std::map<std::string, WeakRef<FFmpegStream> >::iterator itFFStream = self->m_streams.find(channel_name);

        if (itFFStream == self->m_streams.end())
        {
            self->m_mutex.unlock();
            logE_ (_func, "Channel Not Found (mod_nvr): ", channel_name.c_str());
            resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
            std::ostream& out = resp.send();
            out << "404 Channel Not Found (mod_nvr)";
            out.flush();
            logA(ffmpeg_module, _func_, "mod_nvr 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        Ref<ChannelChecker> channelChecker = itFFStream->second.getRefPtr()->GetChannelChecker();
        ChannelChecker::ChannelTimes channel_existence = channelChecker->GetChannelTimes ();

        self->m_mutex.unlock();

        StRef<String> const reply_body = channelExistenceToJson (&channel_existence);
        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        out << reply_body->cstr();
        out.flush();
        logA(ffmpeg_module, _func_, "mod_nvr 200 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
    }
    else
    {
        logE_ (_func, "Unknown request: ", req.getURI().c_str());

        logA(ffmpeg_module, _func_, "mod_nvr 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());

        return false;
    }

    goto _return;

_bad_request:
    {
        resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
        std::ostream& out = resp.send();
        out << "400 Bad Request (mod_nvr)";
        out.flush();
        logA(ffmpeg_module, _func_, "mod_nvr 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
    }

_return:
    return true;
}

StRef<String>
MomentFFmpegModule::doGetFile (ConstMemory   const channel_name,
                            Time          const start_unixtime_sec,
                            Time          const end_unixtime_sec)
{
    logD(ffmpeg_module, _func, "channel: ", channel_name, ", "
           "start: ", start_unixtime_sec, ", "
           "end: ", end_unixtime_sec);

    std::string filePathRes;
    VideoPartMaker vpm;
    StRef<String> str_ch_name = st_makeString(channel_name);
    std::string ch_name = str_ch_name->cstr();

    m_mutex.lock();

    std::map<std::string, WeakRef<FFmpegStream> >::iterator itFFStream = m_streams.find(ch_name);
    if(itFFStream == m_streams.end())
    {
        m_mutex.unlock();
        logE_(_func_, "there is no ", channel_name, " in m_streams");
        return false;
    }
    Ref<ChannelChecker> channelChecker = itFFStream->second.getRefPtr()->GetChannelChecker();

    m_mutex.unlock();

    ChannelChecker::ChannelFileDiskTimes chlFileDiskTimes = channelChecker->GetChannelFileDiskTimes();
    bool bRes = vpm.Init(&chlFileDiskTimes, ch_name, start_unixtime_sec, end_unixtime_sec, filePathRes);
    if(!bRes)
    {
        logE_(_func_, "fail to create file");
        return NULL;
    }
    vpm.Process();

    logD(ffmpeg_module, _func_, "mp4 is done: [", filePathRes.c_str(), "]");

    return st_makeString(filePathRes.c_str());
}

Ref<MediaSource>
MomentFFmpegModule::createMediaSource (CbDesc<MediaSource::Frontend> const &frontend,
                                    Timers            * const timers,
                                    DeferredProcessor * const deferred_processor,
                                    PagePool          * const page_pool,
                                    VideoStream       * const video_stream,
                                    VideoStream       * const mix_video_stream,
                                    Time                const initial_seek,
                                    ChannelOptions    * const channel_opts,
                                    PlaybackItem      * const playback_item)
{
    logD(ffmpeg_module, _func_, "new MediaSource: ", channel_opts->channel_name);

    Ref<MConfig::Config> const config = this->m_pMoment->getConfig ();


    Ref<FFmpegStream> const ffmpeg_stream = grab (new (std::nothrow) FFmpegStream);
    if(!ffmpeg_stream)
    {
        logE_(_func_, "cannot allocate ffmpeg_stream");
        return NULL;
    }
	
    Ref<ChannelChecker> channel_checker = grab (new (std::nothrow) ChannelChecker);
    channel_checker->init (timers, &m_recpath_config, channel_opts->channel_name);

    ffmpeg_stream->init (frontend,
                      timers,
                      deferred_processor,
                      page_pool,
                      video_stream,
                      mix_video_stream,
                      initial_seek,
                      channel_opts,
                      playback_item,
                      config,
                      &m_recpath_config,
                      channel_checker);

    m_mutex.lock();

    m_streams[channel_opts->channel_name->cstr()] = ffmpeg_stream;

    m_mutex.unlock();

    return ffmpeg_stream;
}

bool
MomentFFmpegModule::CreateStatPoint()
{
    StatMeasure stmRes;
    std::vector<StatMeasure> stmFromStreams;

    m_mutex.lock();

    std::map<std::string, WeakRef<FFmpegStream> >::iterator it = m_streams.begin();
    for(it; it != m_streams.end(); ++it)
    {
        if(it->second.getRefPtr())
            stmFromStreams.push_back(it->second.getRefPtr()->GetStatMeasure());
    }

    m_mutex.unlock();

    // find out most minimum, most maximum, most average, etc
    Time avgsInOut = 0;
    Time avgsInNvr = 0;
    Time packAmInOut = 0;
    Time packAmInNvr = 0;
    size_t ss = stmFromStreams.size();

    for(int i = 0; i < ss; ++i)
    {
        if(stmRes.minInOut > stmFromStreams[i].minInOut)
        {
            stmRes.minInOut = stmFromStreams[i].minInOut;
        }
        if(stmRes.maxInOut < stmFromStreams[i].maxInOut)
        {
            stmRes.maxInOut = stmFromStreams[i].maxInOut;
        }
        if(stmRes.minInNvr > stmFromStreams[i].minInNvr)
        {
            stmRes.minInNvr = stmFromStreams[i].minInNvr;
        }
        if(stmRes.maxInNvr < stmFromStreams[i].maxInNvr)
        {
            stmRes.maxInNvr = stmFromStreams[i].maxInNvr;
        }
        avgsInOut += stmFromStreams[i].avgInOut;
        avgsInNvr += stmFromStreams[i].avgInNvr;

        packAmInOut += stmFromStreams[i].packetAmountInOut;
        packAmInNvr += stmFromStreams[i].packetAmountInNvr;
    }

    stmRes.avgInOut = ss != 0 ? avgsInOut / ss : 0;
    stmRes.avgInNvr = ss != 0 ? avgsInNvr / ss : 0;

    stmRes.packetAmountInOut = ss != 0 ? packAmInOut / ss : 0;
    stmRes.packetAmountInNvr = ss != 0 ? packAmInNvr / ss : 0;

    // CPU RAM HDD
    StatMeasure stmGeneral = m_statMeasurer.GetStatMeasure();

    stmRes.minRAM = stmGeneral.minRAM;
    stmRes.maxRAM = stmGeneral.maxRAM;
    stmRes.avgRAM = stmGeneral.avgRAM;

    stmRes.user_util_min = stmGeneral.user_util_min;
    stmRes.user_util_max = stmGeneral.user_util_max;
    stmRes.user_util_avg = stmGeneral.user_util_avg;

    stmRes.devnames = stmGeneral.devnames;
    stmRes.hdd_utils = stmGeneral.hdd_utils;
    // timestamp

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t timePoint = tv.tv_sec;

    // add to allstat

    m_statPoints[timePoint] = stmRes;

    return true;
}

bool
MomentFFmpegModule::DeleteOldPoints()
{
    time_t diff = OLDPOINTS_LIVE;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t curTime = tv.tv_sec;

    std::map<time_t, StatMeasure>::iterator it = m_statPoints.begin();

    while(it != m_statPoints.end())
    {
        if(curTime - it->first > diff)
        {
            m_statPoints.erase(it++);
        }
        else
        {
            break;
        }
    }

    return true;
}

bool
MomentFFmpegModule::DumpStatInFile()
{
    std::stringstream statToFile;

    std::map<time_t, StatMeasure>::iterator it = m_statPoints.begin();
    for(it; it != m_statPoints.end(); ++it)
    {
        // time
        statToFile << it->first << '|';

        // in-out
        statToFile << it->second.minInOut << '|';
        statToFile << it->second.maxInOut << '|';
        statToFile << it->second.avgInOut << '|';
        statToFile << it->second.packetAmountInOut << '|';

        statToFile << it->second.minInNvr << '|';
        statToFile << it->second.maxInNvr << '|';
        statToFile << it->second.avgInNvr << '|';
        statToFile << it->second.packetAmountInNvr << '|';

        // Memory stat
        statToFile << it->second.minRAM << '|';
        statToFile << it->second.maxRAM << '|';
        statToFile << it->second.avgRAM << '|';

        // CPU stat

        statToFile << it->second.user_util_min << "|";
        statToFile << it->second.user_util_max << "|";
        statToFile << it->second.user_util_avg << "|";

        // HDD stat
        for(int i=0;i<it->second.devnames.size();i++)
        {
            statToFile << it->second.devnames[i] << "|";
            statToFile << it->second.hdd_utils[i];
            if(i+1 != it->second.devnames.size())
            {
                statToFile << "|";
            }
            else
            {
                statToFile << std::endl;
            }
        }
    }

    std::ofstream statFile;
    statFile.open(STATFILE, std::ios::out);
    if (statFile.is_open())
    {
        statFile << statToFile.str();
        statFile.close();
    }
    else
    {
        logD(ffmpeg_module, _func_, "fail to open statFile: ", STATFILE);
        return false;
    }

    return true;
}

bool
MomentFFmpegModule::ReadStatFromFile()
{
    std::ifstream statFile;
    statFile.open(STATFILE, std::ios::in);
    if(statFile.is_open())
    {
        std::string line;
        while ( std::getline (statFile, line) )
        {
            time_t timestamp;
            StatMeasure stm;
            std::vector<std::string> tokens = split(line, '|');

            if(tokens.size() != 15)
            {
                logE_(_func_, "wrong stat file!");
                statFile.close();
                break;
            }

            // time
            {
                std::istringstream iss(tokens[0]);
                iss >> timestamp;
            }
            // in-out
            {
                std::istringstream iss(tokens[1]);
                iss >> stm.minInOut;
            }
            {
                std::istringstream iss(tokens[2]);
                iss >> stm.maxInOut;
            }
            {
                std::istringstream iss(tokens[3]);
                iss >> stm.avgInOut;
            }
            {
                std::istringstream iss(tokens[4]);
                iss >> stm.packetAmountInOut;
            }
            // in-nvr
            {
                std::istringstream iss(tokens[5]);
                iss >> stm.minInNvr;
            }
            {
                std::istringstream iss(tokens[6]);
                iss >> stm.maxInNvr;
            }
            {
                std::istringstream iss(tokens[7]);
                iss >> stm.avgInNvr;
            }
            {
                std::istringstream iss(tokens[8]);
                iss >> stm.packetAmountInNvr;
            }
            // RAM
            {
                std::istringstream iss(tokens[9]);
                iss >> stm.minRAM;
            }
            {
                std::istringstream iss(tokens[10]);
                iss >> stm.maxRAM;
            }
            {
                std::istringstream iss(tokens[11]);
                iss >> stm.avgRAM;
            }
            // CPU
            {
                std::istringstream iss(tokens[12]);
                iss >> stm.user_util_min;
            }
            {
                std::istringstream iss(tokens[13]);
                iss >> stm.user_util_max;
            }
            {
                std::istringstream iss(tokens[14]);
                iss >> stm.user_util_avg;
            }
            // HDD
            {
                int i=15;
                while(i+1 < tokens.size())
                {
                    stm.devnames.push_back(tokens[i]);
                    double util;
                    std::istringstream iss(tokens[i+1]);
                    iss >> util;
                    stm.hdd_utils.push_back(util);
                    i+=2;
                }
            }
            // push to statPoints
            m_statPoints[timestamp] = stm;
        }
        statFile.close();
    }

    return false;
}

void
MomentFFmpegModule::refreshTimerTickStat (void *_self)
{
    MomentFFmpegModule * const self = static_cast <MomentFFmpegModule*> (_self);

    self->RefreshStat();
}

void
MomentFFmpegModule::RefreshStat ()
{
    // refresh maps with streams and channel_checkers
    clearEmptyChannels();

    // add new point to vec
    CreateStatPoint();

    // delete old points
    DeleteOldPoints();

    // dump in file
    DumpStatInFile();
}

void
MomentFFmpegModule::refreshTimerSourceTimes (void *_self)
{
    MomentFFmpegModule * const self = static_cast <MomentFFmpegModule*> (_self);

    self->clearEmptyChannels();

    self->UpdateSourceTimes();
}

bool
MomentFFmpegModule::UpdateSourceTimes()
{
    logD(ffmpeg_module, _func);

    m_mutex.lock();

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t curtime = tv.tv_sec;

    std::map<std::string, WeakRef<FFmpegStream> >::iterator itr = m_streams.begin();
    while(itr != m_streams.end())
    {
        if(m_sourceStateTimes.find(itr->first) == m_sourceStateTimes.end()) // if new source appears
        {
            SourceStateTimes sst;
            m_sourceStateTimes[itr->first] = sst;
        }

        if(!itr->second.getRef()->IsClosed()) // if ffmpegStream is valid
        {
            if(m_sourceStateTimes[itr->first].state.isAlive == false) // if source was dead and become alive, then add new time interval
            {
                TimeInterval stt;
                stt.timeStart = curtime;
                m_sourceStateTimes[itr->first].times.live.push_back(stt);
                m_sourceStateTimes[itr->first].state.isAlive = true;
            }

            m_sourceStateTimes[itr->first].times.live.back().timeEnd = curtime; // update last time

            if(itr->second.getRef()->IsRestreaming())
            {
                if(m_sourceStateTimes[itr->first].state.isRestr == false)
                {
                    TimeInterval stt;
                    stt.timeStart = curtime;
                    m_sourceStateTimes[itr->first].times.restr.push_back(stt);
                    m_sourceStateTimes[itr->first].state.isRestr = true;
                }

                m_sourceStateTimes[itr->first].times.restr.back().timeEnd = curtime;

                if(itr->second.getRef()->IsRecording())
                {
                    if(m_sourceStateTimes[itr->first].state.isWrite == false)
                    {
                        TimeInterval stt;
                        stt.timeStart = curtime;
                        m_sourceStateTimes[itr->first].times.write.push_back(stt);
                        m_sourceStateTimes[itr->first].state.isWrite = true;
                    }

                    m_sourceStateTimes[itr->first].times.write.back().timeEnd = curtime;
                }
                else
                {
                    m_sourceStateTimes[itr->first].state.isWrite = false;
                }
            }
            else
            {
                // if source does not restream, then source does not write
                m_sourceStateTimes[itr->first].state.isRestr = false;
                m_sourceStateTimes[itr->first].state.isWrite = false;
            }
        }
        else
        {
            m_sourceStateTimes[itr->first].state.isAlive = false;
            m_sourceStateTimes[itr->first].state.isRestr = false;
            m_sourceStateTimes[itr->first].state.isWrite = false;
        }

        itr++;
    }

    // check if m_sourceStateTimes contains sources that doesnt exist
    std::map<std::string, SourceStateTimes>::iterator itr1 = m_sourceStateTimes.begin();
    while(itr1 != m_sourceStateTimes.end())
    {
        if(m_streams.find(itr1->first) == m_streams.end())
        {
            m_sourceStateTimes[itr1->first].state.isAlive = false;
            m_sourceStateTimes[itr1->first].state.isRestr = false;
            m_sourceStateTimes[itr1->first].state.isWrite = false;
        }

        itr1++;
    }

    m_mutex.unlock();

    return true;
}

// TODO Always succeeds currently.
Result
MomentFFmpegModule::init (MomentServer * const moment)
{
    this->m_pMoment = moment;
    this->m_pTimers = moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers();
    this->m_pPage_pool = moment->getPagePool();

  // Opening video streams.

    MConfig::Config * const config = moment->getConfig();

    ConstMemory confd_dir_mem;
    // get path for recording
    {
        ConstMemory const opt_name = "moment/confd_dir"; // TODO: change mod_nvr to mod_ffmpeg?
        bool confd_dir_is_set = false;
        confd_dir_mem = config->getString (opt_name, &confd_dir_is_set);
        if (!confd_dir_is_set)
        {
            logE_ (_func, opt_name, " config option is not set, disabling writing");
            // we can not write without output path
            return Result::Failure;
        }
        logD(ffmpeg_module, _func, opt_name, ": [", confd_dir_mem, "]");
    }
	
    m_confd_dir = st_grab (new (std::nothrow) String (confd_dir_mem));

    m_nDownloadLimit = DOWNLOAD_LIMIT;
    {
        ConstMemory const opt_name = "mod_nvr/download_limit";
        MConfig::GetResult const res =
                config->getUint64_default (opt_name, &m_nDownloadLimit, m_nDownloadLimit);
        if (!res)
            logE_ (_func, opt_name, " download_limit option is not set, use default value ", DOWNLOAD_LIMIT);
        else
            logD(ffmpeg_module, _func_, opt_name, ": ", m_nDownloadLimit);
    }

    bool bRes = MemoryDispatcher::Instance().Init();
    if (!bRes)
        logE_ (_func, " fail to init MemoryDispatcher ");
    else
        logD(ffmpeg_module, _func_, "MemoryDispatcher is inited");

    ConstMemory recpath_conf_mem;
    {
        ConstMemory const opt_name = "mod_nvr/recpath_conf";
        bool recpath_conf_is_set = false;
        recpath_conf_mem = config->getString (opt_name, &recpath_conf_is_set);
        if (!recpath_conf_is_set)
            logE_ (_func_, opt_name, " config option is not set, disabling work with archives");
        else
            logD(ffmpeg_module, _func, opt_name, ": ", recpath_conf_mem);
    }
    m_recpath_conf = st_grab (new (std::nothrow) String (recpath_conf_mem));
    this->m_recpath_config.Init(m_recpath_conf->cstr(), &m_streams);

    m_media_viewer = grab (new (std::nothrow) MediaViewer);
    m_media_viewer->init (moment, &m_streams, &m_mutex);

    moment->setMediaSourceProvider (this);

    m_statMeasurer.Init(m_pTimers, TIMER_STATMEASURER);

    this->ReadStatFromFile();

    this->m_timer_keyStat = this->m_pTimers->addTimer (CbDesc<Timers::TimerCallback> (refreshTimerTickStat, this, this),
              TIMER_REFRESH_STAT,
              true /* periodical */,
              true /* auto_delete */);

    this->m_timer_updateTimes = this->m_pTimers->addTimer (CbDesc<Timers::TimerCallback> (refreshTimerSourceTimes, this, this),
              TIMER_UPDATE_SOURCES_TIMES,
              true /* periodical */,
              true /* auto_delete */);

    HttpReqHandler::addHandler(std::string("mod_nvr"), httpRequest, this);
    HttpReqHandler::addHandler(std::string("mod_nvr_admin"), adminHttpRequest, this);

    return Result::Success;
}

MomentFFmpegModule::MomentFFmpegModule()
    : m_pMoment (NULL),
      m_pTimers (NULL),
      m_timer_keyStat (NULL),
      m_timer_updateTimes (NULL),
      m_pPage_pool (NULL),
      m_bServe_playlist_json (true)
{
    m_default_channel_opts = grab (new (std::nothrow) ChannelOptions);
    if(!m_default_channel_opts){logE_(_func_, "cannot allocate m_default_channel_opts");}
    m_default_channel_opts->default_item = grab (new (std::nothrow) PlaybackItem);
    if(!m_default_channel_opts->default_item){logE_(_func_, "cannot allocate m_default_channel_opts->default_item");}
}

MomentFFmpegModule::~MomentFFmpegModule ()
{
    if (this->m_timer_keyStat) {
        this->m_pTimers->deleteTimer (this->m_timer_keyStat);
        this->m_timer_keyStat = NULL;
    }

    if (this->m_timer_updateTimes) {
        this->m_pTimers->deleteTimer (this->m_timer_updateTimes);
        this->m_timer_updateTimes = NULL;
    }

  StateMutexLock l (&m_mutex);

    {
    ChannelEntryHash::iter iter (m_channel_entry_hash);
    while (!m_channel_entry_hash.iter_done (iter)) {
        ChannelEntry * const channel_entry = m_channel_entry_hash.iter_next (iter);
	    delete channel_entry;
	}
    }

    {
    RecorderEntryHash::iter iter (m_recorder_entry_hash);
    while (!m_recorder_entry_hash.iter_done (iter)) {
        RecorderEntry * const recorder_entry = m_recorder_entry_hash.iter_next (iter);
	    delete recorder_entry;
	}
    }
}

} // namespace Moment

