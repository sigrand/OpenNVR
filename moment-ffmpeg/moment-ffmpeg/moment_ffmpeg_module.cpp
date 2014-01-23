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


#include <libmary/types.h>
#include <cctype>
#include <mntent.h>
#include <sys/statvfs.h>
#include <json/json.h>

#include <moment/libmoment.h>

#include <moment-ffmpeg/moment_ffmpeg_module.h>
#include <moment-ffmpeg/video_part_maker.h>


// TODO These header macros are the same as in rtmpt_server.cpp
#define MOMENT_FFMPEG__HEADERS_DATE \
	Byte date_buf [unixtimeToString_BufSize]; \
	Size const date_len = unixtimeToString (Memory::forObject (date_buf), getUnixtime());

#define MOMENT_FFMPEG__COMMON_HEADERS \
	"Server: Moment/1.0\r\n" \
	"Date: ", ConstMemory (date_buf, date_len), "\r\n" \
	"Connection: Keep-Alive\r\n" \
	"Cache-Control: no-cache\r\n"

#define MOMENT_FFMPEG__OK_HEADERS(mime_type, content_length) \
	"HTTP/1.1 200 OK\r\n" \
	MOMENT_FFMPEG__COMMON_HEADERS \
	"Content-Type: ", (mime_type), "\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_FFMPEG__404_HEADERS(content_length) \
	"HTTP/1.1 404 Not Found\r\n" \
	MOMENT_FFMPEG__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_FFMPEG__400_HEADERS(content_length) \
	"HTTP/1.1 400 Bad Request\r\n" \
	MOMENT_FFMPEG__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"

#define MOMENT_FFMPEG__500_HEADERS(content_length) \
	"HTTP/1.1 500 Internal Server Error\r\n" \
	MOMENT_FFMPEG__COMMON_HEADERS \
	"Content-Type: text/plain\r\n" \
	"Content-Length: ", (content_length), "\r\n"


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

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
    logD_ (_func, "channel_name: ", channel_name);

    mutex.lock ();

    ChannelEntry * const channel_entry = channel_entry_hash.lookup (channel_name);
    if (!channel_entry) {
	mutex.unlock ();
	Ref<String> const err_msg = makeString ("Channel not found: ", channel_name);
	logE_ (_func, err_msg);
	*ret_err_msg = err_msg;
	return Result::Failure;
    }

    if (!channel_entry->playlist_filename) {
	mutex.unlock ();
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
	mutex.unlock ();
	logE_ (_func, "channel->loadPlaylistFile() failed: ", err_msg);
	*ret_err_msg = makeString ("Playlist parsing error: ", err_msg->mem());
	return Result::Failure;
    }

    mutex.unlock ();

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

    mutex.lock ();

    ChannelEntry * const channel_entry = channel_entry_hash.lookup (channel_name);
    if (!channel_entry) {
	mutex.unlock ();
	logE_ (_func, "Channel not found: ", channel_name);
	return Result::Failure;
    }

    Result res;
    if (item_name_is_id) {
	res = channel_entry->channel->getPlayback()->setPosition_Id (item_name, seek);
    } else {
	Uint32 item_idx;
	if (!strToUint32_safe (item_name, &item_idx)) {
	    mutex.unlock ();
	    logE_ (_func, "Failed to parse item index");
	    return Result::Failure;
	}

	res = channel_entry->channel->getPlayback()->setPosition_Index (item_idx, seek);
    }

    if (!res) {
	mutex.unlock ();
	logE_ (_func, "Item not found: ", item_name, item_name_is_id ? " (id)" : " (idx)", ", channel: ", channel_name);
	return Result::Failure;
    }

    mutex.unlock ();

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
    ChannelEntry * const channel_entry = new (std::nothrow) ChannelEntry;
    assert (channel_entry);
    channel_entry->channel_opts = channel_opts;

    channel_entry->channel_name  = grab (new (std::nothrow) String (channel_opts->channel_name->mem()));
    channel_entry->channel_title = grab (new (std::nothrow) String (channel_opts->channel_title->mem()));
    channel_entry->channel_desc  = grab (new (std::nothrow) String (channel_opts->channel_desc->mem()));
    channel_entry->playlist_filename = grab (new (std::nothrow) String (playlist_filename));

    channel_entry->push_agent  = push_agent;
    channel_entry->fetch_agent = fetch_agent;

    Ref<Channel> const channel = grab (new (std::nothrow) Channel);
    channel_entry->channel = channel;

    channel->init (moment, channel_opts);

    mutex.lock ();
    channel_entry_hash.add (channel_entry);
    mutex.unlock ();
    channel_set.addChannel (channel, channel_opts->channel_name->mem());

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
    ChannelEntry * const channel_entry = new (std::nothrow) ChannelEntry;
    assert (channel_entry);
    channel_entry->channel_opts = channel_opts;

    channel_entry->channel_name  = grab (new (std::nothrow) String (channel_opts->channel_name->mem()));
    channel_entry->channel_title = grab (new (std::nothrow) String (channel_opts->channel_title->mem()));
    channel_entry->channel_desc  = grab (new (std::nothrow) String (channel_opts->channel_desc->mem()));
    channel_entry->playlist_filename = NULL;

    channel_entry->push_agent  = push_agent;
    channel_entry->fetch_agent = fetch_agent;

    Ref<Channel> const channel = grab (new (std::nothrow) Channel);
    channel_entry->channel = channel;

    channel->init (moment, channel_opts);

    mutex.lock ();
    channel_entry_hash.add (channel_entry);
    mutex.unlock ();
    channel_set.addChannel (channel, channel_opts->channel_name->mem());

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
    ChannelEntry * const channel_entry = new (std::nothrow) ChannelEntry;
    assert (channel_entry);

    channel_entry->channel_name  = grab (new (std::nothrow) String (channel_name));
    channel_entry->channel_title = grab (new (std::nothrow) String (channel_title));
    channel_entry->channel_desc  = grab (new (std::nothrow) String (channel_desc));
    channel_entry->playlist_filename = NULL;

    channel_entry->push_agent  = push_agent;
    channel_entry->fetch_agent = fetch_agent;

    Ref<Channel> const channel = grab (new (std::nothrow) Channel);
    channel_entry->channel = channel;

    Ref<ChannelOptions> const opts = grab (new (std::nothrow) ChannelOptions);
    channel_entry->channel_opts = opts;
    {
        *opts = *default_channel_opts;
        opts->channel_name  = st_grab (new (std::nothrow) String (channel_name));
        opts->channel_title = st_grab (new (std::nothrow) String (channel_title));
        opts->channel_desc  = st_grab (new (std::nothrow) String (channel_desc));
    }

    Ref<PlaybackItem> const item = grab (new (std::nothrow) PlaybackItem);
    opts->default_item = item;
    {
        *item = *default_channel_opts->default_item;

        item->stream_spec = st_grab (new (std::nothrow) String);
        item->spec_kind = PlaybackItem::SpecKind::None;

        item->force_transcode = false;
        item->force_transcode_video = false;
        item->force_transcode_audio = false;
    }

    channel->init (moment, opts);

    mutex.lock ();
    channel_entry_hash.add (channel_entry);
    mutex.unlock ();
    channel_set.addChannel (channel, channel_name);

    if (!fetch_agent)
        channel->getPlayback()->setSingleItem (item);
}

void
MomentFFmpegModule::createPlaylistRecorder (ConstMemory const recorder_name,
					 ConstMemory const playlist_filename,
					 ConstMemory const filename_prefix)
{
    RecorderEntry * const recorder_entry = new RecorderEntry;

    recorder_entry->recorder_name = grab (new String (recorder_name));
    recorder_entry->playlist_filename  = grab (new String (playlist_filename));

    Ref<Recorder> const recorder = grab (new Recorder);
    recorder_entry->recorder = recorder;

    recorder->init (moment,
		    page_pool,
		    &channel_set,
		    filename_prefix,
                    default_channel_opts->min_playlist_duration_sec);

    mutex.lock ();
    recorder_entry_hash.add (recorder_entry);
    mutex.unlock ();

    {
	Ref<String> err_msg;
	if (!recorder->loadPlaylistFile (playlist_filename,
                                         false /* keep_cur_item */,
                                         default_channel_opts->default_item,
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

    recorder->init (moment,
		    page_pool,
		    &channel_set,
		    filename_prefix,
                    default_channel_opts->min_playlist_duration_sec);

    mutex.lock ();
    recorder_entry_hash.add (recorder_entry);
    mutex.unlock ();

    recorder->setSingleChannel (channel_name);
}

void
MomentFFmpegModule::printChannelInfoJson (PagePool::PageListHead * const page_list,
				       ChannelEntry           * const channel_entry)
{
    static char const open_str []  = "[ \"";
    page_pool->getFillPages (page_list, open_str);

    page_pool->getFillPages (page_list, channel_entry->channel_name->mem());

    static char const sep_a [] = "\", \"";
    page_pool->getFillPages (page_list, sep_a);

    page_pool->getFillPages (page_list, channel_entry->channel_desc->mem());

    page_pool->getFillPages (page_list, sep_a);

    {
	if (channel_entry->channel
	    && channel_entry->channel->isSourceOnline())
	{
	    page_pool->getFillPages (page_list, "ONLINE");
	} else {
	    page_pool->getFillPages (page_list, "OFFLINE");
	}
    }

    static char const close_str [] = "\" ]";
    page_pool->getFillPages (page_list, close_str);
}

StRef<String>
MomentFFmpegModule::channelFilesExistenceToJson (
        ChannelChecker::ChannelFileSummary * const mt_nonnull files_existence)
{
     std::ostringstream s;
     ChannelChecker::ChannelFileSummary::iterator it = files_existence->begin();
     for(it; it != files_existence->end(); ++it)
     {
         s << "{\n\"path\":\"";
         s << (*it).first;
         s <<"\",\n\"start\":";
         s << (*it).second.first;
         s << ",\n\"end\":";
         s << (*it).second.second;
         s << "\n}";
        ChannelChecker::ChannelFileSummary::iterator it1 = it;
        it1++;
         if(it1 != files_existence->end())
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
     std::ostringstream s;
     std::map<time_t, StatMeasure>::iterator it = statPoints->begin();

     for(it; it != statPoints->end(); ++it)
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
         s << "\"avg\":\"" << (*it).second.user_util_avg << "\"}\n";

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

#ifdef __linux__
int
MomentFFmpegModule::getDiskInfoLinux(std::map<std::string, std::vector<int> > & diskInfo)
{
    struct mntent *mnt;
    char const *table = "/etc/mtab";
    FILE *fp;

    fp = setmntent (table, "r");
    if (fp == NULL)
    {
        logE_(_func_, "fail to get disk info from ", table);
        return 1;
    }

    while ((mnt = getmntent (fp)))
    {
        struct statvfs info;
        if(statvfs (mnt->mnt_dir, &info))
        {
            logE_(_func_, "fail statvfs for ", mnt->mnt_dir);
            return 2;
        }

        if(info.f_frsize > 0 && info.f_blocks > 0)
        {
            unsigned long allSize = (info.f_frsize * info.f_blocks) / 1024; // in KBytes
            unsigned long freeSize = (info.f_frsize * info.f_bavail) / 1024; // in KBytes
            unsigned long usedSize = allSize - (info.f_frsize * info.f_bfree) / 1024; // in KBytes

            std::vector<int> sizeInfo;
            sizeInfo.push_back(allSize);
            sizeInfo.push_back(freeSize);
            sizeInfo.push_back(usedSize);
            diskInfo[std::string(mnt->mnt_dir)] = sizeInfo;
        }
    }

    if (endmntent (fp) == 0)
    {
        logE_(_func_, "fail to close ", table);
        return 3;
    }

    return 0;
}
#endif

bool
MomentFFmpegModule::getDiskInfo (std::string & json_respond)
{
    int nRes = -1;
    std::map<std::string, std::vector<int> > diskInfo; // [diskName, [allSize, freeSize, usedSize]]

#ifdef __linux__
    nRes = getDiskInfoLinux(diskInfo);
#endif

    if(nRes < 0)
    {
        logD_(_func_, "getDiskInfo isn't supported for current platform");
    }
    else if(nRes > 0)
    {
        return false;
    }

    // writing to json

    Json::Value json_value_disks;

    Json::FastWriter json_writer_fast;
    Json::StyledWriter json_writer_styled;

    std::map<std::string, std::vector<int> >::iterator it = diskInfo.begin();

    for(it; it != diskInfo.end(); ++it)
    {
        Json::Value json_value_disk;
        Json::Value json_value_diskInfo;

        json_value_disk["disk name"] = it->first;

        json_value_diskInfo["all size"] = it->second[0];
        json_value_diskInfo["free size"] = it->second[1];
        json_value_diskInfo["used size"] = it->second[2];

        json_value_disk["disk info"] = json_value_diskInfo;

        logD_ (_func_, "playlist.json line: ", json_writer_fast.write(json_value_disk).c_str());

        json_value_disks.append(json_value_disk);
    }

    json_respond = json_writer_styled.write(json_value_disks);

    return true;
}

bool
MomentFFmpegModule::removeVideoFiles(StRef<String> const dir_name, StRef<String> const channel_name,
                                 Time const startTime, Time const endTime)
{
    Ref<Vfs> vfs = Vfs::createDefaultLocalVfs (dir_name->mem());
    ChannelChecker::ChannelFileSummary file_existence;

    ChannelChecker().readIdx(file_existence, dir_name, channel_name);

    NvrFileIterator file_iter;
    file_iter.init (vfs, channel_name->mem(), startTime);

    bool bRes = false;
    while (true)
    {
        StRef<String> const filename = file_iter.getNext ();
        logD_(_func_, "========= next file: [", filename, "]");
        if (!filename)
            break;

        Time file_unixtime_sec;
        std::string stdStr(filename->cstr());
        std::map<std::string, std::pair<int,int> >::iterator it = file_existence.find(stdStr);
        if(it != file_existence.end())
        {
            if(it->second.second > startTime && it->second.first < endTime)
            {
                StRef<String> const filenameFull = st_makeString(filename, ".flv");
                logD_(_func_, "remove by request: [", filenameFull, "]");
                vfs->removeFile (filenameFull->mem());
                vfs->removeSubdirsForFilename (filenameFull->mem());
                bRes = true;
            }
        }
    }
    logD_(_func_, "========== end of story");

    ChannelChecker().writeIdx(file_existence, dir_name, channel_name);

    return bRes;
}


bool
MomentFFmpegModule::adminHttpRequest (HTTPServerRequest &req, HTTPServerResponse &resp, void * _self)
{
    MomentFFmpegModule * const self = static_cast <MomentFFmpegModule*> (_self);

    logD_ (_func_);

    URI uri(req.getURI());
    std::vector < std::string > segments;
    uri.getPathSegments(segments);

    if (segments.size() >= 2 &&
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

        self->mutex.lock();

        std::map<std::string, WeakRef<FFmpegStream> >::iterator it = self->m_streams.find(st_channel_name->cstr());
        if(it != self->m_streams.end() && it->second.getRefPtr())
            channelIsFound = true;

        if (!channelIsFound)
        {
            self->mutex.unlock();

            resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
            std::ostream& out = resp.send();
            out << "404 Channel Not Found (mod_nvr_admin)";
            out.flush();

            logA_ ("mod_nvr_admin 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        it->second.getRefPtr()->SetChannelState(set_on);
        it->second.getRefPtr()->GetChannelState(channel_state);

        self->mutex.unlock();

        bool bDisableRecordFound = false;
        StRef<String> channelFullPath = st_makeString(self->confd_dir, "/", st_channel_name);
        std::string line;
        std::string allLines = "";
        std::ifstream channelPath (channelFullPath->cstr());
        logD_(_func_, "channelFullPath = ", channelFullPath);
        if (channelPath.is_open())
        {
            logD_(_func_, "opened");
            while ( std::getline (channelPath, line) )
            {
                logD_(_func_, "got line: [", line.c_str(), "]");
                if(line.compare("disable_record") == 0)
                    bDisableRecordFound = true;
                else
                    allLines += line + "\n";
            }
            channelPath.close();
        }

        if(bDisableRecordFound && set_on)
        {
            logD_(_func_, "bDisableRecordFound && set_on");
            std::ofstream fout;
            fout.open(channelFullPath->cstr());
            if (fout.is_open())
            {
                logD_(_func_, "CHANGED");
                fout << allLines;
                fout.close();
            }
        }
        else if (!bDisableRecordFound && !set_on)
        {
            std::ofstream fout;
            fout.open(channelFullPath->cstr(),std::ios::app);
            if (fout.is_open())
            {
                logD_(_func_, "!bDisableRecordFound && !set_on");
                fout << "disable_record" << std::endl;
                fout.close();
            }
        }

        StRef<String> const reply_body = st_makeString ("{ \"seq\": \"", seq.c_str(), "\", \"recording\": ", channel_state, " }");
        logD_ (_func, "reply: ", reply_body);

        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        out << reply_body->cstr();
        out.flush();

        logA_ ("mod_nvr_admin 200 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
    }
    else if(segments.size() >= 2 &&
            (segments[1].compare("files_existence") == 0))
    {
        HTMLForm form( req );

        NameValueCollection::ConstIterator channel_name_iter = form.find("stream");
        std::string channel_name = (channel_name_iter != form.end()) ? channel_name_iter->second: "";

        ChannelChecker::ChannelFileSummary * files_existence;
        StRef<String> st_channel_name = st_makeString(channel_name.c_str());

        self->mutex.lock();

        std::map<std::string, Ref<ChannelChecker> >::iterator it = self->m_channel_checkers.find(st_channel_name->cstr());
        if(it == self->m_channel_checkers.end() || it->second.isNull())
            files_existence = NULL;
        else
            files_existence = it->second->getChannelFilesExistence ();

        if (files_existence == NULL)
        {
            self->mutex.unlock();

            resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
            std::ostream& out = resp.send();
            out << "404 Channel Not Found (mod_nvr_admin)";
            out.flush();
            logA_ ("mod_nvr_admin 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        self->mutex.unlock();

        StRef<String> const reply_body = channelFilesExistenceToJson (files_existence);
        logD_ (_func, "reply: ", reply_body);

        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        out << reply_body->cstr();
        out.flush();
    }
    else if(segments.size() >= 2 && (segments[1].compare("statistics") == 0))
    {
        StRef<String> reply_body = statisticsToJson(&self->statPoints);
        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        out << reply_body->cstr();
        out.flush();
    }
    else if(segments.size() >= 2 && (segments[1].compare("disk_info") == 0))
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
            logD_ (_func, "reply: 500 Internal server error");
            resp.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            std::ostream& out = resp.send();
            out << "500 Internal Server Error: fail to get disk info";
            out.flush();
        }

    }
    else if(segments.size() >= 2 && (segments[1].compare("remove_video") == 0))
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
            logA_ ("mod_nvr_admin 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
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
            logA_ ("mod_nvr_admin 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
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
            logA_ ("mod_nvr_admin 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        if(start_unixtime_sec >= end_unixtime_sec)
        {
            logE_ (_func, "start_unixtime_sec >= end_unixtime_sec");
            resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            std::ostream& out = resp.send();
            out << "400 start_unixtime_sec >= end_unixtime_sec";
            out.flush();
            logA_ ("mod_nvr_admin 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        bool bRes = self->removeVideoFiles(self->record_dir, st_channel_name, start_unixtime_sec, end_unixtime_sec);

        if(bRes)
        {
            logD_ (_func, "reply: OK");
            resp.setStatus(HTTPResponse::HTTP_OK);
            resp.setContentType("text/html");
            std::ostream& out = resp.send();
            out << "OK";
            out.flush();
        }
        else
        {
            logD_ (_func, "reply: 500 Internal server error");
            resp.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            std::ostream& out = resp.send();
            out << "500 Internal Server Error: fail to remove video files";
            out.flush();
        }

    }
    else
    {
        logE_ (_func, "Unknown request: ", req.getURI().c_str());

        logA_ ("mod_nvr_admin 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());

        return false;
    }

_return:
    return true;
}

StRef<String>
MomentFFmpegModule::channelExistenceToJson(std::vector<std::pair<int,int>> * const mt_nonnull existence)
{
    std::ostringstream s;
    for(int i = 0; i < existence->size(); i++)
    {
        s << "{\n\"start\":";
        s << (*existence)[i].first;
        s << ",\n\"end\":";
        s << (*existence)[i].second;
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
    //logD_(_func_, "OOOOO m_streams BEFORE CLEANING");

    mutex.lock();

    std::map<std::string, WeakRef<FFmpegStream> >::iterator it1 = m_streams.begin();
//    while(it1 != m_streams.end())
//    {
//        logD_(_func_, "OOOOO m_streams stream_name = ", it1->first.c_str());
//        it1++;
//    }

    std::vector<std::string> strNameToDelete;
    it1 = m_streams.begin();
    while(it1 != m_streams.end())
    {
        if(!it1->second.getRef())
        {
//            logD_(_func_, "OOOOO delete ", it1->first.c_str());
            strNameToDelete.push_back(std::string(it1->first.c_str()));
            m_streams.erase(it1++);
        }
        else
            it1++;
    }

//    logD_(_func_, "OOOOO m_streams AFTER CLEANING");

//    it1 = m_streams.begin();
//    while(it1 != m_streams.end())
//    {
//        logD_(_func_, "OOOOO m_streams stream_name = ", it1->first.c_str());
//        it1++;
//    }

//////////////////////////////////////////////////////////////////////////////////

//    logD_(_func_, "OOOOO m_channel_checkers BEFORE CLEANING");

    std::map<std::string, Ref<ChannelChecker> >::iterator it2 = m_channel_checkers.begin();
//    while(it2 != m_channel_checkers.end())
//    {
//        logD_(_func_, "OOOOO m_channel_checkers stream_name = ", it2->first.c_str());
//        it2++;
//    }

    it2 = m_channel_checkers.begin();
    for(int i=0;i<strNameToDelete.size();++i)
    {
        m_channel_checkers.erase(strNameToDelete[i]);
    }

//    logD_(_func_, "OOOOO m_channel_checkers AFTER CLEANING");

//    it2 = m_channel_checkers.begin();
//    while(it2 != m_channel_checkers.end())
//    {
//        logD_(_func_, "OOOOO m_channel_checkers stream_name = ", it2->first.c_str());
//        it2++;
//    }

    mutex.unlock();
}

bool
MomentFFmpegModule::httpRequest (HTTPServerRequest &req, HTTPServerResponse &resp, void * _self)
{
    MomentFFmpegModule * const self = static_cast <MomentFFmpegModule*> (_self);

    logD_ (_func, req.getURI().c_str());

    URI uri(req.getURI());
    std::vector < std::string > segments;
    uri.getPathSegments(segments);

    if (segments.size() >= 2 && segments[1].compare("unixtime") == 0)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);

        StRef<String> const unixtime_str = st_makeString (tv.tv_sec);
        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        out << unixtime_str->cstr();
        out.flush();
        logA_ ("mod_nvr 200 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
    }
    else if (segments.size() >= 2 && segments[1].compare("channel_state") == 0)
    {
        HTMLForm form( req );

        NameValueCollection::ConstIterator channel_name_iter = form.find("stream");
        std::string channel_name = (channel_name_iter != form.end()) ? channel_name_iter->second: "";

        NameValueCollection::ConstIterator seq_iter = form.find("seq");
        std::string seq = (seq_iter != form.end()) ? seq_iter->second: "";

        bool channel_state = false; // false - isn't writing
        bool channelIsFound = false;

        self->mutex.lock();

        std::map<std::string, WeakRef<FFmpegStream> >::iterator it = self->m_streams.find(channel_name);
        if(it != self->m_streams.end() && it->second.getRefPtr())
            channelIsFound = true;

        if (!channelIsFound)
        {
            self->mutex.unlock();

            logE_ (_func, "Channel Not Found (mod_nvr): ", channel_name.c_str());
            resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
            std::ostream& out = resp.send();
            out << "404 Channel Not Found (mod_nvr)";
            out.flush();
            logA_ ("mod_nvr 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        it->second.getRefPtr()->GetChannelState(channel_state);

        self->mutex.unlock();

        StRef<String> const reply_body = st_makeString ("{ \"seq\": \"", seq.c_str(), "\", \"recording\": ", channel_state, " }");
        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        out << reply_body->cstr();
        out.flush();
        logA_ ("mod_nvr 200 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
    }
    else if (segments.size() >= 2 && (segments[1].compare("file") == 0 || segments[1].rfind(".mp4") != std::string::npos))
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
            goto _bad_request;
        }

        Uint64 end_unixtime_sec = 0;
        NameValueCollection::ConstIterator end_time_iter = form.find("end");
        std::string end_time = (end_time_iter != form.end()) ? end_time_iter->second: "";

        if (!strToUint64_safe (end_time.c_str(), &end_unixtime_sec, 10 /* base */))
        {
            logE_ (_func, "Bad \"end\" request parameter value");
            goto _bad_request;
        }

        StRef<String> const pathToFile = self->_doGetFile (channel_name, start_unixtime_sec, end_unixtime_sec);

        if(pathToFile != NULL) // is it correct?
        {
            std::ifstream myfile (pathToFile->cstr());
            std::string line;
            if (myfile.is_open())
            {
                resp.setStatus(HTTPResponse::HTTP_OK);
                resp.setContentType("application/octet-stream");
                std::ostream& out = resp.send();

                while ( std::getline (myfile,line) )
                {
                    out << line << "\n";
                }
                myfile.close();
                out.flush();
            }
            else
            {
                logE_(_func_, "cannot open file = ", pathToFile);
                resp.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                std::ostream& out = resp.send();
                out << "500 Internal Server Error: download file is failed";
                out.flush();
            }
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
    else if (segments.size() >= 2 && segments[1].compare("existence") == 0)
    {
        HTMLForm form( req );

        NameValueCollection::ConstIterator channel_name_iter = form.find("stream");
        std::string channel_name = (channel_name_iter != form.end()) ? channel_name_iter->second: "";

        logD_(_func_, "channel_name: [", channel_name.c_str(), "]");

        std::vector<std::pair<int,int>> * channel_existence;

        self->mutex.lock();

        std::map<std::string, Ref<ChannelChecker> >::iterator it = self->m_channel_checkers.find(channel_name);
        if(it == self->m_channel_checkers.end() || it->second.isNull())
            channel_existence = NULL;
        else
            channel_existence = it->second->getChannelExistence ();

        self->mutex.unlock();

        if (channel_existence == NULL)
        {
            logE_ (_func, "Channel Not Found (mod_nvr): ", channel_name.c_str());
            resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
            std::ostream& out = resp.send();
            out << "404 Channel Not Found (mod_nvr)";
            out.flush();
            logA_ ("mod_nvr 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }
        StRef<String> const reply_body = channelExistenceToJson (channel_existence);
        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/html");
        std::ostream& out = resp.send();
        out << reply_body->cstr();
        out.flush();
        logA_ ("mod_nvr 200 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
    }
    else
    {
        logE_ (_func, "Unknown request: ", req.getURI().c_str());

        logA_ ("mod_nvr 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());

        return false;
    }

    goto _return;

_bad_request:
    {
        resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
        std::ostream& out = resp.send();
        out << "400 Bad Request (mod_nvr)";
        out.flush();
        logA_ ("mod_nvr 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
    }

_return:
    return true;
}

GetFileSession::Frontend const MomentFFmpegModule::get_file_session_frontend = {
    getFileSession_done
};

void
MomentFFmpegModule::getFileSession_done (Result   const res,
                                      void   * const _self)
{
    MomentFFmpegModule * const self = static_cast <MomentFFmpegModule*> (_self);

    logD_ (_func_);

    // TODO Destroy GetFileSession.
}

StRef<String>
MomentFFmpegModule::doGetFile (HttpRequest * const mt_nonnull req,
                            Sender      * const mt_nonnull sender,
                            ConstMemory   const channel_name,
                            Time          const start_unixtime_sec,
                            Time          const end_unixtime_sec)
{
    logD_ (_func, "channel: ", channel_name, ", "
           "start: ", start_unixtime_sec, ", "
           "end: ", end_unixtime_sec);

    VideoPartMaker vpm;
    StRef<String> filePathRes = st_makeString(record_dir, "/", channel_name, "/", channel_name, ".mp4");

    bool bRes = vpm.Init(vfs, channel_name, record_dir->mem(), filePathRes->mem(), start_unixtime_sec, end_unixtime_sec);
    if(!bRes)
        return st_makeString("fail to create file");
    vpm.Process();

    logD_(_func_, "mp4 is done: [", filePathRes, "]");

    Ref<GetFileSession> const get_file_session = grab (new (std::nothrow) GetFileSession);
    {
        get_file_session->init (moment,
                                req,
                                sender,
                                page_pool,
                                vfs,
                                channel_name,
                                start_unixtime_sec,
                                end_unixtime_sec,
                                true,
                                CbDesc<GetFileSession::Frontend> (&get_file_session_frontend, this, this),
                                filePathRes/*record_dir*/);
    }

    mutex.lock ();
#warning TODO GetFileSession lifetime
    get_file_sessions.append (get_file_session);
    mutex.unlock ();

    get_file_session->start ();

    return st_makeString(filePathRes);
}

StRef<String>
MomentFFmpegModule::_doGetFile (ConstMemory   const channel_name,
                            Time          const start_unixtime_sec,
                            Time          const end_unixtime_sec)
{
    logD_ (_func, "channel: ", channel_name, ", "
           "start: ", start_unixtime_sec, ", "
           "end: ", end_unixtime_sec);

    static int iii = 0;

    VideoPartMaker vpm;
    StRef<String> filePathRes = st_makeString(record_dir, "/", channel_name, "/", channel_name, "_", iii++, ".mp4");

    bool bRes = vpm.Init(vfs, channel_name, record_dir->mem(), filePathRes->mem(), start_unixtime_sec, end_unixtime_sec);
    if(!bRes)
    {
        logE_(_func_, "fail to create file");
        return NULL;
    }
    vpm.Process();

    logD_(_func_, "mp4 is done: [", filePathRes, "]");

    return st_makeString(filePathRes);
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
    logD_(_func_, "new MediaSource: ", channel_opts->channel_name);

    Ref<MConfig::Config> const config = this->moment->getConfig ();


    Ref<FFmpegStream> const ffmpeg_stream = grab (new (std::nothrow) FFmpegStream);
    ffmpeg_stream->init (frontend,
                      timers,
                      deferred_processor,
                      page_pool,
                      video_stream,
                      mix_video_stream,
                      initial_seek,
                      channel_opts,
                      playback_item,
                      config);

    m_streams[channel_opts->channel_name->cstr()] = ffmpeg_stream;

    Ref<ChannelChecker> channel_checker = grab (new (std::nothrow) ChannelChecker);
    channel_checker->init (timers, vfs, record_dir, channel_opts->channel_name);

    m_channel_checkers[channel_opts->channel_name->cstr()] = channel_checker;

    return ffmpeg_stream;
}

bool
MomentFFmpegModule::CreateStatPoint()
{
    StatMeasure stmRes;
    std::vector<StatMeasure> stmFromStreams;

    mutex.lock();

    std::map<std::string, WeakRef<FFmpegStream> >::iterator it = m_streams.begin();
    for(it; it != m_streams.end(); ++it)
    {
        if(it->second.getRefPtr())
            stmFromStreams.push_back(it->second.getRefPtr()->GetStatMeasure());
    }

    mutex.unlock();

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

    // CPU RAM
    StatMeasure stmGeneral = m_statMeasurer.GetStatMeasure();

    stmRes.minRAM = stmGeneral.minRAM;
    stmRes.maxRAM = stmGeneral.maxRAM;
    stmRes.avgRAM = stmGeneral.avgRAM;

    stmRes.user_util_min = stmGeneral.user_util_min;
    stmRes.user_util_max = stmGeneral.user_util_max;
    stmRes.user_util_avg = stmGeneral.user_util_avg;

    // timestamp

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t timePoint = tv.tv_sec;

    // add to allstat

    statPoints[timePoint] = stmRes;

    return true;
}

bool
MomentFFmpegModule::DeleteOldPoints()
{
    time_t diff = 864000; // 10 days

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t curTime = tv.tv_sec;

    std::map<time_t, StatMeasure>::iterator it = statPoints.begin();

    while(it != statPoints.end())
    {
        if(curTime - it->first > diff)
        {
            statPoints.erase(it++);
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

    std::map<time_t, StatMeasure>::iterator it = statPoints.begin();
    for(it; it != statPoints.end(); ++it)
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
        statToFile << it->second.user_util_avg << std::endl;
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
        logD_(_func_, "fail to open statFile: ", STATFILE);
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
            // push to statPoints
            statPoints[timestamp] = stm;
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

// TODO Always succeeds currently.
Result
MomentFFmpegModule::init (MomentServer * const moment)
{
    this->moment = moment;
    this->timers = moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers();
    this->page_pool = moment->getPagePool();

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
        logI_ (_func, opt_name, ": [", confd_dir, "]");
    }
    confd_dir = st_grab (new (std::nothrow) String (confd_dir_mem));

    ConstMemory record_dir_mem;
    {
        ConstMemory const opt_name = "mod_nvr/record_dir";
        bool record_dir_is_set = false;
        record_dir_mem = config->getString (opt_name, &record_dir_is_set);
        if (!record_dir_is_set) {
            logE_ (_func, opt_name, " config option is not set, disabling mod_nvr [actually mod_ffmpeg]");
            return Result::Failure;
        }
        logI_ (_func, opt_name, ": ", record_dir_mem);
    }
    record_dir = st_grab (new (std::nothrow) String (record_dir_mem));
    vfs = Vfs::createDefaultLocalVfs (record_dir_mem);

    media_viewer = grab (new (std::nothrow) MediaViewer);
    media_viewer->init (moment, vfs, record_dir);

    moment->setMediaSourceProvider (this);

    m_statMeasurer.Init(timers, 5);

    this->ReadStatFromFile();

    this->timer_keyStat = this->timers->addTimer (CbDesc<Timers::TimerCallback> (refreshTimerTickStat, this, this),
              300, // 5 minutes
              true /* periodical */,
              true /* auto_delete */);

    HttpReqHandler::addHandler(std::string("mod_nvr"), httpRequest, this);
    AdminHttpReqHandler::addHandler(std::string("mod_nvr_admin"), adminHttpRequest, this);

    return Result::Success;
}

MomentFFmpegModule::MomentFFmpegModule()
    : moment (NULL),
      timers (NULL),
      timer_keyStat (NULL),
      page_pool (NULL),
      serve_playlist_json (true)
{
    default_channel_opts = grab (new (std::nothrow) ChannelOptions);
    default_channel_opts->default_item = grab (new (std::nothrow) PlaybackItem);
}

MomentFFmpegModule::~MomentFFmpegModule ()
{
    if (this->timer_keyStat) {
        this->timers->deleteTimer (this->timer_keyStat);
        this->timer_keyStat = NULL;
    }

  StateMutexLock l (&mutex);

    {
	ChannelEntryHash::iter iter (channel_entry_hash);
	while (!channel_entry_hash.iter_done (iter)) {
	    ChannelEntry * const channel_entry = channel_entry_hash.iter_next (iter);
	    delete channel_entry;
	}
    }

    {
	RecorderEntryHash::iter iter (recorder_entry_hash);
	while (!recorder_entry_hash.iter_done (iter)) {
	    RecorderEntry * const recorder_entry = recorder_entry_hash.iter_next (iter);
	    delete recorder_entry;
	}
    }
}

} // namespace Moment

