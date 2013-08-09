/*  Moment-Gst - GStreamer support module for Moment Video Server
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


#ifndef MOMENT_GST__MOMENT_GST_MODULE__H__
#define MOMENT_GST__MOMENT_GST_MODULE__H__


#include <moment/libmoment.h>
#include <moment-gst/gst_stream.h>


namespace MomentGst {

using namespace M;
using namespace Moment;

class MomentGstModule : public Object,
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

    mt_const bool serve_playlist_json;
    mt_const StRef<String> playlist_json_protocol;

    mt_const Ref<ChannelOptions> default_channel_opts;

    mt_mutex (mutex) ChannelEntryHash channel_entry_hash;
    mt_mutex (mutex) RecorderEntryHash recorder_entry_hash;

    ChannelSet channel_set;

    Result updatePlaylist (ConstMemory  channel_name,
			   bool         keep_cur_item,
			   Ref<String> * mt_nonnull ret_err_msg);

    Result setPosition (ConstMemory channel_name,
			ConstMemory item_name,
			bool        item_name_is_id,
			ConstMemory seek_str);

    void printChannelInfoJson (PagePool::PageListHead *page_list,
			       ChannelEntry           *channel_entry);

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

    void parseSourcesConfigSection ();
    void parseChainsConfigSection ();
    Result parseStreamsConfigSection ();
    Result parseStreams ();
    void parseRecordingsConfigSection ();

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

    MomentGstModule ();

    ~MomentGstModule ();
};

}


#endif /* MOMENT_GST__MOMENT_GST_MODULE__H__ */

