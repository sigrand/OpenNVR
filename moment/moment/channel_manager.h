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


#ifndef MOMENT__CHANNEL_MANAGER__H__
#define MOMENT__CHANNEL_MANAGER__H__


#include <libmary/libmary.h>
#include <moment/moment_server.h>


namespace Moment {

using namespace M;

class Channel;

class ChannelManager : public Object
{
private:
    StateMutex mutex;

public:
    struct ChannelInfo
    {
        Channel         *channel;
        ConstMemory      channel_name;
        MConfig::Config *config;
    };


  // _________________________________ Events __________________________________

public:
    struct Events
    {
        void (*channelCreated) (ChannelInfo * mt_nonnull channel_info,
                                void        *cb_data);
    };

private:
    Informer_<Events> event_informer;

    static void informChannelCreated (Events * const events,
                                      void   * const cb_data,
                                      void   * const _channel_info)
    {
        if (events->channelCreated) {
            ChannelInfo * const channel_info = static_cast <ChannelInfo*> (_channel_info);
            events->channelCreated (channel_info, cb_data);
        }
    }

    void fireChannelCreated (ChannelInfo * const mt_nonnull channel_info)
        { event_informer.informAll (informChannelCreated, channel_info); }

public:
    Informer_<Events>* getEventInformer () { return &event_informer; }

  // ___________________________________________________________________________


private:
    class ConfigItem : public StReferenced
    {
    public:
        Ref<MConfig::Config> config;
        Ref<Channel> channel;
        StRef<String> channel_name;
        StRef<String> channel_title;
    };

    mt_const Ref<MomentServer> moment;
    mt_const DataDepRef<PagePool> page_pool;

    mt_const StRef<String> confd_dirname;
    mt_const bool          serve_playlist_json;
    mt_const StRef<String> playlist_json_protocol;

    typedef StringHash< StRef<ConfigItem> > ItemHash;
    mt_mutex (mutex) ItemHash item_hash;

    mt_mutex (mutex) Ref<ChannelOptions> default_channel_opts;

  mt_iface (MomentServer::HttpRequestHandler)
      static MomentServer::HttpRequestHandler admin_http_handler;

      static MomentServer::HttpRequestResult adminHttpRequest (HttpRequest * mt_nonnull req,
                                                               Sender      * mt_nonnull conn_sender,
                                                               Memory       msg_body,
                                                               void        *cb_data);
  mt_iface_end

  mt_iface (MomentServer::HttpRequestHandler)
      static MomentServer::HttpRequestHandler server_http_handler;

      static MomentServer::HttpRequestResult serverHttpRequest (HttpRequest * mt_nonnull req,
                                                                Sender      * mt_nonnull conn_sender,
                                                                Memory       msg_body,
                                                                void        *cb_data);
  mt_iface_end


  // _____________________ Channel creation notification ______________________

    struct ChannelCreationMessage : public IntrusiveListElement<>
    {
        Ref<Channel>         channel;
        StRef<String>        channel_name;
        Ref<MConfig::Config> config;
    };

    typedef IntrusiveList< ChannelCreationMessage > ChannelCreationMessageList;

    mt_mutex (mutex) ChannelCreationMessageList channel_creation_messages;

    DeferredProcessor::Task channel_created_task;
    DeferredProcessor::Registration deferred_reg;

    void notifyChannelCreated (ChannelInfo * mt_nonnull channel_info);

    static bool channelCreatedTask (void *_self);

  // ___________________________________________________________________________


public:
    void channelManagerLock   () { mutex.lock (); }
    void channelManagerUnlock () { mutex.unlock (); }

    Result loadConfigFull ();

    Result loadConfigItem (ConstMemory item_name,
                           ConstMemory item_path);

    void setDefaultChannelOptions (ChannelOptions * mt_nonnull channel_opts);


  // ____________________________ channel_iterator _____________________________

    class channel_iterator
    {
    private:
        ItemHash::iterator iter;
    public:
        channel_iterator (ChannelManager * const mt_nonnull cm) : iter (cm->item_hash) {}
        channel_iterator () {};

        bool operator == (channel_iterator const &iter) const { return this->iter == iter.iter; }
        bool operator != (channel_iterator const &iter) const { return this->iter != iter.iter; }

        bool done () const { return iter.done(); }

        void next (ChannelInfo * const mt_nonnull ret_info)
        {
            ConfigItem * const item = iter.next ()->ptr();
            ret_info->channel      = item->channel;
            ret_info->channel_name = item->channel_name->mem();
            ret_info->config       = item->config;
        }
    };

  // ___________________________________________________________________________


    mt_const void init (MomentServer * mt_nonnull moment,
                        PagePool     * mt_nonnull page_pool);

     ChannelManager ();
    ~ChannelManager ();
};

}


#include <moment/channel.h>


#endif /* MOMENT__CHANNEL_MANAGER__H__ */

