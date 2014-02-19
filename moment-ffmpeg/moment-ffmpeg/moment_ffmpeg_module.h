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


#ifndef MOMENT_FFMPEG__MOMENT_FFMPEG_MODULE__H__
#define MOMENT_FFMPEG__MOMENT_FFMPEG_MODULE__H__

#include <map>
#include <vector>
#include <string>
#include <utility>
#include <sstream>
#include <fstream>
#include <sys/time.h>
#include <moment/libmoment.h>
#include <moment-ffmpeg/ffmpeg_stream.h>
#include <moment-ffmpeg/channel_checker.h>
#include <moment-ffmpeg/media_viewer.h>
#include <moment-ffmpeg/stat_measurer.h>
#include <moment-ffmpeg/rec_path_config.h>
#include <moment/moment_request_handler.h>


namespace MomentFFmpeg {

using namespace M;
using namespace Moment;


class MomentFFmpegModule : public Object,
                        public MediaSourceProvider
{
private:
    StateMutex m_mutex;

    enum DiskInfoRetVal
    {
        NOERR = 0,
        ERR,
        NOTIMPLEMENTED
    };

    struct DiskSizes
    {
        unsigned long allSize;
        unsigned long freeSize;
        unsigned long usedSize;
    };

    struct TimeInterval
    {
        TimeInterval():timeStart(0),timeEnd(0){}
        time_t timeStart;
        time_t timeEnd;
    };

    struct SourceState
    {
        SourceState():isAlive(false),isRestr(false),isWrite(false){}
        bool isAlive;   // true, if source exist
        bool isRestr;   // true, if source is restreaming
        bool isWrite;   // true, if source is writing on disk
    };

    struct SourceTimes
    {
        std::vector<TimeInterval> live;    // times of source's existence
        std::vector<TimeInterval> restr;   // times of source's restreaming
        std::vector<TimeInterval> write;   // times of source's writing
    };

    struct SourceStateTimes
    {
        SourceState state;
        SourceTimes times;
    };

    class ChannelEntry : public HashEntry<>
    {
    public:
	mt_const Ref<Channel> channel;
        mt_const Ref<ChannelOptions> channel_opts;

	mt_const Ref<String> channel_name;
        mt_const Ref<String> channel_title;
	mt_const Ref<String> channel_desc;
	mt_const Ref<String> playlist_filename;

        mt_const Ref<PushAgent>  push_agent;
        mt_const Ref<FetchAgent> fetch_agent;
    };

    typedef Hash< ChannelEntry,
		  Memory,
		  MemberExtractor< ChannelEntry,
				   Ref<String>,
				   &ChannelEntry::channel_name,
				   Memory,
				   AccessorExtractor< String,
						      Memory,
						      &String::mem > >,
		  MemoryComparator<> >
	    ChannelEntryHash;

    class RecorderEntry : public HashEntry<>
    {
    public:
	mt_const Ref<Recorder> recorder;

	mt_const Ref<String> recorder_name;
	mt_const Ref<String> playlist_filename;
    };

    typedef Hash< RecorderEntry,
		  Memory,
		  MemberExtractor< RecorderEntry,
				   Ref<String>,
				   &RecorderEntry::recorder_name,
				   Memory,
				   AccessorExtractor< String,
						      Memory,
						      &String::mem > >,
		  MemoryComparator<> >
	    RecorderEntryHash;

    mt_const MomentServer *m_pMoment;
    mt_const Timers *m_pTimers;
    mt_const PagePool *m_pPage_pool;
    mt_const StRef<String> m_confd_dir;
    mt_const StRef<String> m_recpath_conf;
    RecpathConfig m_recpath_config;
    Uint64 m_nDownloadLimit;

    mt_const bool m_bServe_playlist_json;

    mt_const Ref<ChannelOptions> m_default_channel_opts;

    mt_mutex (mutex) ChannelEntryHash m_channel_entry_hash;
    mt_mutex (mutex) RecorderEntryHash m_recorder_entry_hash;

    ChannelSet m_channel_set;

    std::map<std::string, WeakRef<FFmpegStream> > m_streams;

    mt_const Ref<MediaViewer>     m_media_viewer;

    // statistics stuff
    Timers::TimerKey m_timer_keyStat;
    Timers::TimerKey m_timer_updateTimes;
    StatMeasurer m_statMeasurer;

    std::map<time_t, StatMeasure> m_statPoints;

    std::map<std::string, SourceStateTimes> m_sourceStateTimes;

    bool CreateStatPoint();

    bool DeleteOldPoints();

    bool DumpStatInFile();

    bool ReadStatFromFile();

    void clearEmptyChannels();

    static void refreshTimerTickStat (void *_self);

    bool UpdateSourceTimes();

    static void refreshTimerSourceTimes (void *_self);
    //

    bool removeVideoFiles(StRef<String> const channel_name,
                                     Time const startTime, Time const endTime);

    Result updatePlaylist (ConstMemory  channel_name,
			   bool         keep_cur_item,
			   Ref<String> * mt_nonnull ret_err_msg);

    Result setPosition (ConstMemory channel_name,
			ConstMemory item_name,
			bool        item_name_is_id,
			ConstMemory seek_str);

    void printChannelInfoJson (PagePool::PageListHead *page_list,
			       ChannelEntry           *channel_entry);

    static StRef<String>  channelExistenceToJson (
            ChannelChecker::ChannelFileTimes * const mt_nonnull existence);

    static StRef<String>  channelFilesExistenceToJson (
            ChannelChecker::ChannelFileDiskTimes * const mt_nonnull chFileDiskTimes);

    static StRef<String>  statisticsToJson (
            std::map<time_t, StatMeasure> * const mt_nonnull statPoints);

    StRef<String> doGetFile (ConstMemory  stream_name,
                    Time         start_unixtime_sec,
                    Time         end_unixtime_sec);

    static Result httpGetChannelsStat (HttpRequest  * mt_nonnull req,
				       Sender       * mt_nonnull conn_sender,
				       void         *_self);


    static bool adminHttpRequest(HTTPServerRequest &req, HTTPServerResponse &resp, void * _self);

    static bool httpRequest(HTTPServerRequest &req, HTTPServerResponse &resp, void * _self);

    void createPlaylistChannel (ConstMemory     playlist_filename,
                                bool            is_dir,
                                bool            dir_re_read,
                                ChannelOptions *channel_opts,
                                PushAgent      *push_agent  = NULL,
                                FetchAgent     *fetch_agent = NULL);

    void createStreamChannel (ChannelOptions *channel_opts,
                              PlaybackItem   *playback_item,
                              PushAgent      *push_agent  = NULL,
                              FetchAgent     *fetch_agent = NULL);

    void createDummyChannel (ConstMemory  channel_name,
                             ConstMemory  channel_title,
			     ConstMemory  channel_desc,
                             PushAgent   *push_agent  = NULL,
                             FetchAgent  *fetch_agent = NULL);

    void createPlaylistRecorder (ConstMemory recorder_name,
				 ConstMemory playlist_filename,
				 ConstMemory filename_prefix);

    void createChannelRecorder (ConstMemory recorder_name,
				ConstMemory channel_name,
				ConstMemory filename_prefix);

    static int getDiskInfo(std::map<std::string, DiskSizes > & diskInfo);

    static bool getDiskInfo (std::string & json_respond);

public:
  mt_iface (MediaSourceProvider)
    Ref<MediaSource> createMediaSource (CbDesc<MediaSource::Frontend> const &frontend,
                                        Timers            *timers,
                                        DeferredProcessor *deferred_processor,
                                        PagePool          *page_pool,
                                        VideoStream       *video_stream,
                                        VideoStream       *mix_video_stream,
                                        Time               initial_seek,
                                        ChannelOptions    *channel_opts,
                                        PlaybackItem      *playback_item);
  mt_iface_end

    Result init (MomentServer *moment);

    void RefreshStat ();

    MomentFFmpegModule ();

    ~MomentFFmpegModule ();
};

}


#endif /* MOMENT_FFMPEG__MOMENT_FFMPEG_MODULE__H__ */

