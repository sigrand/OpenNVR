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
//#include <gst/gst.h>

#include <moment/libmoment.h>

#include <moment-ffmpeg/moment_ffmpeg_module.h>


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

HttpService::HttpHandler MomentFFmpegModule::admin_http_handler = {
    adminHttpRequest,
    NULL /* httpMessageBody */
};

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

Result
MomentFFmpegModule::httpGetChannelsStat (HttpRequest  * const mt_nonnull req,
				      Sender       * const mt_nonnull conn_sender,
				      void         * const _self)
{
    MomentFFmpegModule * const self = static_cast <MomentFFmpegModule*> (_self);

    MOMENT_FFMPEG__HEADERS_DATE

    PagePool::PageListHead page_list;

    static char const prefix [] =
	    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
	    "<html style=\"height: 100%\" xmlns=\"http://www.w3.org/1999/xhtml\">\n"
	    "<head>\n"
	    "  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"/>\n"
	    "  <title>Traffic stats</title>\n"
	    "  <style type=\"text/css\">\n"
	    "    .top_table      { border-right: 1px solid #777777; border-bottom: 1px solid #777777;\n"
            "                      margin-top: 5px; }\n"
	    "    .top_table td   { border-left:  1px solid #777777; border-top:    1px solid #777777; padding: 3px; }\n"
	    "    .inner_table    { border: 0; }\n"
	    "    .inner_table td { border: 0; padding: 0; }\n"
	    "    .total td       { border-top:   2px solid #444444; }\n"
	    "  </style>\n"
	    "</head>\n"
	    "<body style=\"font-family: sans-serif\">\n"
	    "<form name=\"reset\" action=\"channels_stat_reset\" method=\"post\">\n"
	    "  <input type=\"submit\" value=\"Сброс статистики\">\n"
	    "</form>\n"
	    "<table class=\"top_table\" border=\"0\" cellpadding=\"0\" cellspacing=\"0\">\n"
	    "<tr>\n"
	    "<td>Канал</td><td>Описание</td><td>Время<sup>1</sup> ЧЧ:ММ</td><td>Время<sup>1</sup>, сек</td>"
	    "<td>Ширина канала<sup>2</sup>, Мбит/сек</td>"
	    "<td>Получено<sup>3</sup>, ГБайт</td><td>Получено<sup>3</sup>, байт</td><td>Ген.<sup>4</sup>, Мбит/сек</td>"
	    "<td>Видео<sup>5</sup>, байт</td><td>Аудио<sup>6</sup>, байт</td>\n"
	    "</tr>\n";

    static char const suffix [] =
	    "</table>\n"
	    "<p>1 &mdash; Время, прошедшее с момента создания канала, включая время, когда камера была недоступна;<br/>\n"
	    "2 &mdash; Усреднённая скорость поступления видеоданных от камеры. Ширина = Получено / Время;<br/>\n"
	    "3 &mdash; Объём полученных с камеры видео/аудиоданных. Накладные расходы транспортных протоколов (HTTP, RTSP) не включены;<br/>\n"
	    "4 &mdash; Усреднённый битрейт генерируемого видеопотока, т.е. потока, который будет отдан смотрящим клиентам;<br/>\n"
	    "5 &mdash; Общий объём перекодированного видео, подготовленного для отдачи клиентам (только видео, без аудио);<br/>\n"
	    "6 &mdash; Общий объём аудио, подготовленного для отдачи клиентам.</p>\n"
	    "</body>\n"
	    "</html>\n";

    self->page_pool->getFillPages (&page_list, ConstMemory (prefix, sizeof (prefix) - 1));

    self->mutex.lock ();

    double width_total = 0.0;
    double rx_total = 0.0;
    {
	ChannelEntryHash::iter iter (self->channel_entry_hash);
	while (!self->channel_entry_hash.iter_done (iter)) {
	    ChannelEntry * const channel_entry = self->channel_entry_hash.iter_next (iter);

	    if (!channel_entry->channel)
		continue;

	    Channel::TrafficStats traffic_stats;
	    channel_entry->channel->getTrafficStats (&traffic_stats);
	    Uint64 const rx_bytes = traffic_stats.rx_bytes;

	    double const width =
		    traffic_stats.time_elapsed != 0 ?
			    (double) rx_bytes / (double) traffic_stats.time_elapsed * 8.0 / (1024.0 * 1024.0) : 0.0;

	    width_total += width;
	    rx_total += (double) rx_bytes;

	    Format fmt_time;
	    fmt_time.min_digits = 2;
	    Format fmt;
	    fmt.precision = 3;
	    Ref<String> const line_str = makeString (
		    "<tr>"
		    "<td>"
		    "  <table class=\"inner_table\" border=\"0\" cellpadding=\"0\" cellspacing=\"0\"><tr>\n"
		    "    <td style=\"padding-right: 3px\">\n"
		    "      <form name=\"reset\" action=\"channel_reconnect?name=", channel_entry->channel_name->mem(), "\" method=\"post\">\n"
		    "        <input type=\"submit\" value=\"Переподкл.\">\n"
		    "      </form>\n"
		    "    </td>\n"
		    "    <td>", channel_entry->channel_name->mem(), "</td>\n"
		    "  </tr></table>\n"
		    "</td>\n"
		    "<td>", channel_entry->channel_desc->mem(), "</td>"
		    "<td>", fmt_time, traffic_stats.time_elapsed / 3600, ":", (traffic_stats.time_elapsed % 3600) / 60, "</td>"
		    "<td>", fmt_def, traffic_stats.time_elapsed, "</td>"
		    "<td>", fmt, width, "</td>"
		    "<td>", (double) rx_bytes / (1024.0 * 1024.0 * 1024.0), "</td>"
		    "<td>", rx_bytes, "</td>"
		    "<td>", (traffic_stats.time_elapsed != 0 ?
				    (double) (traffic_stats.rx_video_bytes + traffic_stats.rx_audio_bytes) /
					    (double) traffic_stats.time_elapsed * 8.0 / (1024.0 * 1024.0) : 0), "</td>"
		    "<td>", traffic_stats.rx_video_bytes, "</td>"
		    "<td>", traffic_stats.rx_audio_bytes, "</td>"
		    "</tr>");
	    self->page_pool->getFillPages (&page_list, line_str->mem());
	}
    }
    {
	Format fmt;
	fmt.precision = 3;
	Ref<String> const line_str = makeString (
		"<tr class=\"total\">"
		"<td colspan=\"2\"><b>Всего:</b></td>"
		"<td/>"
		"<td/>"
		"<td>", fmt, width_total, "</td>"
		"<td>", rx_total / (1024.0 * 1024.0 * 1024.0), "</td>"
		"<td></td>"
		"<td></td>"
		"<td></td>"
		"<td></td>"
		"</tr>");
	self->page_pool->getFillPages (&page_list, line_str->mem());
    }

    self->mutex.unlock ();

    self->page_pool->getFillPages (&page_list, ConstMemory (suffix, sizeof (suffix) - 1));

    Size content_len = 0;
    {
	PagePool::Page *page = page_list.first;
	while (page) {
	    content_len += page->data_len;
	    page = page->getNextMsgPage();
	}
    }

    conn_sender->send (self->page_pool,
		       false /* do_flush */,
		       MOMENT_FFMPEG__OK_HEADERS ("text/html", content_len),
		       "\r\n");
    conn_sender->sendPages (self->page_pool, page_list.first, true /* do_flush */);

    if (!req->getKeepalive())
        conn_sender->closeAfterFlush();

    logA_ ("mod_ffmpeg 200 ", req->getClientAddress(), " ", req->getRequestLine());

    return Result::Success;
}

Result
MomentFFmpegModule::adminHttpRequest (HttpRequest  * const mt_nonnull req,
				   Sender       * const mt_nonnull conn_sender,
				   Memory const & /* msg_body */,
				   void        ** const mt_nonnull /* ret_msg_data */,
				   void         * const _self)
{
    MomentFFmpegModule * const self = static_cast <MomentFFmpegModule*> (_self);

    logD_ (_func_);

    MOMENT_FFMPEG__HEADERS_DATE

//    if (req->getNumPathElems() >= 2
//        && (equal (req->getPath (1), "rec_on") ||
//            equal (req->getPath (1), "rec_off")))
//    {
//        ConstMemory const channel_name = req->getParameter ("stream");
//        ConstMemory const seq = req->getParameter ("seq");

//        ChannelRecorder::ChannelState channel_state;

//        bool const set_on = equal (req->getPath (1), "rec_on");

//        if(!recording_state_dir->isNullString())
//        {
//            StRef<String> const path = st_makeString (recording_state_dir->mem(), "/", channel_name);

//            bool file_exist = false;
//            if (FILE *file = fopen(path->cstr(), "r")) {
//                fclose(file);
//                file_exist = true;
//            }

//            if(file_exist && set_on)
//            {
//                if( remove( path->cstr() ) != 0 )
//                    logE_ (_func, "could not delete file", path->cstr());
//                else
//                    logD_ (_func, "removed file", path->cstr());
//            }
//            else if (!file_exist && !set_on)
//            {
//                FILE * fp = fopen( path->cstr(), "w");
//                if(fp == NULL)
//                    logE_ (_func, "could not create file [", path->cstr(),"]\n");
//                else
//                {
//                    logD_ (_func, "created file [", path->cstr(),"]\n");
//                    fclose(fp);
//                }
//            }
//        }


//        ChannelRecorder::ChannelResult res = self->channel_recorder->setRecording (channel_name, set_on);
//        if (res == ChannelRecorder::ChannelResult_Success)
//            res = self->channel_recorder->getChannelState (channel_name, &channel_state);

//        if (res == ChannelRecorder::ChannelResult_ChannelNotFound) {
//            ConstMemory const reply_body = "404 Channel Not Found (mod_nvr_admin)";
//            conn_sender->send (self->page_pool,
//                               true /* do_flush */,
//                               MOMENT_SERVER__404_HEADERS (reply_body.len()),
//                               "\r\n",
//                               reply_body);
//            logA_ ("mod_nvr_admin 404 ", req->getClientAddress(), " ", req->getRequestLine());
//            goto _return;
//        } else
//        if (res == ChannelRecorder::ChannelResult_Failure) {
//            ConstMemory const reply_body = "500 Internal Server Error (mod_nvr_admin)";
//            conn_sender->send (self->page_pool,
//                               true /* do_flush */,
//                               MOMENT_SERVER__404_HEADERS (reply_body.len()),
//                               "\r\n",
//                               reply_body);
//            logA_ ("mod_nvr_admin 500 ", req->getClientAddress(), " ", req->getRequestLine());
//            goto _return;
//        }
//        assert (res == ChannelRecorder::ChannelResult_Success);

//        StRef<String> const reply_body = channelStateToJson (&channel_state, seq);
//        logD_ (_func, "reply: ", reply_body);
//        conn_sender->send (self->page_pool,
//                           true /* do_flush */,
//                           MOMENT_SERVER__OK_HEADERS ("text/html", reply_body->len()),
//                           "\r\n",
//                           reply_body->mem());

//        logA_ ("mod_nvr_admin 200 ", req->getClientAddress(), " ", req->getRequestLine());
//    } else
//    if(req->getNumPathElems() >= 2 &&
//            (equal (req->getPath (1), "files_existence"))) {
//        ConstMemory const channel_name = req->getParameter ("stream");
//        std::vector<ChannelChecker::ChannelFileSummary> * files_existence =
//            self->channel_checker->getChannelFilesExistence (channel_name);

//        if (files_existence == NULL) {
//            ConstMemory const reply_body = "404 Channel Not Found (mod_nvr)";
//            conn_sender->send (self->page_pool,
//                               true /* do_flush */,
//                               MOMENT_SERVER__404_HEADERS (reply_body.len()),
//                               "\r\n",
//                               reply_body);
//            logA_ ("mod_nvr 404 ", req->getClientAddress(), " ", req->getRequestLine());
//            goto _return;
//        }

//        StRef<String> const reply_body = channelFilesExistenceToJson (files_existence);
//        logD_ (_func, "reply: ", reply_body);
//        conn_sender->send (self->page_pool,
//                           true /* do_flush */,
//                           MOMENT_SERVER__OK_HEADERS ("text/html", reply_body->len()),
//                           "\r\n",
//                           reply_body->mem());
//    }
//    else
    {
        logE_ (_func, "Unknown request: ", req->getFullPath());

        ConstMemory const reply_body = "404 Not Found (mod_nvr_admin) [ffmpeg]";
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__404_HEADERS (reply_body.len()),
                           "\r\n",
                           reply_body);

        logA_ ("mod_nvr 404 ", req->getClientAddress(), " ", req->getRequestLine());
    }

    goto _return;


_return:
    if (!req->getKeepalive())
        conn_sender->closeAfterFlush ();

    return Result::Success;
}

HttpService::HttpHandler MomentFFmpegModule::http_handler = {
    httpRequest,
    NULL /* httpMessageBody */
};

Result
MomentFFmpegModule::httpRequest (HttpRequest  * const mt_nonnull req,
			      Sender       * const mt_nonnull conn_sender,
			      Memory const & /* msg_body */,
			      void        ** const mt_nonnull /* ret_msg_data */,
			      void         * const _self)
{
    MomentFFmpegModule * const self = static_cast <MomentFFmpegModule*> (_self);

    logD_ (_func, req->getRequestLine());

    MOMENT_SERVER__HEADERS_DATE

    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "unixtime"))
    {
        StRef<String> const unixtime_str = st_makeString (getUnixtime());
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           // TODO No cache
                           MOMENT_SERVER__OK_HEADERS ("text/html", unixtime_str->len()),
                           "\r\n",
                           unixtime_str);

        logA_ ("mod_nvr 200 ", req->getClientAddress(), " ", req->getRequestLine());
    }
//    else if (req->getNumPathElems() >= 2
//        && equal (req->getPath (1), "channel_state"))
//    {
//        ConstMemory const channel_name = req->getParameter ("stream");
//        ConstMemory const seq = req->getParameter ("seq");

//        ChannelRecorder::ChannelState channel_state;
//        ChannelRecorder::ChannelResult const res =
//                self->channel_recorder->getChannelState (channel_name, &channel_state);
//        if (res == ChannelRecorder::ChannelResult_ChannelNotFound) {
//            ConstMemory const reply_body = "404 Channel Not Found (mod_nvr)";
//            conn_sender->send (self->page_pool,
//                               true /* do_flush */,
//                               MOMENT_SERVER__404_HEADERS (reply_body.len()),
//                               "\r\n",
//                               reply_body);
//            logA_ ("mod_nvr 404 ", req->getClientAddress(), " ", req->getRequestLine());
//            goto _return;
//        } else
//        if (res == ChannelRecorder::ChannelResult_Failure) {
//            ConstMemory const reply_body = "500 Internal Server Error (mod_nvr)";
//            conn_sender->send (self->page_pool,
//                               true /* do_flush */,
//                               MOMENT_SERVER__404_HEADERS (reply_body.len()),
//                               "\r\n",
//                               reply_body);
//            logA_ ("mod_nvr 500 ", req->getClientAddress(), " ", req->getRequestLine());
//            goto _return;
//        }
//        assert (res == ChannelRecorder::ChannelResult_Success);

//        StRef<String> const reply_body = channelStateToJson (&channel_state, seq);
//        conn_sender->send (self->page_pool,
//                           true /* do_flush */,
//                           MOMENT_SERVER__OK_HEADERS ("text/html", reply_body->len()),
//                           "\r\n",
//                           reply_body->mem());

//        logA_ ("mod_nvr 200 ", req->getClientAddress(), " ", req->getRequestLine());
//    } else
//    if (req->getNumPathElems() >= 2
//        && (equal (req->getPath (1), "file") ||
//            stringHasSuffix (req->getPath (1), ".mp4", NULL /* ret_str */)))
//    {
//        ConstMemory const channel_name = req->getParameter ("stream");

//        Uint64 start_unixtime_sec = 0;
//        if (!strToUint64_safe (req->getParameter ("start"), &start_unixtime_sec, 10 /* base */)) {
//            logE_ (_func, "Bad \"start\" request parameter value");
//            goto _bad_request;
//        }

//        Uint64 duration_sec = 0;
//        if (!strToUint64_safe (req->getParameter ("duration"), &duration_sec, 10 /* base */)) {
//            logE_ (_func, "Bad \"duration\" request parameter value");
//            goto _bad_request;
//        }

//        bool const download = req->hasParameter ("download");
//        self->doGetFile (req, conn_sender, channel_name, start_unixtime_sec, duration_sec, download);
//        return Result::Success;
//    } else
//    if (req->getNumPathElems() >= 2 && (equal (req->getPath (1), "existence"))) {
//        ConstMemory const channel_name = req->getParameter ("stream");
//        // ConstMemory const seq = req->getParameter ("seq"); TODO doesn't know meaning of sequence, so omit it

//        std::vector<std::pair<int,int>> * channel_existence = self->channel_checker->getChannelExistence (channel_name);

//        if (channel_existence == NULL) {
//            ConstMemory const reply_body = "404 Channel Not Found (mod_nvr)";
//            conn_sender->send (self->page_pool,
//                               true /* do_flush */,
//                               MOMENT_SERVER__404_HEADERS (reply_body.len()),
//                               "\r\n",
//                               reply_body);
//            logA_ ("mod_nvr 404 ", req->getClientAddress(), " ", req->getRequestLine());
//            goto _return;
//        }

//        StRef<String> const reply_body = channelExistenceToJson (channel_existence);
//        logD_ (_func, "reply: ", reply_body);
//        conn_sender->send (self->page_pool,
//                           true /* do_flush */,
//                           MOMENT_SERVER__OK_HEADERS ("text/html", reply_body->len()),
//                           "\r\n",
//                           reply_body->mem());
//    }else {
//        logE_ (_func, "Unknown request: ", req->getFullPath());

//        ConstMemory const reply_body = "404 Not Found (mod_nvr)";
//        conn_sender->send (self->page_pool,
//                           true /* do_flush */,
//                           MOMENT_SERVER__404_HEADERS (reply_body.len()),
//                           "\r\n",
//                           reply_body);

//        logA_ ("mod_nvr 404 ", req->getClientAddress(), " ", req->getRequestLine());
//    }

    goto _return;

_bad_request:
    {
        MOMENT_SERVER__HEADERS_DATE
        ConstMemory const reply_body = "400 Bad Request (mod_nvr)";
        conn_sender->send (
                self->page_pool,
                true /* do_flush */,
                MOMENT_SERVER__400_HEADERS (reply_body.len()),
                "\r\n",
                reply_body);

        logA_ ("mod_nvr 400 ", req->getClientAddress(), " ", req->getRequestLine());
    }

_return:
    if (!req->getKeepalive())
        conn_sender->closeAfterFlush ();

    return Result::Success;
}

void
MomentFFmpegModule::parseSourcesConfigSection ()
{
    logD_ (_func_);

    MConfig::Config * const config = moment->getConfig();

    MConfig::Section * const src_section = config->getSection ("mod_ffmpeg/sources");
    if (!src_section) {
//	logI_ ("No video stream sources specified "
//	       "(\"mod_ffmpeg/sources\" config section is missing).");
	return;
    }

    MConfig::Section::iter iter (*src_section);
    while (!src_section->iter_done (iter)) {
	MConfig::SectionEntry * const section_entry = src_section->iter_next (iter);
	if (section_entry->getType() == MConfig::SectionEntry::Type_Option) {
	    MConfig::Option * const src_option = static_cast <MConfig::Option*> (section_entry);
	    if (!src_option->getValue())
		continue;

	    ConstMemory const stream_name = src_option->getName();
	    ConstMemory const stream_uri = src_option->getValue()->mem();

	    logD_ (_func, "Stream name: ", stream_name, "; stream uri: ", stream_uri);

            Ref<ChannelOptions> const opts = grab (new (std::nothrow) ChannelOptions);
            {
                *opts = *default_channel_opts;
                opts->channel_name  = st_grab (new (std::nothrow) String (stream_name));
                opts->channel_title = st_grab (new (std::nothrow) String (stream_name));
                opts->channel_desc  = st_grab (new (std::nothrow) String);
            }

            Ref<PlaybackItem> const item = grab (new (std::nothrow) PlaybackItem);
            opts->default_item = item;
            {
                *item = *default_channel_opts->default_item;
                item->stream_spec = st_grab (new (std::nothrow) String (stream_uri));
                item->spec_kind = PlaybackItem::SpecKind::Uri;
            }

	    createStreamChannel (opts, item);
        }
    }
}

void
MomentFFmpegModule::parseChainsConfigSection ()
{
    logD_ (_func_);

    MConfig::Config * const config = moment->getConfig();

#if 0
// Debugging
    do {
	logD_ (_func, "Iterating through \"mod_ffmpeg\" section");
	MConfig::Section * const root_section = config->getSection ("mod_ffmpeg");
	MConfig::Section::iter iter (*root_section);
	while (!root_section->iter_done (iter)) {
	    MConfig::SectionEntry * const section_entry = root_section->iter_next (iter);
	    switch (section_entry->getType()) {
		case MConfig::SectionEntry::Type_Section: {
		    MConfig::Section * const subsection = static_cast <MConfig::Section*> (section_entry);
		    logD_ (_func, section_entry->getName(), ", section");
		} break;
		default:
		    logD_ (_func, section_entry->getName(), ", not a section");
	    }
	}
    } while (0);
#endif

    MConfig::Section * const chains_section = config->getSection ("mod_ffmpeg/chains");
    if (!chains_section) {
//	logI_ ("No custom chains specified "
//	       "(\"mod_ffmpeg/chains\" config section is missing).");
	return;
    }

    MConfig::Section::iter iter (*chains_section);
    while (!chains_section->iter_done (iter)) {
	MConfig::SectionEntry * const section_entry = chains_section->iter_next (iter);
	if (section_entry->getType() == MConfig::SectionEntry::Type_Option) {
	    MConfig::Option * const chain_option = static_cast <MConfig::Option*> (section_entry);
	    if (!chain_option->getValue())
		continue;

#if 0
// Unused
	    VideoStream::VideoCodecId video_codec = VideoStream::VideoCodecId::SorensonH263;
	    {
		MConfig::Option::iter iter (*chain_option);
		assert (!chain_option->iter_done (iter));
		chain_option->iter_next (iter);
		if (!chain_option->iter_done (iter)) {
		    ConstMemory const codec_str = chain_option->iter_next (iter)->mem();
		    if (equal (codec_str, "flv")) {
			logD_ (_func, "codec: ", codec_str);
			video_codec = VideoStream::VideoCodecId::SorensonH263;
		    } else
		    if (equal (codec_str, "flashsv")) {
			logD_ (_func, "codec: ", codec_str);
			video_codec = VideoStream::VideoCodecId::ScreenVideo;
		    } else {
			logW_ (_func, "unknown codec \"", codec_str, "\"specified, ignoring chain");
			continue;
		    }
		}
	    }
#endif

	    ConstMemory const stream_name = chain_option->getName();
	    ConstMemory const chain_spec = chain_option->getValue()->mem();

            Ref<ChannelOptions> const opts = grab (new (std::nothrow) ChannelOptions);
            {
                *opts = *default_channel_opts;
                opts->channel_name  = st_grab (new (std::nothrow) String (stream_name));
                opts->channel_title = st_grab (new (std::nothrow) String (stream_name));
                opts->channel_desc  = st_grab (new (std::nothrow) String);
            }

            Ref<PlaybackItem> const item = grab (new (std::nothrow) PlaybackItem);
            opts->default_item = item;
            {
                *item = *default_channel_opts->default_item;
                item->stream_spec = st_grab (new (std::nothrow) String (chain_spec));
                item->spec_kind = PlaybackItem::SpecKind::Chain;
            }

	    createStreamChannel (opts, item);
	} else
	if (section_entry->getType() == MConfig::SectionEntry::Type_Section) {
	    MConfig::Section * const section = static_cast <MConfig::Section*> (section_entry);

	    ConstMemory stream_name;
	    bool got_stream_name = false;

	    ConstMemory chain_spec;
	    bool got_chain_spec = false;

	    ConstMemory record_path;
	    bool got_record_path = false;

	    {
		MConfig::Section::iter iter (*section);
		while (!section->iter_done (iter)) {
		    MConfig::SectionEntry * const section_entry = section->iter_next (iter);
		    if (section_entry->getType() != MConfig::SectionEntry::Type_Option)
			continue;

		    MConfig::Option * const option = static_cast <MConfig::Option*> (section_entry);
		    ConstMemory const opt_name = option->getName ();
		    MConfig::Value * const opt_val = option->getValue ();
		    if (equal (opt_name, "name")) {
			if (opt_val) {
			    stream_name = opt_val->mem();
			    got_stream_name = true;
			}
		    } else
		    if (equal (opt_name, "chain")) {
			if (opt_val) {
			    chain_spec = opt_val->mem();
			    got_chain_spec = true;
			}
		    } else
		    if (equal (opt_name, "record_path")) {
			if (opt_val) {
			    record_path = opt_val->mem();
			    got_record_path = true;
			}
		    } else {
			logW_ (_func, "Unknown chain option (\"chains\" section): ", opt_name);
		    }
		}
	    }

	    if (!got_stream_name) {
		logE_ (_func, "Stream name not specified (\"chains\" section)");
		continue;
	    }

	    if (!got_chain_spec) {
		logE_ (_func, "No chain specification for stream "
		       "\"", stream_name, "\" (\"chains\" section)");
		continue;
	    }

            Ref<ChannelOptions> const opts = grab (new (std::nothrow) ChannelOptions);
            {
                *opts = *default_channel_opts;
                opts->channel_name  = st_grab (new (std::nothrow) String (stream_name));
                opts->channel_title = st_grab (new (std::nothrow) String (stream_name));
                opts->channel_desc  = st_grab (new (std::nothrow) String);

                opts->recording = got_record_path;
                opts->record_path = st_grab (new (std::nothrow) String (record_path));
            }

            Ref<PlaybackItem> const item = grab (new (std::nothrow) PlaybackItem);
            opts->default_item = item;
            {
                *item = *default_channel_opts->default_item;
                item->stream_spec = st_grab (new (std::nothrow) String (chain_spec));
                item->spec_kind = PlaybackItem::SpecKind::Chain;
            }

	    createStreamChannel (opts, item);
	}
    }
}

Result
MomentFFmpegModule::parseStreamsConfigSection ()
{
    logD_ (_func_);

    MConfig::Config * const config = moment->getConfig();

    MConfig::Section * const streams_section = config->getSection ("mod_ffmpeg/streams");
    if (!streams_section)
	return Result::Success;

    MConfig::Section::iter streams_iter (*streams_section);
    while (!streams_section->iter_done (streams_iter)) {
	MConfig::SectionEntry * const item_entry = streams_section->iter_next (streams_iter);
	if (item_entry->getType() == MConfig::SectionEntry::Type_Section) {
	    MConfig::Section* const item_section = static_cast <MConfig::Section*> (item_entry);

	    logD_ (_func, "section");

	    StRef<String> stream_name;
	    {
		MConfig::Option * const opt = item_section->getOption ("name");
		if (opt && opt->getValue()) {
		    stream_name = opt->getValue()->getAsString();
		    logD_ (_func, "stream_name: ", stream_name);
		}

		if (!stream_name) {
		    logW_ (_func, "Unnamed stream in section mod_ffmpeg/streams");
		    stream_name = st_grab (new (std::nothrow) String);
		}
	    }

            StRef<String> channel_title;
            {
                MConfig::Option * const opt = item_section->getOption ("title");
                if (opt && opt->getValue()) {
                    channel_title = opt->getValue()->getAsString();
                    logD_ (_func, "channel_title: ", channel_title);
                }

                if (!channel_title)
                    channel_title = stream_name;
            }

	    StRef<String> channel_desc;
	    {
		MConfig::Option * const opt = item_section->getOption ("desc");
		if (opt && opt->getValue()) {
		    channel_desc = opt->getValue()->getAsString();
		    logD_ (_func, "channel_desc: ", channel_desc);
		}

		if (!channel_desc)
		    channel_desc = st_grab (new (std::nothrow) String);
	    }

	    StRef<String> chain;
	    StRef<String> uri;
	    StRef<String> playlist;
            StRef<String> playlist_dir;
	    {
		int num_set_opts = 0;

		{
		    MConfig::Option * const opt = item_section->getOption ("chain");
		    if (opt && opt->getValue()) {
			chain = opt->getValue()->getAsString();
			logD_ (_func, "chain: ", chain);
			++num_set_opts;
		    }
		}

		{
		    MConfig::Option * const opt = item_section->getOption ("uri");
		    if (opt && opt->getValue()) {
			uri = opt->getValue()->getAsString();
			logD_ (_func, "uri: ", uri);
			++num_set_opts;
		    }
		}

		{
		    MConfig::Option * const opt = item_section->getOption ("playlist");
		    if (opt && opt->getValue()) {
			playlist = opt->getValue()->getAsString();
			logD_ (_func, "playlist: ", playlist);
			++num_set_opts;
		    }
		}

                {
                    MConfig::Option * const opt = item_section->getOption ("dir");
                    if (opt && opt->getValue()) {
                        playlist_dir = opt->getValue()->getAsString();
                        logD_ (_func, "dir: ", playlist_dir);
                        ++num_set_opts;
                    }
                }

		if (num_set_opts > 1) {
		    logW_ (_func, "Only one of uri/chain/playlist "
			   "should be specified for stream \"", stream_name, "\"");
		}
	    }

            bool playlist_dir_re_read = true;
            {
                ConstMemory const opt_name = "dir_re_read";
                if (!configSectionGetBoolean (item_section, opt_name, &playlist_dir_re_read, playlist_dir_re_read))
                    return Result::Failure;
                logI_ (_func, "stream ", stream_name->mem(), ": ", opt_name, ": ", playlist_dir_re_read);
            }

	    StRef<String> record_path;
	    {
		MConfig::Option * const opt = item_section->getOption ("record_path");
		if (opt && opt->getValue()) {
		    record_path = opt->getValue()->getAsString();
		    logD_ (_func, "record_path: ", record_path);
		}
	    }

            bool connect_on_demand = default_channel_opts->connect_on_demand;
            {
                ConstMemory const opt_name = "connect_on_demand";
                if (!configSectionGetBoolean (item_section, opt_name, &connect_on_demand, connect_on_demand))
                    return Result::Failure;
                logI_ (_func, "stream ", stream_name->mem(), ": ", opt_name, ": ", connect_on_demand);
            }

            Time connect_on_demand_timeout = default_channel_opts->connect_on_demand_timeout;
            {
                ConstMemory const opt_name = "connect_on_demand_timeout";
                MConfig::Option * const opt = item_section->getOption (opt_name);
                if (opt && opt->getValue()) {
                    Uint64 tmp_uint64;
                    if (!opt->getValue()->getAsUint64 (&tmp_uint64)) {
                        logE_ (_func, "Bad value for \"", opt_name, "\" option: ", opt->getValue()->mem());
                        return Result::Failure;
                    }
                    connect_on_demand_timeout = (Time) tmp_uint64;
                }
            }

            Ref<PushAgent> push_agent;
            {
                Ref<String> push_uri;
                {
                    ConstMemory const opt_name = "push_uri";
                    MConfig::Option * const opt = item_section->getOption (opt_name);
                    if (opt && opt->getValue()) {
                        push_uri = opt->getValue()->getAsString();
                        logD_ (_func, opt_name, ": ", push_uri);
                    }
                }

#if 0
// TODO Unused?
                Ref<String> push_server;
                {
                    ConstMemory const opt_name = "push_server";
                    MConfig::Option * const opt = item_section->getOption (opt_name);
                    if (opt && opt->getValue()) {
                        push_server = opt->getValue()->getAsString();
                        logD_ (_func, opt_name, ": ", push_server);
                    } else {
                        push_server = grab (new String);
                    }
                }

                Uint64 push_port = 1935;
                {
                    ConstMemory const opt_name = "push_port";
                    MConfig::Option * const opt = item_section->getOption (opt_name);
                    if (opt && opt->getValue()) {
                        if (!opt->getValue()->getAsUint64 (&push_port)) {
                            logE_ (_func, "Bad value for \"", opt_name, "\" option: ", opt->getValue()->mem());
                            return Result::Failure;
                        }
                        logD_ (_func, opt_name, ": ", push_port);
                    } else {
                        if (push_server && !push_server->isNull())
                            logD_ (_func, "Default push_port: ", push_port);
                    }
                }
#endif

                StRef<String> push_username;
                {
                    ConstMemory const opt_name = "push_username";
                    MConfig::Option * const opt = item_section->getOption (opt_name);
                    if (opt && opt->getValue()) {
                        push_username = opt->getValue()->getAsString();
                        logD_ (_func, opt_name, ": ", push_username);
                    } else {
                        push_username = st_grab (new (std::nothrow) String);
                    }
                }

                StRef<String> push_password;
                {
                    ConstMemory const opt_name = "push_password";
                    MConfig::Option * const opt = item_section->getOption (opt_name);
                    if (opt && opt->getValue()) {
                        push_password = opt->getValue()->getAsString();
                        logD_ (_func, opt_name, ": ", push_password);
                    } else {
                        push_password = st_grab (new (std::nothrow) String);
                    }
                }

                if (push_uri) {
                    Ref<PushProtocol> const push_protocol = moment->getPushProtocolForUri (push_uri->mem());
                    if (push_protocol) {
                        push_agent = grab (new (std::nothrow) PushAgent);
                        push_agent->init (stream_name->mem(),
                                          push_protocol,
                                          push_uri      ? push_uri->mem()      : ConstMemory(),
                                          push_username ? push_username->mem() : ConstMemory(),
                                          push_password ? push_password->mem() : ConstMemory());
                    }
                }
            }

            Ref<FetchAgent> fetch_agent;
            {
                Ref<String> fetch_uri;
                {
                    ConstMemory const opt_name = "fetch_uri";
                    MConfig::Option * const opt = item_section->getOption (opt_name);
                    if (opt && opt->getValue()) {
                        fetch_uri = opt->getValue()->getAsString();
                        logD_ (_func, opt_name, ": ", fetch_uri);
                    }
                }

                if (fetch_uri) {
                    Ref<FetchProtocol> const fetch_protocol = moment->getFetchProtocolForUri (fetch_uri->mem());
                    if (fetch_protocol) {
                        fetch_agent = grab (new (std::nothrow) FetchAgent);
                        fetch_agent->init (moment,
                                           fetch_protocol,
                                           stream_name->mem(),
                                           fetch_uri->mem(),
                                           1000 /* reconnect_interval_millisec */ /* TODO Config parameter */);
                    }
                }
            }

            bool no_audio = default_channel_opts->default_item->no_audio;
            {
                ConstMemory const opt_name = "no_audio";
                if (!configSectionGetBoolean (item_section, opt_name, &no_audio, no_audio))
                    return Result::Failure;
                logI_ (_func, "stream ", stream_name->mem(), ": ", opt_name, ": ", no_audio);
            }

            bool no_video = default_channel_opts->default_item->no_video;
            {
                ConstMemory const opt_name = "no_video";
                if (!configSectionGetBoolean (item_section, opt_name, &no_video, no_video))
                    return Result::Failure;
                logI_ (_func, "stream ", stream_name->mem(), ": ", opt_name, ": ", no_video);
            }

            bool force_transcode = default_channel_opts->default_item->force_transcode;
            {
                ConstMemory const opt_name = "force_transcode";
                if (!configSectionGetBoolean (item_section, opt_name, &force_transcode, force_transcode))
                    return Result::Failure;
                logI_ (_func, "stream ", stream_name->mem(), ": ", opt_name, ": ", force_transcode);
            }

            bool force_transcode_audio = default_channel_opts->default_item->force_transcode_audio;
            {
                ConstMemory const opt_name = "force_transcode_audio";
                if (!configSectionGetBoolean (item_section, opt_name, &force_transcode_audio, force_transcode_audio))
                    return Result::Failure;
                logI_ (_func, "stream ", stream_name->mem(), ": ", opt_name, ": ", force_transcode_audio);
            }

            bool force_transcode_video = default_channel_opts->default_item->force_transcode_video;
            {
                ConstMemory const opt_name = "force_transcode_video";
                if (!configSectionGetBoolean (item_section, opt_name, &force_transcode_video, force_transcode_video))
                    return Result::Failure;
                logI_ (_func, "stream ", stream_name->mem(), ": ", opt_name, ": ", force_transcode_video);
            }

            bool aac_perfect_timestamp = default_channel_opts->default_item->aac_perfect_timestamp;
            {
                ConstMemory const opt_name = "aac_perfect_timestamp";
                if (!configSectionGetBoolean (item_section, opt_name, &aac_perfect_timestamp, aac_perfect_timestamp))
                    return Result::Failure;
                logI_ (_func, "stream ", stream_name->mem(), ": ", opt_name, ": ", aac_perfect_timestamp);
            }

            bool sync_to_clock = default_channel_opts->default_item->sync_to_clock;
            {
                ConstMemory const opt_name = "sync_to_clock";
                if (!configSectionGetBoolean (item_section, opt_name, &sync_to_clock, sync_to_clock))
                    return Result::Failure;
                logI_ (_func, "stream ", stream_name->mem(), ": ", opt_name, ": ", sync_to_clock);
            }

            Ref<ChannelOptions> const opts = grab (new (std::nothrow) ChannelOptions);
            {
                *opts = *default_channel_opts;
                opts->channel_name  = st_grab (new (std::nothrow) String (stream_name->mem()));
                opts->channel_title = st_grab (new (std::nothrow) String (channel_title->mem()));
                opts->channel_desc  = st_grab (new (std::nothrow) String (channel_desc->mem()));

                opts->recording = (record_path ? true : false);
                opts->record_path = st_grab (new (std::nothrow) String (record_path ? record_path->mem() : ConstMemory()));

                opts->connect_on_demand = connect_on_demand;
                opts->connect_on_demand_timeout = connect_on_demand_timeout;
            }

            Ref<PlaybackItem> const item = grab (new (std::nothrow) PlaybackItem);
            opts->default_item = item;
            {
                *item = *default_channel_opts->default_item;

                item->no_audio = no_audio;
                item->no_video = no_video;
                item->force_transcode = force_transcode;
                item->force_transcode_audio = force_transcode_audio;
                item->force_transcode_video = force_transcode_video;
                item->aac_perfect_timestamp = aac_perfect_timestamp;
                item->sync_to_clock = sync_to_clock;
            }

            if (!Moment::parseOverlayConfig (item_section, opts))
                return Result::Failure;

	    if (chain && !chain->isNull()) {
                logD_ (_func, "chain, channel \"", opts->channel_name, "\"");

                item->stream_spec = st_grab (new (std::nothrow) String (chain->mem()));
                item->spec_kind = PlaybackItem::SpecKind::Chain;

		createStreamChannel (opts, item, push_agent, fetch_agent);
	    } else
	    if (uri && !uri->isNull()) {
                logD_ (_func, "uri, channel \"", opts->channel_name, "\"");

                item->stream_spec = st_grab (new (std::nothrow) String (uri->mem()));
                item->spec_kind = PlaybackItem::SpecKind::Uri;

		createStreamChannel (opts, item, push_agent, fetch_agent);
	    } else
	    if (playlist && !playlist->isNull()) {
                logD_ (_func, "playlist, channel \"", opts->channel_name, "\"");

		createPlaylistChannel (playlist->mem(),
                                       false /* is_dir */,
                                       false /* dir_re_read */,
                                       opts,
                                       push_agent,
                                       fetch_agent);
            } else
            if (playlist_dir && !playlist_dir->isNull()) {
                logD_ (_func, "playlist dir, channel \"", opts->channel_name, "\"");

                createPlaylistChannel (playlist_dir->mem(),
                                       true /* is_dir */,
                                       playlist_dir_re_read,
                                       opts,
                                       push_agent,
                                       fetch_agent);
	    } else {
		logW_ (_func, "None of chain/uri/playlist specified for stream \"", stream_name, "\"");
		createDummyChannel (stream_name->mem(),
                                    channel_title->mem(),
                                    channel_desc->mem(),
                                    push_agent,
                                    fetch_agent);
	    }

	}
    }

    return Result::Success;
}

Result
MomentFFmpegModule::parseStreams ()
{
    MConfig::Config * const config = moment->getConfig();

    logD_ (_func_);

    MConfig::Section * const ffmpeg_section = config->getSection ("mod_ffmpeg");
    if (!ffmpeg_section)
        return Result::Success;

    MConfig::Section::iterator ffmpeg_section_iter (*ffmpeg_section);
    while (!ffmpeg_section_iter.done()) {
        MConfig::SectionEntry * const ffmpeg_section = ffmpeg_section_iter.next ();
        if (ffmpeg_section->getType() == MConfig::SectionEntry::Type_Section) {
            MConfig::Section * const section = static_cast <MConfig::Section*> (ffmpeg_section);
            if (!equal (section->getName(), "stream"))
                continue;

            ConstMemory stream_name;
            {
                MConfig::Section::attribute_iterator attr_iter (*section);
                if (!attr_iter.done()) {
                    MConfig::Attribute * const attr = attr_iter.next ();
                    if (!attr->hasValue())
                        stream_name = attr->getName();
                }
            }

            if (MConfig::Attribute * const attr = section->getAttribute ("name"))
                stream_name = attr->getValue ();

            if (!stream_name.isNull()) {
                logD_ (_func, "--- STREAM NAME: ", stream_name);
            }
        }
    }

    return Result::Success;
}

void
MomentFFmpegModule::parseRecordingsConfigSection ()
{
    MConfig::Config * const config = moment->getConfig();

    MConfig::Section * const recordings_section = config->getSection ("mod_ffmpeg/recordings");
    if (!recordings_section)
	return;

    MConfig::Section::iter recordings_iter (*recordings_section);
    while (!recordings_section->iter_done (recordings_iter)) {
	MConfig::SectionEntry * const recorder_entry = recordings_section->iter_next (recordings_iter);
	if (recorder_entry->getType() == MConfig::SectionEntry::Type_Section) {
	    MConfig::Section* const recorder_section = static_cast <MConfig::Section*> (recorder_entry);

	    ConstMemory const recorder_name = recorder_section->getName();
	    if (!recorder_name.len())
		logW_ (_func, "Unnamed recorder in section mod_ffmpeg/recordings");

	    Ref<String> channel_name;
	    Ref<String> playlist_filename;
	    {
		int num_set_opts = 0;

		{
		    MConfig::Option * const opt = recorder_section->getOption ("channel");
		    if (opt && opt->getValue()) {
			channel_name = opt->getValue()->getAsString();
			++num_set_opts;
		    }
		}

		{
		    MConfig::Option * const opt = recorder_section->getOption ("playlist");
		    if (opt && opt->getValue()) {
			playlist_filename = opt->getValue()->getAsString();
			++num_set_opts;
		    }
		}

		if (num_set_opts > 1) {
		    logW_ (_func, "Only one of channel/playlist "
			   "should be specified for recorder \"", recorder_name, "\"");
		}
	    }

	    Ref<String> record_path;
	    {
		MConfig::Option * const opt = recorder_section->getOption ("record_path");
		if (opt && opt->getValue())
		    record_path = opt->getValue()->getAsString();
	    }

	    if (channel_name && !channel_name->isNull()) {
		createChannelRecorder (recorder_name,
				       channel_name->mem(),
				       record_path ? record_path->mem() : ConstMemory());
	    } else
	    if (playlist_filename && !playlist_filename->isNull()) {
		createPlaylistRecorder (recorder_name,
					playlist_filename->mem(),
					record_path ? record_path->mem() : ConstMemory());
	    } else {
		logW_ (_func, "None of channel/playlist specified for recorder \"", recorder_name, "\"");
	    }
	}
    }
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

    return ffmpeg_stream;
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

    moment->setMediaSourceProvider (this);


//    moment->getAdminHttpService()->addHttpHandler (
//        CbDesc<HttpService::HttpHandler> (
//            &admin_http_handler, this /* cb_data */, NULL /* coderef_container */),
//        "mod_nvr_admin",
//        true    /* preassembly */,
//        1 << 20 /* 1 Mb */ /* preassembly_limit */,
//        true    /* parse_body_params */);

    // Alias to "moment_ffmpeg"
    moment->getHttpService()->addHttpHandler (
        CbDesc<HttpService::HttpHandler> (
            &http_handler, this /* cb_data */, NULL /* coderef_container */),
        "mod_ffmpeg",
        true /* preassembly */,
        1 << 20 /* 1 Mb */ /* preassembly_limit */,
        true /* parse_body_params */);

//    parseSourcesConfigSection ();
//    parseChainsConfigSection ();

//    if (!parseStreamsConfigSection ())
//        return Result::Failure;

//    if (!parseStreams ())
//        return Result::Failure;

//    parseRecordingsConfigSection ();

    return Result::Success;
}

MomentFFmpegModule::MomentFFmpegModule()
    : moment (NULL),
      timers (NULL),
      page_pool (NULL),
      serve_playlist_json (true)
{
    default_channel_opts = grab (new (std::nothrow) ChannelOptions);
    default_channel_opts->default_item = grab (new (std::nothrow) PlaybackItem);
}

MomentFFmpegModule::~MomentFFmpegModule ()
{
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

