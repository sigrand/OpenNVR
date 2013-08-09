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


#ifndef MOMENT_NVR__CHANNEL_RECORDER__H__
#define MOMENT_NVR__CHANNEL_RECORDER__H__


#include <moment/libmoment.h>

#include <moment-nvr/media_recorder.h>
#include <moment-nvr/nvr_cleaner.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

class ChannelRecorder : public Object
{
private:
    StateMutex mutex;

public:
    enum ChannelResult
    {
        ChannelResult_Success,
        ChannelResult_Failure,
        ChannelResult_ChannelNotFound
    };

private:
    class ChannelEntry : public Object
    {
    public:
        mt_const WeakRef<ChannelRecorder> weak_channel_recorder;

        mt_const CodeDepRef<ServerThreadContext> thread_ctx;
        mt_const ServerThreadContext *recorder_thread_ctx;

        mt_const Ref<Channel> channel;
        mt_const StRef<String> channel_name;

        mt_const Ref<MediaRecorder> media_recorder;
        mt_const Ref<NvrCleaner> nvr_cleaner;

        mt_const GenericStringHash::EntryKey hash_entry_key;

        mt_mutex (ChannelRecorder::mutex) bool valid;
    };

    mt_const Ref<MomentServer>   moment;
    mt_const Ref<ChannelManager> channel_manager;
    mt_const Ref<Vfs>            vfs;
    mt_const Ref<NamingScheme>   naming_scheme;

    mt_const Time max_age_sec;
    mt_const Time clean_interval_sec;

    typedef StringHash< Ref<ChannelEntry> > ChannelHash;
    mt_mutex (mutex) ChannelHash channel_hash;

  mt_iface (Channel::ChannelEvents)
    static Channel::ChannelEvents const channel_events;
    static StRef<String> recording_state_dir;

    static void channelStartItem      (VideoStream *stream,
                                       bool         stream_changed,
                                       void        *_channel_entry);

    static void channelStopItem       (VideoStream *stream,
                                       bool         stream_changed,
                                       void        *_channel_entry);

    static void channelNewVideoStream (VideoStream *stream,
                                       void        *_channel_entry);

    static void channelDestroyed      (void *_channel_entry);
  mt_iface_end

  mt_iface (ChannelManager::Events)
    static ChannelManager::Events const channel_manager_events;

    static void channelCreated (ChannelManager::ChannelInfo * mt_nonnull channel_info,
                                void *_self);
  mt_iface_end

    void doCreateChannel (ChannelManager::ChannelInfo * mt_nonnull channel_info);

    mt_mutex (mutex) void doDestroyChannel (ChannelEntry * mt_nonnull channel_entry);

public:
    struct ChannelState
    {
        bool recording;
    };

    ChannelResult getChannelState (ConstMemory   channel_anme,
                                   ChannelState * mt_nonnull ret_state);

    ChannelResult setRecording (ConstMemory channel_name,
                                bool        set_on);

    mt_const void init (MomentServer * mt_nonnull moment,
                        Vfs          * mt_nonnull vfs,
                        NamingScheme * mt_nonnull naming_scheme,
                        Time          max_age_sec,
                        Time          clean_interval_sec,
                        String       * recording_state_dir);

     ChannelRecorder ();
    ~ChannelRecorder ();
};

}


#endif /* MOMENT_NVR__CHANNEL_RECORDER__H__ */

