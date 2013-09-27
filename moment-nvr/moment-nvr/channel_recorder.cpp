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

#include <stdio.h>

#include <moment-nvr/inc.h>

#include <moment-nvr/channel_recorder.h>


using namespace M;
using namespace Moment;

namespace MomentNvr {

StRef<String> ChannelRecorder::recording_state_dir;

Channel::ChannelEvents const ChannelRecorder::channel_events = {
    channelStartItem,
    channelStopItem,
    channelNewVideoStream,
    channelDestroyed
};

void
ChannelRecorder::channelStartItem (VideoStream * const stream,
                                   bool          const stream_changed,
                                   void        * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
    if (stream_changed) {
        channel_entry->media_recorder->setVideoStream (stream);

        bool bSourceNotExist = true;
        if(!recording_state_dir->isNullString())
        {
            StRef<String> const path = st_makeString (recording_state_dir->mem(), "/", channel_entry->channel_name->mem());
            if (FILE *file = fopen(path->cstr(), "r")) {
                fclose(file);
                bSourceNotExist = false;
            }
        }
        if(bSourceNotExist)
            channel_entry->media_recorder->startRecording ();
    }
}

void
ChannelRecorder::channelStopItem (VideoStream * const stream,
                                  bool          const stream_changed,
                                  void        * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);

    if (stream_changed) {
        channel_entry->media_recorder->setVideoStream (stream);

        bool bSourceNotExist = true;
        if(!recording_state_dir->isNullString())
        {
            StRef<String> const path = st_makeString (recording_state_dir->mem(), "/", channel_entry->channel_name->mem());
            if (FILE *file = fopen(path->cstr(), "r")) {
                fclose(file);
                bSourceNotExist = false;
            }
        }
        if(bSourceNotExist)
            channel_entry->media_recorder->startRecording ();
    }
}

void
ChannelRecorder::channelNewVideoStream (VideoStream * const stream,
                                        void        * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
    channel_entry->media_recorder->setVideoStream (stream);

    bool bSourceNotExist = true;
    if(!recording_state_dir->isNullString())
    {
        StRef<String> const path = st_makeString (recording_state_dir->mem(), "/", channel_entry->channel_name->mem());
        if (FILE *file = fopen(path->cstr(), "r")) {
            fclose(file);
            bSourceNotExist = false;
        }
    }
    if(bSourceNotExist)
        channel_entry->media_recorder->startRecording ();
}

void
ChannelRecorder::channelDestroyed (void * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
    Ref<ChannelRecorder> const self = channel_entry->weak_channel_recorder.getRef ();
    if (!self)
        return;

    self->mutex.lock ();
    self->doDestroyChannel (channel_entry);
    self->mutex.unlock ();
}

ChannelManager::Events const ChannelRecorder::channel_manager_events = {
    channelCreated
};

void
ChannelRecorder::channelCreated (ChannelManager::ChannelInfo * const mt_nonnull channel_info,
                                 void * const _self)
{
    ChannelRecorder * const self = static_cast <ChannelRecorder*> (_self);

    logD_ (_func, "channel_name: ", channel_info->channel_name);
    self->doCreateChannel (channel_info);
}

// May be called with 'channel_manager' locked.
void
ChannelRecorder::doCreateChannel (ChannelManager::ChannelInfo * const mt_nonnull channel_info)
{
    logD_ (_func, "channel_name: ", channel_info->channel_name);

    Channel * const channel = channel_info->channel;

    Ref<ChannelEntry> const channel_entry = grab (new (std::nothrow) ChannelEntry);
    channel_entry->valid = true;
    channel_entry->weak_channel_recorder = this;
    // TODO release thread_ctx
    channel_entry->thread_ctx = moment->getRecorderThreadPool()->grabThreadContext (ConstMemory() /* TODO filename */);
    if (channel_entry->thread_ctx) {
        channel_entry->recorder_thread_ctx = channel_entry->thread_ctx;
    } else {
        logE_ (_func, "Couldn't get recorder thread context: ", exc->toString());
        channel_entry->recorder_thread_ctx = NULL;
        channel_entry->thread_ctx = moment->getServerApp()->getServerContext()->getMainThreadContext();
    }

    Uint64 prewrite_sec = 5;
    {
        ConstMemory const opt_name = "prewrite";
        MConfig::GetResult const res =
                channel_info->config->getUint64_default (opt_name, &prewrite_sec, prewrite_sec);
        if (!res) {
            logE_ (_func, "Invalid value for config option ", opt_name, " "
                   "(channel \"", channel_info->channel_name, "\"): ",
                   channel_info->config->getString (opt_name));
        } else {
            logI_ (_func, "channel \"", channel_info->channel_name, "\", ", opt_name, ": ", prewrite_sec);
        }
    }

    Uint64 postwrite_sec = 5;
    {
        ConstMemory const opt_name = "postwrite";
        MConfig::GetResult const res =
                channel_info->config->getUint64_default (opt_name, &postwrite_sec, postwrite_sec);
        if (!res) {
            logE_ (_func, "Invalid value for config option ", opt_name, " "
                   "(channel \"", channel_info->channel_name, "\"): ",
                   channel_info->config->getString (opt_name));
        } else {
            logI_ (_func, "channel \"", channel_info->channel_name, "\", ", opt_name, ": ", postwrite_sec);
        }
    }

    Uint64 prewrite_num_frames = prewrite_sec * 200;
    {
        ConstMemory const opt_name = "prewrite_frames";
        MConfig::GetResult const res =
            channel_info->config->getUint64_default (opt_name, &prewrite_num_frames, prewrite_num_frames);
        if (!res) {
            logE_ (_func, "Invalid value for config option ", opt_name, " "
                   "(channel \"", channel_info->channel_name, "\"): ",
                   channel_info->config->getString (opt_name));
        } else {
            logI_ (_func, "channel \"", channel_info->channel_name, "\", ", opt_name, ": ", prewrite_num_frames);
        }
    }

    Uint64 postwrite_num_frames = postwrite_sec * 200;
    {
        ConstMemory const opt_name = "postwrite_frames";
        MConfig::GetResult const res =
            channel_info->config->getUint64_default (opt_name, &postwrite_num_frames, postwrite_num_frames);
        if (!res) {
            logE_ (_func, "Invalid value for config option ", opt_name, " "
                   "(channel \"", channel_info->channel_name, "\"): ",
                   channel_info->config->getString (opt_name));
        } else {
            logI_ (_func, "channel \"", channel_info->channel_name, "\", ", opt_name, ": ", postwrite_num_frames);
        }
    }

    channel_entry->channel = channel;
    channel_entry->channel_name = st_grab (new (std::nothrow) String (channel_info->channel_name));

    channel_entry->media_recorder = grab (new (std::nothrow) MediaRecorder);
    channel_entry->media_recorder->init (moment->getPagePool (),
                                         channel_entry->thread_ctx,
                                         vfs,
                                         naming_scheme,
                                         channel_info->channel_name,
                                         prewrite_sec * 1000000000,
                                         prewrite_num_frames,
                                         postwrite_sec * 1000000000,
                                         postwrite_num_frames);

    channel_entry->nvr_cleaner = grab (new (std::nothrow) NvrCleaner);
    channel_entry->nvr_cleaner->init (moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers(),
                                      vfs,
                                      channel_info->channel_name,
                                      max_age_sec,
                                      clean_interval_sec);

    mutex.lock ();
#warning TODO Deal with duplicate channel names.

    channel_entry->hash_entry_key = channel_hash.add (channel_info->channel_name, channel_entry);
    mutex.unlock ();

    {
        channel->channelLock ();
        if (channel->isDestroyed_unlocked()) {
            channel->channelUnlock ();

            mutex.lock ();
            doDestroyChannel (channel_entry);
            mutex.unlock ();
        } else {
            channel_entry->media_recorder->setVideoStream (channel->getVideoStream_unlocked());
            channel->getEventInformer()->subscribe_unlocked (
                    CbDesc<Channel::ChannelEvents> (&channel_events, channel_entry, channel_entry));
            channel->channelUnlock ();

            bool bSourceNotExist = true;
            if(!recording_state_dir->isNullString())
            {
                StRef<String> const path = st_makeString (recording_state_dir->mem(), "/", channel_entry->channel_name->mem());
                if (FILE *file = fopen(path->cstr(), "r")) {
                    fclose(file);
                    bSourceNotExist = false;
                }
            }
            if(bSourceNotExist)
                channel_entry->media_recorder->startRecording ();
        }
    }
}

mt_mutex (mutex) void
ChannelRecorder::doDestroyChannel (ChannelEntry * const mt_nonnull channel_entry)
{
    logD_ (_func, "channel: ", channel_entry->channel_name);

    if (!channel_entry->valid) {
        return;
    }
    channel_entry->valid = false;

    if (channel_entry->recorder_thread_ctx) {
        moment->getRecorderThreadPool()->releaseThreadContext (channel_entry->recorder_thread_ctx);
        channel_entry->recorder_thread_ctx = NULL;
    }

    channel_hash.remove (channel_entry->hash_entry_key);
    // 'channel_entry' is not valid anymore.
}

ChannelRecorder::ChannelResult
ChannelRecorder::getChannelState (ConstMemory    const channel_name,
                                  ChannelState * const mt_nonnull ret_state)
{
    mutex.lock ();
    ChannelHash::EntryKey const channel_entry_key = channel_hash.lookup (channel_name);
    if (!channel_entry_key) {
        mutex.unlock ();
        return ChannelResult_ChannelNotFound;
    }
    Ref<ChannelEntry> const &channel_entry = channel_entry_key.getData();

    ret_state->recording = channel_entry->media_recorder->isRecording();

    mutex.unlock ();
    return ChannelResult_Success;
}

ChannelRecorder::ChannelResult
ChannelRecorder::setRecording (ConstMemory const channel_name,
                               bool        const set_on)
{
    mutex.lock ();
    ChannelHash::EntryKey const channel_entry_key = channel_hash.lookup (channel_name);
    if (!channel_entry_key) {
        mutex.unlock ();
        return ChannelResult_ChannelNotFound;
    }
    Ref<ChannelEntry> const &channel_entry = channel_entry_key.getData();

    if (set_on)
        channel_entry->media_recorder->startRecording ();
    else
        channel_entry->media_recorder->stopRecording ();

    mutex.unlock ();
    return ChannelResult_Success;
}

mt_const void
ChannelRecorder::init (MomentServer * const mt_nonnull moment,
                       Vfs          * const mt_nonnull vfs,
                       NamingScheme * const mt_nonnull naming_scheme,
                       Time           const max_age_sec,
                       Time           const clean_interval_sec,
                       String       * const recording_state_dir)
{
    logD_ (_func_);

    this->moment = moment;
    this->vfs = vfs;
    this->naming_scheme = naming_scheme;
    this->max_age_sec = max_age_sec;
    this->clean_interval_sec = clean_interval_sec;
    this->recording_state_dir = st_makeString(recording_state_dir->mem());

    Ref<ChannelManager> const channel_manager = moment->getChannelManager();

    MOMENT_NVR__CHANNEL_RECORDER

    {
        channel_manager->channelManagerLock ();

        ChannelManager::ChannelInfo channel_info;
        ChannelManager::channel_iterator iter (channel_manager);
        while (!iter.done()) {
            logD_ (_func, "iteration");
            iter.next (&channel_info);
            logD_ (_func, "channel_name: ", channel_info.channel_name);
            doCreateChannel (&channel_info);
        }

        channel_manager->getEventInformer()->subscribe_unlocked (
                CbDesc<ChannelManager::Events> (&channel_manager_events, this, this));

        channel_manager->channelManagerUnlock ();
    }
}

ChannelRecorder::ChannelRecorder ()
    : max_age_sec (3600),
      clean_interval_sec (5)
{
}

ChannelRecorder::~ChannelRecorder ()
{
    mutex.lock ();

    ChannelHash::iterator iter (channel_hash);
    while (!iter.done()) {
        Ref<ChannelEntry> * const channel_entry = iter.next ();
        doDestroyChannel (*channel_entry);
    }
    assert (channel_hash.isEmpty());

    mutex.unlock ();
}

}

