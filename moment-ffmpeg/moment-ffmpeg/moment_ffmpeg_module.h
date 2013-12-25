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
#include <moment-ffmpeg/get_file_session.h>
#include <moment-ffmpeg/stat_measurer.h>
#include <moment/moment_request_handler.h>


namespace MomentFFmpeg {

using namespace M;
using namespace Moment;

class MomentFFmpegModule : public Object,
                        public MediaSourceProvider
{
private:
    StateMutex mutex;

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

    mt_const MomentServer *moment;
    mt_const Timers *timers;
    mt_const PagePool *page_pool;
    mt_const StRef<String> record_dir;
    mt_const StRef<String> confd_dir;
    Ref<Vfs> vfs;

    mt_const bool serve_playlist_json;
    mt_const StRef<String> playlist_json_protocol;

    mt_const Ref<ChannelOptions> default_channel_opts;

    mt_mutex (mutex) ChannelEntryHash channel_entry_hash;
    mt_mutex (mutex) RecorderEntryHash recorder_entry_hash;

    ChannelSet channel_set;

    std::map<std::string, WeakRef<FFmpegStream> > m_streams;

    mt_mutex (mutex) List< Ref<GetFileSession> > get_file_sessions;

    std::map<std::string, Ref<ChannelChecker> > m_channel_checkers;
    mt_const Ref<MediaViewer>     media_viewer;

    // statistics stuff
    Timers::TimerKey timer_keyStat;
    StatMeasurer m_statMeasurer;

    std::map<time_t, StatMeasure> statPoints;

    bool CreateStatPoint();

    bool DeleteOldPoints();

    bool DumpStatInFile();

    bool ReadStatFromFile();

    void clearEmptyChannels();

    static void refreshTimerTickStat (void *_self);
    //

    bool removeVideoFiles(StRef<String> const dir_name, StRef<String> const channel_name,
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
            std::vector<std::pair<int,int>> * const mt_nonnull existence);

    static StRef<String>  channelFilesExistenceToJson (
            ChannelChecker::ChannelFileSummary * const mt_nonnull files_existence);

    static StRef<String>  statisticsToJson (
            std::map<time_t, StatMeasure> * const mt_nonnull statPoints);

    mt_iface (GetFileSession::Frontend)
      static GetFileSession::Frontend const get_file_session_frontend;

      static void getFileSession_done (Result  res,
                                       void   *_self);
    mt_iface_end

    StRef<String> doGetFile (HttpRequest * mt_nonnull req,
                    Sender      * mt_nonnull sender,
                    ConstMemory  stream_name,
                    Time         start_unixtime_sec,
                    Time         end_unixtime_sec);

    static Result httpGetChannelsStat (HttpRequest  * mt_nonnull req,
				       Sender       * mt_nonnull conn_sender,
				       void         *_self);

    mt_iface (HttpService::Frontend)
      static HttpService::HttpHandler admin_http_handler;

      static Result adminHttpRequest (HttpRequest  * mt_nonnull req,
				      Sender       * mt_nonnull conn_sender,
				      Memory const &msg_body,
				      void        ** mt_nonnull ret_msg_data,
				      void         *_self);

      static HttpService::HttpHandler http_handler;

      static Result httpRequest (HttpRequest  * mt_nonnull req,
				 Sender       * mt_nonnull conn_sender,
				 Memory const &msg_body,
				 void        ** mt_nonnull ret_msg_data,
				 void         *_self);
    mt_iface_end

    static bool _adminHttpRequest(HTTPServerRequest &req, HTTPServerResponse &resp, void * _self);

    static bool _httpRequest(HTTPServerRequest &req, HTTPServerResponse &resp, void * _self);

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

