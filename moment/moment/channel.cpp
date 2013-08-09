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


#include <moment/channel.h>


using namespace M;

namespace Moment {

static LogGroup libMary_logGroup_ctl ("moment.channel", LogLevel::I);

Playback::Frontend Channel::playback_frontend = {
    startPlaybackItem,
    stopPlaybackItem
};

void
Channel::startPlaybackItem (Playlist::Item          * const item,
			    Time                      const seek,
			    Playback::AdvanceTicket * const advance_ticket,
			    void                    * const _self)
{
//    logD_ (_func, "seek: ", seek);

    Channel * const self = static_cast <Channel*> (_self);

    self->beginVideoStream (item->playback_item,
                            advance_ticket /* stream_ticket */,
                            advance_ticket /* stream_ticket_ref */,
                            seek);
}

void
Channel::stopPlaybackItem (void * const _self)
{
//    logD_ (_func_);

    Channel * const self = static_cast <Channel*> (_self);
    self->endVideoStream ();
}

VideoStream::EventHandler const Channel::stream_event_handler = {
    NULL /* audioMessage */,
    NULL /* videoMessage */,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    numWatchersChanged
};

void
Channel::numWatchersChanged (Count   const num_watchers,
                             void  * const _stream_data)
{
    logD_ (_func, num_watchers);

    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    self->mutex.lock ();
#warning Revise usage of stream_closed
    if (stream_data != self->cur_stream_data /* ||
	stream_data->stream_closed */)
    {
	self->mutex.unlock ();
	return;
    }

    stream_data->num_watchers = num_watchers;

    if (num_watchers == 0) {
#warning TODO    if (!channel_opts->connect_on_demand || stream_stopped)
        if (!self->connect_on_demand_timer) {
            logD_ (_func, "starting timer, timeout: ", self->channel_opts->connect_on_demand_timeout);
            self->connect_on_demand_timer = self->timers->addTimer (
                    CbDesc<Timers::TimerCallback> (connectOnDemandTimerTick,
                                                   stream_data /* cb_data */,
                                                   self        /* coderef_container */,
                                                   stream_data /* ref_data */),
                    self->channel_opts->connect_on_demand_timeout,
                    false /* periodical */);
        }
    } else {
        if (self->connect_on_demand_timer) {
            self->timers->deleteTimer (self->connect_on_demand_timer);
            self->connect_on_demand_timer = NULL;
        }

        logD_ (_func, "media_source: 0x", fmt_hex, (UintPtr) self->media_source.ptr(), ", "
               "stream_stopped: ", self->stream_stopped);
        if (!self->media_source
            && !self->stream_stopped)
        {
            logD_ (_func, "connecting on demand");
            mt_unlocks (mutex) self->doRestartStream (true /* from_ondemand_reconnect */);
            return;
        }
    }

    self->mutex.unlock ();
}

void
Channel::connectOnDemandTimerTick (void * const _stream_data)
{
    logD_ (_func_);

    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    Ref<VideoStream> old_stream;

    self->mutex.lock ();
    if (stream_data != self->cur_stream_data /* ||
	stream_data->stream_closed */)
    {
	self->mutex.unlock ();
	return;
    }

    if (stream_data->num_watchers == 0) {
        logD_ (_func, "disconnecting on timeout");
        self->closeStream (&old_stream);
    }
    self->mutex.unlock ();

    if (old_stream)
        old_stream->close ();
}

mt_mutex (mutex) void
Channel::beginConnectOnDemand (bool const start_timer)
{
    assert (video_stream);

    if (!channel_opts->connect_on_demand || stream_stopped)
        return;

    video_stream->lock ();

    if (start_timer
        && video_stream->getNumWatchers_unlocked() == 0)
    {
        logD_ (_func, "starting timer, timeout: ", channel_opts->connect_on_demand_timeout);
        connect_on_demand_timer = timers->addTimer (
                CbDesc<Timers::TimerCallback> (connectOnDemandTimerTick,
                                               cur_stream_data /* cb_data */,
                                               this        /* coderef_container */,
                                               cur_stream_data /* ref_data */),
                channel_opts->connect_on_demand_timeout,
                false /* periodical */);
    }

    video_stream_events_sbn = video_stream->getEventInformer()->subscribe_unlocked (
            CbDesc<VideoStream::EventHandler> (
                    &stream_event_handler,
                    cur_stream_data /* cb_data */,
                    this        /* coderef_container */,
                    cur_stream_data /* ref_data */));

    video_stream->unlock ();
}

void
Channel::setStreamParameters (VideoStream * const mt_nonnull video_stream)
{
    Ref<StreamParameters> const stream_params = grab (new (std::nothrow) StreamParameters);
    if (cur_item->no_audio)
        stream_params->setParam ("no_audio", "true");
    if (cur_item->no_video)
        stream_params->setParam ("no_video", "true");

    video_stream->setStreamParameters (stream_params);
}

mt_mutex (mutex) void
Channel::createStream (Time const initial_seek)
{
    logD (ctl, _this_func_);

/* closeStream() is always called before createStream(), so this is unnecessary.
 *
    if (gst_stream) {
	gst_stream->releasePipeline ();
	gst_stream = NULL;
    }
 */

    stream_stopped = false;

    got_video = false;

    if (!cur_stream_data) {
        Ref<StreamData> const stream_data = grab (new (std::nothrow) StreamData (
                this, stream_ticket, stream_ticket_ref.ptr()));
        cur_stream_data = stream_data;
    }

    if (video_stream && video_stream_events_sbn) {
        video_stream->getEventInformer()->unsubscribe (video_stream_events_sbn);
        video_stream_events_sbn = NULL;
    }

    if (!video_stream) {
	video_stream = grab (new (std::nothrow) VideoStream);
        setStreamParameters (video_stream);

	logD_ (_func, "Calling moment->addVideoStream, stream_name: ", channel_opts->channel_name->mem());
	video_stream_key = moment->addVideoStream (video_stream, channel_opts->channel_name->mem());
    }

    Ref<VideoStream> bind_stream = video_stream;
    if (channel_opts->continuous_playback) {
        bind_stream = grab (new (std::nothrow) VideoStream);
        video_stream->bindToStream (bind_stream, bind_stream, true, true);
    }

    beginConnectOnDemand (true /* start_timer */);

    if (stream_start_time == 0)
	stream_start_time = getTime();

    media_source = moment->createMediaSource (
                           CbDesc<MediaSource::Frontend> (
                                   &media_source_frontend,
                                   cur_stream_data /* cb_data */,
                                   this            /* coderef_container */,
                                   cur_stream_data /* ref_data */),
                           timers,
                           deferred_processor,
                           page_pool,
                           bind_stream,
                           moment->getMixVideoStream(),
                           initial_seek,
                           channel_opts,
                           cur_item);
    if (media_source) {
	media_source->ref ();
	GThread * const thread = g_thread_create (
#warning Not joinable?
		streamThreadFunc, media_source, FALSE /* joinable */, NULL /* error */);
	if (thread == NULL) {
	    logE_ (_func, "g_thread_create() failed");
	    media_source->unref ();
	}
    } else {
#warning Handle !media_source case
    }
}

gpointer
Channel::streamThreadFunc (gpointer const _media_source)
{
    MediaSource * const media_source = static_cast <MediaSource*> (_media_source);

    logD (ctl, _func_);

    updateTime ();
    media_source->createPipeline ();

    media_source->unref ();
    return (gpointer) 0;
}

mt_mutex (mutex) void
Channel::closeStream (Ref<VideoStream> * const mt_nonnull ret_old_stream)
{
    logD (ctl, _this_func_);

    got_video = false;

    if (connect_on_demand_timer) {
        timers->deleteTimer (connect_on_demand_timer);
        connect_on_demand_timer = NULL;
    }

    if (media_source) {
	{
	    MediaSource::TrafficStats traffic_stats;
	    media_source->getTrafficStats (&traffic_stats);

	    rx_bytes_accum += traffic_stats.rx_bytes;
	    rx_audio_bytes_accum += traffic_stats.rx_audio_bytes;
	    rx_video_bytes_accum += traffic_stats.rx_video_bytes;
	}

	media_source->releasePipeline ();
	media_source = NULL;
    }
    cur_stream_data = NULL;

//    logD_ (_func, "video_stream: 0x", fmt_hex, (UintPtr) video_stream.ptr(), ", "
//           "keep_video_stream: ", channel_opts->keep_video_stream, ", "
//           "continuous_playback: ", channel_opts->continuous_playback);
    if (video_stream) {
        if (   !channel_opts->keep_video_stream
            && !channel_opts->continuous_playback)
        {
            // TODO moment->replaceVideoStream() to swap video streams atomically
            moment->removeVideoStream (video_stream_key);
            *ret_old_stream = video_stream;

            if (video_stream_events_sbn) {
                video_stream->getEventInformer()->unsubscribe (video_stream_events_sbn);
                video_stream_events_sbn = NULL;
            }
            video_stream = NULL;

            {
                assert (!cur_stream_data);

                Ref<StreamData> const stream_data = grab (new (std::nothrow) StreamData (
                        this, stream_ticket, stream_ticket_ref.ptr()));
                cur_stream_data = stream_data;
            }

            video_stream = grab (new (std::nothrow) VideoStream);
            setStreamParameters (video_stream);

//            logD_ (_func, "calling moment->addVideoStream, stream_name: ", channel_opts->channel_name->mem());
            video_stream_key = moment->addVideoStream (video_stream, channel_opts->channel_name->mem());

            // Note: This is correct and essential for connect_on_demand.
            beginConnectOnDemand (false /* start_timer */);
        }
    }

//    logD_ (_func, "done");
}

mt_unlocks (mutex) void
Channel::doRestartStream (bool const from_ondemand_reconnect)
{
    logD (ctl, _this_func_);

    Ref<VideoStream> old_stream;
    bool new_video_stream = false;
#warning 'new_video_stream' setting is likely incorrect.
    if (media_source && !from_ondemand_reconnect) {
        closeStream (&old_stream);
        new_video_stream = true;
    }

    // TODO FIXME Set correct initial seek
    createStream (0 /* initial_seek */);

    Ref<VideoStream> const new_stream = video_stream;
    mutex.unlock ();

    if (new_video_stream)
        fireNewVideoStream (new_stream);

    if (old_stream)
        old_stream->close ();
}

MediaSource::Frontend Channel::media_source_frontend = {
    streamError,
    streamEos,
    noVideo,
    gotVideo
};

void
Channel::streamError (void * const _stream_data)
{
    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    logD (ctl, _self_func_);

    self->mutex.lock ();

    stream_data->stream_closed = true;
    if (stream_data != self->cur_stream_data) {
	self->mutex.unlock ();
	return;
    }

    VirtRef const tmp_stream_ticket_ref = self->stream_ticket_ref;
    void * const tmp_stream_ticket = self->stream_ticket;

    self->mutex.unlock ();

//    if (self->frontend)
//	self->frontend.call (self->frontend->error, tmp_stream_ticket);
}

void
Channel::streamEos (void * const _stream_data)
{
    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    logD (ctl, _self_func_);

    self->mutex.lock ();

    stream_data->stream_closed = true;
    if (stream_data != self->cur_stream_data) {
	self->mutex.unlock ();
	return;
    }

    VirtRef const tmp_stream_ticket_ref = self->stream_ticket_ref;
    void * const tmp_stream_ticket = self->stream_ticket;

    self->mutex.unlock ();

    Playback::AdvanceTicket * const advance_ticket =
            static_cast <Playback::AdvanceTicket*> (tmp_stream_ticket);
    self->playback.advance (advance_ticket);
}

void
Channel::noVideo (void * const _stream_data)
{
    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    self->mutex.lock ();
    if (stream_data != self->cur_stream_data ||
	stream_data->stream_closed)
    {
	self->mutex.unlock ();
	return;
    }

    logW_ (_func, "channel \"", self->channel_opts->channel_name, "\" (\"", self->channel_opts->channel_title, "\"): "
           "no video, restarting stream");
    mt_unlocks (mutex) self->doRestartStream ();
}

void
Channel::gotVideo (void * const _stream_data)
{
    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    logD (ctl, _self_func_);

    self->mutex.lock ();
    if (stream_data != self->cur_stream_data ||
	stream_data->stream_closed)
    {
	self->mutex.unlock ();
	return;
    }

    self->got_video = true;
    self->mutex.unlock ();
}

void
Channel::beginVideoStream (PlaybackItem   * const mt_nonnull item,
                           void           * const stream_ticket,
                           VirtReferenced * const stream_ticket_ref,
                           Time             const seek)
{
    if (logLevelOn (ctl, LogLevel::Debug)) {
        logD (ctl, _this_func_);
        item->dump ();
    }

    Ref<VideoStream> old_stream;

    mutex.lock ();

    if (media_source)
	closeStream (&old_stream);

    this->cur_item = item;

    this->stream_ticket = stream_ticket;
    this->stream_ticket_ref = stream_ticket_ref;

    createStream (seek);

    Ref<VideoStream> const new_stream = video_stream;
    mutex.unlock ();

    if (old_stream)
        old_stream->close ();

//    logD_ (_func, "firing startItem");
    fireStartItem (new_stream, (old_stream != NULL) /* stream_changed */);
}

void
Channel::endVideoStream ()
{
    logD (ctl, _this_func_);

    Ref<VideoStream> old_stream;

    mutex.lock ();

    stream_stopped = true;

    if (media_source)
	closeStream (&old_stream);

    Ref<VideoStream> const new_stream = video_stream;
    mutex.unlock ();

    if (old_stream)
        old_stream->close ();

    fireStopItem (new_stream, (old_stream != NULL) /* stream_changed */);
}

void
Channel::restartStream ()
{
    logD (ctl, _this_func_);

    mutex.lock ();
    mt_unlocks (mutex) doRestartStream ();
}

bool
Channel::isSourceOnline ()
{
    mutex.lock ();
    bool const res = got_video;
    mutex.unlock ();
    return res;
}

void
Channel::getTrafficStats (TrafficStats * const ret_traffic_stats)
{
    mutex.lock ();

    MediaSource::TrafficStats stream_tstat;
    if (media_source)
	media_source->getTrafficStats (&stream_tstat);
    else
	stream_tstat.reset ();

    ret_traffic_stats->rx_bytes = rx_bytes_accum + stream_tstat.rx_bytes;
    ret_traffic_stats->rx_audio_bytes = rx_audio_bytes_accum + stream_tstat.rx_audio_bytes;
    ret_traffic_stats->rx_video_bytes = rx_video_bytes_accum + stream_tstat.rx_video_bytes;
    {
	Time const cur_time = getTime();
	if (cur_time > stream_start_time)
	    ret_traffic_stats->time_elapsed = cur_time - stream_start_time;
	else
	    ret_traffic_stats->time_elapsed = 0;
    }

    mutex.unlock ();
}

void
Channel::resetTrafficStats ()
{
    mutex.lock ();

    if (media_source)
	media_source->resetTrafficStats ();

    rx_bytes_accum = 0;
    rx_audio_bytes_accum = 0;
    rx_video_bytes_accum = 0;

    stream_start_time = getTime();

    mutex.unlock ();
}

mt_const void
Channel::init (MomentServer   * const mt_nonnull moment,
               ChannelOptions * const mt_nonnull channel_opts)
{
    this->channel_opts = channel_opts;

    this->moment = moment;
    this->timers = moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers();
    this->deferred_processor = moment->getServerApp()->getServerContext()->getMainThreadContext()->getDeferredProcessor();
    this->page_pool = moment->getPagePool();

    playback.init (CbDesc<Playback::Frontend> (&playback_frontend, this, this),
                   moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers(),
                   channel_opts->min_playlist_duration_sec);
}

Channel::Channel ()
    : event_informer (this /* coderef_container */, &mutex),
      playback       (this /* coderef_container */),
        
      moment             (this /* coderef_container */),
      timers             (this /* coderef_container */),
      deferred_processor (this /* coderef_container */),
      page_pool          (this /* coderef_container */),

      destroyed (false),

      stream_ticket (NULL),

      stream_stopped (false),
      got_video (false),

      stream_start_time (0),

      connect_on_demand_timer (NULL),

      rx_bytes_accum (0),
      rx_audio_bytes_accum (0),
      rx_video_bytes_accum (0)
{
    logD (ctl, _this_func_);
}

void
Channel::doDestroy (bool const from_dtor)
{
    mutex.lock ();
    destroyed = true;

    mt_unlocks_locks (mutex) fireDestroyed_unlocked ();

    stream_stopped = true;

    // If we're in a dtor, then timers and subscription keys are invalid
    // at this point, since deletion callbacks have already been called.
    if (!from_dtor) {
        if (connect_on_demand_timer)
            timers->deleteTimer (connect_on_demand_timer);

        if (media_source)
            media_source->releasePipeline ();

        if (video_stream_events_sbn)
            video_stream->getEventInformer()->unsubscribe (video_stream_events_sbn);
    }

    media_source = NULL;
    cur_stream_data = NULL;
    video_stream_events_sbn = NULL;
    connect_on_demand_timer = NULL;

    Ref<VideoStream> old_stream = video_stream;
    if (video_stream) {
        moment->removeVideoStream (video_stream_key);
        video_stream = NULL;
    }

    mutex.unlock ();

    if (old_stream)
        old_stream->close ();
}

void
Channel::destroy ()
{
    doDestroy (false /* from_dtor */);
}

Channel::~Channel ()
{
    logD (ctl, _this_func_);

    doDestroy (true /* from_dtor */);
}

}

