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
#include <fstream>

#include <libmary/types.h>
#include <cctype>

#include <mconfig/mconfig.h>
#include <moment/libmoment.h>

#include <moment/channel_manager.h>

#include <json/json.h>

using namespace M;

namespace Moment {


bool
ChannelManager::adminHttpRequest (HTTPServerRequest &req, HTTPServerResponse &resp, void * _self)
{
    ChannelManager * const self = static_cast <ChannelManager*> (_self);

    logD_ (_func_);

    URI uri(req.getURI());
    std::vector < std::string > segments;
    uri.getPathSegments(segments);

    if(segments.size() >= 2 && segments[1].compare("reload_channel") == 0)
    {
        logD_ (_func, "reload_channel");

        HTMLForm form( req );

        NameValueCollection::ConstIterator item_name_iter = form.find("conf_file");
        std::string item_name =(item_name_iter != form.end()) ? item_name_iter->second : "";

        if (item_name.size() == 0)
        {
            resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            std::ostream& out = resp.send();
            out << "400 Bad Request: no conf_file parameter";
            out.flush();

            logA_ ("moment__channel_manager 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());

            goto _return;
        }

        ConstMemory const dir_name = self->confd_dirname->mem();
        StRef<String> const path = st_makeString (dir_name, "/", item_name.c_str());

        ConstMemory item_name_mem = ConstMemory(item_name.c_str(), item_name.size());
        if (!self->loadConfigItem (item_name_mem, path->mem()))
        {
            resp.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            std::ostream& out = resp.send();
            out << "500 Internal Server Error: loadConfigItem() failed";
            out.flush();

            logA_ ("moment__channel_manager 500 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());

            goto _return;
        }

        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/plain");
        std::ostream& out = resp.send();
        out << "OK";
        out.flush();

        logA_ ("moment__channel_manager OK ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());

    }
    else if (segments.size() >= 2 && segments[1].compare("add_channel") == 0)
    {
        HTMLForm form( req );

        NameValueCollection::ConstIterator item_name_iter = form.find("conf_file");
        std::string item_name =(item_name_iter != form.end()) ? item_name_iter->second : "";
        NameValueCollection::ConstIterator item_uri_iter = form.find("uri");
        std::string item_uri =(item_uri_iter != form.end()) ? item_uri_iter->second : "";
        NameValueCollection::ConstIterator item_title_iter = form.find("title");
        std::string item_title =(item_title_iter != form.end()) ? item_title_iter->second : "";
        NameValueCollection::ConstIterator update_iter = form.find("update");
        bool const update = (update_iter != form.end());

        if (item_name.size() == 0 ||
            item_uri.size() == 0 ||
            item_title.size() == 0)
        {
            resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            std::ostream& out = resp.send();
            out << "400 Bad Request: no conf_file or uri or title parameter";
            out.flush();

            logA_ ("moment__channel_manager 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        // checking for existing url
        {
            Ref<Vfs> vfs = Vfs::createDefaultLocalVfs (self->confd_dirname->mem());
            Ref<Vfs::VfsDirectory> const parent_dir = vfs->openDirectory (".");
            bool urlIsUsed = false;
            for (;;)
            {
                Ref<String> entry_name;

                if (parent_dir->getNextEntry (entry_name) && entry_name)
                {
                    if(std::string(entry_name->cstr()).compare(".") == 0 || std::string(entry_name->cstr()).compare("..") == 0)
                        continue;

                    StRef<String> fullname = st_makeString(self->confd_dirname, "/", entry_name->cstr());
                    logD_(_func_, "fullname = ", fullname);

                    std::ifstream file (fullname->cstr());
                    if (!file.is_open())
                    {
                        logE_(_func_, "fail to open file = ", fullname);
                        continue;
                    }

                    std::string line;
                    while ( std::getline(file, line) )
                    {
                        size_t uriSrc_beg = line.find("uri = \"");
                        if(uriSrc_beg == std::string::npos)
                            continue;
                        uriSrc_beg += strlen("uri = \"");
                        size_t uriSrc_end = line.rfind("\"");
                        std::string uriSrc = line.substr(uriSrc_beg, uriSrc_end - uriSrc_beg);

                        logD_(_func_, "uriSrc = ", uriSrc.c_str());

                        if(uriSrc.compare(item_uri) == 0 && 
                            item_name.compare(std::string(entry_name->cstr())) != 0)
                        {
                            logD_(_func_, "uriSrc is already used");
                            urlIsUsed = true;
                        }
                    }

                    file.close();
                }
                else
                {
                    break;
                }
            }
            if(urlIsUsed)
            {
                resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
                std::ostream& out = resp.send();
                out << "such url is already used";
                out.flush();

                goto _return;
            }
        }

        ConstMemory const dir_name = self->confd_dirname->mem();
        StRef<String> const path = st_makeString (dir_name, "/", item_name.c_str());

        bool bSourceExist = false;
        if (FILE *file = fopen(path->cstr(), "r")) {
            fclose(file);
            bSourceExist = true;
        }

        if(bSourceExist && !update)
        {
            resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            std::ostream& out = resp.send();
            out << "source already exists (missed update parameter?)";
            out.flush();

            goto _return;
        }
        else if (!bSourceExist && update)
        {
            resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            std::ostream& out = resp.send();
            out << "no source to update, need to add";
            out.flush();

            goto _return;
        }
        else
        {
            FILE *fp;
            fp = fopen( path->cstr(), "w");
            if(fp == NULL)
            {
                logE_ (_func, "could not open file [", path->cstr(),"]\n");
            }
            else
            {
                StRef<String> const content = st_makeString ("title = \"", item_title.c_str(),
                                                             "\"\nuri = \"", item_uri.c_str(), "\"\n");
                fprintf(fp, content->cstr());
                fclose(fp);
            }
        }

        ConstMemory item_name_mem = ConstMemory(item_name.c_str(), item_name.size());
        if (!self->loadConfigItem (item_name_mem, path->mem()))
        {
            resp.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            std::ostream& out = resp.send();
            out << "500 Internal Server Error: loadConfigItem() failed";
            out.flush();

            logA_ ("moment__channel_manager 500 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());

            goto _return;
        }

        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/plain");
        std::ostream& out = resp.send();
        out << "OK";
        out.flush();

    }
    else if (segments.size() >= 2 && segments[1].compare("remove_channel") == 0)
    {
        HTMLForm form( req );

        NameValueCollection::ConstIterator item_name_iter = form.find("conf_file");
        std::string item_name = (item_name_iter != form.end()) ? item_name_iter->second: "";

        //ConstMemory const item_name = req->getParameter ("conf_file");
        if (item_name.size() == 0)
        {
            resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
            std::ostream& out = resp.send();
            out << "400 Bad Request: no conf_file parameter";
            out.flush();

            logA_ ("moment__channel_manager 400 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());
            goto _return;
        }

        ConstMemory const dir_name = self->confd_dirname->mem();
        StRef<String> const path = st_makeString (dir_name, "/", item_name.c_str());

        bool bSourceExist = false;
        if (FILE *file = fopen(path->cstr(), "r"))
        {
            fclose(file);
            bSourceExist = true;
        }

        if(bSourceExist)
        {
            if( remove( path->cstr() ) != 0 )
                logE_ (_func, "could not delete file", path->cstr());
        }
        else
        {
            resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
            std::ostream& out = resp.send();
            out << "no source to delete";
            out.flush();

            goto _return;
        }

        ConstMemory item_name_mem = ConstMemory(item_name.c_str(), item_name.size());
        self->loadConfigItem (item_name_mem, path->mem());

        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("text/plain");
        std::ostream& out = resp.send();
        out << "OK";
        out.flush();
    }
    else
    {
        logE_ (_func, "Unknown server HTTP request: ", req.getURI().c_str());

        logA_ ("moment_server__server 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());

        return false;
    }

_return:
    return true;
}


bool
ChannelManager::serverHttpRequest (HTTPServerRequest &req, HTTPServerResponse &resp, void * _self)
{
    ChannelManager * const self = static_cast <ChannelManager*> (_self);

    URI uri(req.getURI());
    std::vector < std::string > segments;
    uri.getPathSegments(segments);

    if(segments.size() >= 2 && segments[1].compare("playlist.json") == 0 && self->serve_playlist_json )
    {
        logD_ (_func, "playlist.json");

        PagePool::PageListHead page_list;

        std::string json_respond;
        self->mutex.lock ();
        {
            // TODO: Update once on config reload
            StRef<String> this_rtmp_server_addr;
            StRef<String> this_rtmpt_server_addr;
            {
                self->moment->configLock ();
                MomentServer::VarHash * const var_hash = self->moment->getDefaultVarHash_unlocked ();

                if (MomentServer::VarHashEntry * const entry = var_hash->lookup (ConstMemory ("RtmpAddr")))
                    this_rtmp_server_addr = st_grab (new (std::nothrow) String (entry->var->getValue()));
                else
                    this_rtmp_server_addr = st_grab (new (std::nothrow) String ("127.0.0.1:1935"));

                if (MomentServer::VarHashEntry * const entry = var_hash->lookup (ConstMemory ("RtmptAddr")))
                    this_rtmpt_server_addr = st_grab (new (std::nothrow) String (entry->var->getValue()));
                else
                    this_rtmpt_server_addr = st_grab (new (std::nothrow) String ("127.0.0.1:8080"));

                self->moment->configUnlock ();
            }

            bool use_rtmpt_proto = equal(self->playlist_json_protocol->mem(), "rtmpt");

            // Create writers
            Json::FastWriter json_writer_fast;
            Json::StyledWriter json_writer_styled;

            // Create root and source list Json values
            Json::Value json_value_root;
            Json::Value json_value_sources;

            // Run through all sources
            ItemHash::iterator iter(self->item_hash);
            while (!iter.done())
            {
                ConfigItem * const item = iter.next()->ptr();

                // Prepare Json value describes current source
                Json::Value json_value_source;
                json_value_source["name"] = item->channel_name->cstr();
                json_value_source["title"] = item->channel_title ? item->channel_title->cstr() : item->channel_name->cstr();
                if (use_rtmpt_proto)
                    json_value_source["uri"] = st_makeString(ConstMemory("rtmpt://"), this_rtmpt_server_addr->mem(), ConstMemory("/live/"), item->channel_name->mem())->cstr();
                else
                    json_value_source["uri"] = st_makeString(ConstMemory("rtmp://"), this_rtmp_server_addr->mem(), ConstMemory("/live/"), item->channel_name->mem())->cstr();

                ConstMemory const dir_name = self->confd_dirname->mem();
                StRef<String> const path = st_makeString (dir_name, "/", item->channel_name->cstr());

                std::ifstream file(path->cstr());
                if (file.is_open()) 
                {
                    logD_ (_func, "PATH_NAME: ", path->cstr());
                    std::string line;
                    while ( std::getline(file,line) )
                    {
                        size_t uriSrc_beg = line.find("uri = \"");
                        if(uriSrc_beg == std::string::npos)
                            continue;
                        uriSrc_beg += strlen("uri = \"");
                        size_t uriSrc_end = line.rfind("\"");
                        std::string uriSrc = line.substr(uriSrc_beg, uriSrc_end - uriSrc_beg);
                        json_value_source["uriSrc"] = uriSrc.c_str();
                    }

                    file.close();
                }

                // Add just created Json value to Json source list
                json_value_sources.append(json_value_source);

                logD_ (_func, "playlist.json line: ", json_writer_fast.write(json_value_source).c_str());
            }

            // Add just created Json source list to root Json value
            json_value_root["sources"] = json_value_sources;

            json_respond = json_writer_styled.write(json_value_root);
            self->page_pool->getFillPages(&page_list, ConstMemory(json_respond.c_str(), json_respond.length()));
        }
        self->mutex.unlock ();


        Size content_len = 0;
        {
            PagePool::Page *page = page_list.first;
            while (page)
            {
                content_len += page->data_len;
                page = page->getNextMsgPage();
            }
        }

        resp.setStatus(HTTPResponse::HTTP_OK);
        resp.setContentType("application/json");
        std::ostream& out = resp.send();
        out << json_respond;
        out.flush();

        logA_ ("moment__channel_manager 200 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());

        return true;
    }

    logE_ (_func, "Unknown server HTTP request: ", req.getURI().c_str());

    logA_ ("moment_server__server 404 ", req.clientAddress().toString().c_str(), " ", req.getURI().c_str());

    return false;
}

void
ChannelManager::notifyChannelCreated (ChannelInfo * const mt_nonnull channel_info)
{
    logD_ (_func_);

    {
        ChannelCreationMessage * const msg = new (std::nothrow) ChannelCreationMessage;
        assert (msg);
        msg->channel = channel_info->channel;
        msg->channel_name = st_grab (new (std::nothrow) String (channel_info->channel_name));
        msg->config = channel_info->config;

        mutex.lock ();
        channel_creation_messages.append (msg);
        mutex.unlock ();
    }

    deferred_reg.scheduleTask (&channel_created_task, false /* permanent */);
}

bool
ChannelManager::channelCreatedTask (void * const _self)
{
    ChannelManager * const self = static_cast <ChannelManager*> (_self);

    logD_ (_func_);

    self->mutex.lock ();

    while (!self->channel_creation_messages.isEmpty()) {
        ChannelCreationMessage * const msg = self->channel_creation_messages.getFirst();
        self->channel_creation_messages.remove (msg);

        ChannelInfo channel_info;
        channel_info.channel      = msg->channel;
        channel_info.channel_name = msg->channel_name->mem();
        channel_info.config       = msg->config;

        self->mutex.unlock ();

        self->fireChannelCreated (&channel_info);
        delete msg;

        self->mutex.lock ();
    }

    self->mutex.unlock ();

    return false /* do not rechedule */;
}

Result
ChannelManager::loadConfigFull ()
{
    logD_ (_func_);

    ConstMemory const dir_name = confd_dirname->mem();
    Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (dir_name);

    Ref<Vfs::VfsDirectory> const dir = vfs->openDirectory ("");
    if (!dir) {
        logE_ (_func, "could not open ", dir_name, " directory: ", exc->toString());
        return Result::Success;
    }

    for (;;) {
        Ref<String> filename;
        if (!dir->getNextEntry (filename)) {
            logE_ (_func, "Vfs::VfsDirectory::getNextEntry() failed: ", exc->toString());
            return Result::Failure;
        }
        if (!filename)
            break;

        if (equal (filename->mem(), ".") || equal (filename->mem(), ".."))
            continue;

        // Don't process hidden files (e.g. vim tmp files).
        if (filename->len() > 0 && filename->mem().mem() [0] == '.')
            continue;

        StRef<String> const path = st_makeString (dir_name, "/", filename->mem());
        if (!loadConfigItem (filename->mem(), path->mem()))
            return Result::Failure;
    }

    return Result::Success;
}

#if 0
// Deprecated

static ConstMemory itemToStreamName (ConstMemory const item_name)
{
    ConstMemory stream_name = item_name;
    for (Size i = stream_name.len(); i > 0; --i) {
        if (stream_name.mem() [i - 1] == '.') {
            stream_name = stream_name.region (0, i - 1);
            break;
        }
    }

    return stream_name;
}

static Result
parseChannelConfig (MConfig::Section * const mt_nonnull section,
                    ConstMemory        const config_item_name,
                    ChannelOptions   * const default_opts,
                    ChannelOptions   * const mt_nonnull opts,
                    PlaybackItem     * const mt_nonnull item)
{
    char const opt_name__name []                     = "name";
    char const opt_name__title[]                     = "title";
    char const opt_name__desc []                     = "desc";
    char const opt_name__chain[]                     = "chain";
    char const opt_name__uri  []                     = "uri";
    char const opt_name__playlist[]                  = "playlist";
    char const opt_name__master[]                    = "master";
    char const opt_name__keep_video_stream[]         = "keep_video_stream";
    char const opt_name__continuous_playback[]       = "continous_playback";
    char const opt_name__record_path[]               = "record_path";
    char const opt_name__connect_on_demand[]         = "connect_on_demand";
    char const opt_name__connect_on_demand_timeout[] = "connect_on_demand_timeout";
    char const opt_name__no_video_timeout[]          = "no_video_timeout";
    char const opt_name__min_playlist_duration[]     = "min_playlist_duration";
    char const opt_name__no_audio[]                  = "no_audio";
    char const opt_name__no_video[]                  = "no_video";
    char const opt_name__force_transcode[]           = "force_transcode";
    char const opt_name__force_transcode_audio[]     = "force_transcode_audio";
    char const opt_name__force_transcode_video[]     = "force_transcode_video";
    char const opt_name__aac_perfect_timestamp[]     = "aac_perfect_timestamp";
    char const opt_name__sync_to_clock[]             = "sync_to_clock";
    char const opt_name__send_metadata[]             = "send_metadata";
    char const opt_name__enable_prechunking[]        = "enable_prechunking";
// TODO resize and transcoding settings
#if 0
    char const opt_name__width[]                     = "width";
    char const opt_name__height[]                    = "height";
    char const opt_name__bitrate[]                   = "bitrate";
#endif

    ConstMemory channel_name = itemToStreamName (config_item_name);
    {
        MConfig::Section::attribute_iterator attr_iter (*section);
        MConfig::Attribute *name_attr = NULL;
        if (!attr_iter.done()) {
            MConfig::Attribute * const attr = attr_iter.next ();
            if (!attr->hasValue()) {
                channel_name = attr->getName();
                name_attr = attr;
            }
        }

        if (MConfig::Attribute * const attr = section->getAttribute (opt_name__name)) {
            if (attr != name_attr)
                channel_name = attr->getValue ();
        }
    }
    logD_ (_func, opt_name__name, ": ", channel_name);

    ConstMemory channel_title = channel_name;
    if (MConfig::Option * const opt = section->getOption (opt_name__title)) {
        if (opt->getValue())
            channel_title = opt->getValue()->mem();
    }
    logD_ (_func, opt_name__title, ": ", channel_title);

    ConstMemory channel_desc = default_opts->channel_desc->mem();
    if (MConfig::Option * const opt = section->getOption (opt_name__desc)) {
        if (opt->getValue())
            channel_desc = opt->getValue()->mem();
    }
    logD_ (_func, opt_name__desc, ": ", channel_desc);

    PlaybackItem::SpecKind spec_kind = PlaybackItem::SpecKind::None;
    ConstMemory stream_spec;
    {
        int num_set_opts = 0;

        if (MConfig::Option * const opt = section->getOption (opt_name__chain)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Chain;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__chain, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__uri)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Uri;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__uri, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__playlist)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Playlist;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__playlist, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__master)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Slave;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__master, ": ", stream_spec);
        }

        if (num_set_opts > 1)
            logW_ (_func, "only one of uri/chain/playlist options should be specified");
    }

    bool keep_video_stream = default_opts->keep_video_stream;
    if (!configSectionGetBoolean (section,
                                  opt_name__keep_video_stream,
                                  &keep_video_stream,
                                  keep_video_stream))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__keep_video_stream, ": ", keep_video_stream);

    bool continuous_playback = default_opts->continuous_playback;
    if (!configSectionGetBoolean (section,
                                  opt_name__continuous_playback,
                                  &continuous_playback,
                                  continuous_playback))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__continuous_playback, ": ", continuous_playback);

    ConstMemory record_path = default_opts->record_path->mem();
    bool got_record_path = false;
    if (MConfig::Option * const opt = section->getOption (opt_name__record_path)) {
        if (opt->getValue()) {
            record_path = opt->getValue()->mem();
            got_record_path = true;
        }
    }
    logD_ (_func, opt_name__record_path, ": ", record_path);

    bool connect_on_demand = default_opts->connect_on_demand;
    if (!configSectionGetBoolean (section,
                                  opt_name__connect_on_demand,
                                  &connect_on_demand,
                                  connect_on_demand))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__connect_on_demand, ": ", connect_on_demand);

    Uint64 connect_on_demand_timeout = default_opts->connect_on_demand_timeout;
    if (!configSectionGetUint64 (section,
                                 opt_name__connect_on_demand_timeout,
                                 &connect_on_demand_timeout,
                                 connect_on_demand_timeout))
    {
        return Result::Failure;
    }

    Uint64 no_video_timeout = default_opts->no_video_timeout;
    if (!configSectionGetUint64 (section,
                                 opt_name__no_video_timeout,
                                 &no_video_timeout,
                                 no_video_timeout))
    {
        return Result::Failure;
    }

    Uint64 min_playlist_duration = default_opts->min_playlist_duration_sec;
    if (!configSectionGetUint64 (section,
                                 opt_name__min_playlist_duration,
                                 &min_playlist_duration,
                                 min_playlist_duration))
    {
        return Result::Failure;
    }

// TODO PushAgent    ConstMmeory push_uri;

    bool no_audio = default_opts->default_item->no_audio;
    if (!configSectionGetBoolean (section, opt_name__no_audio, &no_audio, no_audio))
        return Result::Failure;
    logD_ (_func, opt_name__no_audio, ": ", no_audio);

    bool no_video = default_opts->default_item->no_video;
    if (!configSectionGetBoolean (section, opt_name__no_video, &no_video, no_video))
        return Result::Failure;
    logD_ (_func, opt_name__no_video, ": ", no_video);

    bool force_transcode = default_opts->default_item->force_transcode;
    if (!configSectionGetBoolean (section,
                                  opt_name__force_transcode,
                                  &force_transcode,
                                  force_transcode))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__force_transcode, ": ", force_transcode);

    bool force_transcode_audio = default_opts->default_item->force_transcode_audio;
    if (!configSectionGetBoolean (section,
                                  opt_name__force_transcode_audio,
                                  &force_transcode_audio,
                                  force_transcode_audio))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__force_transcode_audio, ": ", force_transcode_audio);

    bool force_transcode_video = default_opts->default_item->force_transcode_video;
    if (!configSectionGetBoolean (section,
                                  opt_name__force_transcode_video,
                                  &force_transcode_video,
                                  force_transcode_video))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__force_transcode_video, ": ", force_transcode_video);

    bool aac_perfect_timestamp = default_opts->default_item->aac_perfect_timestamp;
    if (!configSectionGetBoolean (section,
                                  opt_name__aac_perfect_timestamp,
                                  &aac_perfect_timestamp,
                                  aac_perfect_timestamp))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__aac_perfect_timestamp, ": ", aac_perfect_timestamp);

    bool sync_to_clock = default_opts->default_item->sync_to_clock;
    if (!configSectionGetBoolean (section,
                                  opt_name__sync_to_clock,
                                  &sync_to_clock,
                                  sync_to_clock))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__sync_to_clock, ": ", sync_to_clock);

    bool send_metadata = default_opts->default_item->send_metadata;
    if (!configSectionGetBoolean (section,
                                  opt_name__send_metadata,
                                  &send_metadata,
                                  send_metadata))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__send_metadata, ": ", send_metadata);

    bool enable_prechunking = default_opts->default_item->enable_prechunking;
    if (!configSectionGetBoolean (section,
                                  opt_name__enable_prechunking,
                                  &enable_prechunking,
                                  enable_prechunking))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__enable_prechunking, ": ", enable_prechunking);

    opts->channel_name  = st_grab (new (std::nothrow) String (channel_name));
    opts->channel_title = st_grab (new (std::nothrow) String (channel_title));
    opts->channel_desc  = st_grab (new (std::nothrow) String (channel_desc));

    opts->keep_video_stream  = keep_video_stream;
    opts->continuous_playback = continuous_playback;

    opts->recording = got_record_path;
    opts->record_path = st_grab (new (std::nothrow) String (record_path));

    opts->connect_on_demand = connect_on_demand;
    opts->connect_on_demand_timeout = connect_on_demand_timeout;

    opts->no_video_timeout = no_video_timeout;
    opts->min_playlist_duration_sec = min_playlist_duration;

    item->stream_spec = st_grab (new (std::nothrow) String (stream_spec));
    item->spec_kind = spec_kind;

    item->no_audio = no_audio;
    item->no_video = no_video;

    item->force_transcode = force_transcode;
    item->force_transcode_audio = force_transcode_audio;
    item->force_transcode_video = force_transcode_video;

    item->aac_perfect_timestamp = aac_perfect_timestamp;
    item->sync_to_clock = sync_to_clock;

    item->send_metadata = /* TODO send_metadata */ false;
    item->enable_prechunking = enable_prechunking;

    return Result::Success;
}
#endif

Result
ChannelManager::loadConfigItem (ConstMemory const item_name,
                                ConstMemory const item_path)
{
    logD_ (_func, "item_name: ", item_name, ", item_path: ", item_path);

    Ref<MConfig::Config> const config = grab (new (std::nothrow) MConfig::Config);
    if (!MConfig::parseConfig (item_path, config)) {
        logE_ (_func, "could not parse config file ", item_path);

        mutex.lock ();
        if (ItemHash::EntryKey const old_item_key = item_hash.lookup (item_name)) {
            StRef<ConfigItem> const item = old_item_key.getData();
            item->channel->getPlayback()->stop ();
            item_hash.remove (old_item_key);
        }
        mutex.unlock ();

        return Result::Failure;
    }

    Ref<VideoStream> const stream = grab (new (std::nothrow) VideoStream);

    mutex.lock ();

    StRef<ConfigItem> item;
    if (ItemHash::EntryKey const old_item_key = item_hash.lookup (item_name)) {
        item = old_item_key.getData();
        item->channel->getPlayback()->stop ();
    } else {
        item = st_grab (new (std::nothrow) ConfigItem);
        item_hash.add (item_name, item);
    }

    Ref<ChannelOptions> const channel_opts = grab (new (std::nothrow) ChannelOptions);
    *channel_opts = *default_channel_opts;

    Ref<PlaybackItem> const playback_item = grab (new (std::nothrow) PlaybackItem);
    channel_opts->default_item = playback_item;
    *playback_item = *default_channel_opts->default_item;

    parseChannelConfig (config->getRootSection(),
                        item_name,
                        default_channel_opts,
                        channel_opts,
                        playback_item);
    item->channel_name  = st_grab (new (std::nothrow) String (channel_opts->channel_name->mem()));
    item->channel_title = st_grab (new (std::nothrow) String (channel_opts->channel_title->mem()));

    logD_ (_func, "ChannelOptions: ");
    channel_opts->dump ();

    logD_ (_func, "PlaybackItem: ");
    playback_item->dump ();

#warning Don't substitute existing channel.
#warning Notify only when a new channel is created.
    item->channel = grab (new (std::nothrow) Channel);
    item->channel->init (moment, channel_opts);

    item->config = config;

    // Calling with 'mutex' locked for extra safety.
    item->channel->getPlayback()->setSingleItem (playback_item);

    mutex.unlock ();

    {
        ChannelInfo channel_info;
        channel_info.channel      = item->channel;
        channel_info.channel_name = item->channel_name->mem();
        channel_info.config       = item->config;

        notifyChannelCreated (&channel_info);
    }

    return Result::Success;
}

void
ChannelManager::setDefaultChannelOptions (ChannelOptions * const mt_nonnull channel_opts)
{
    mutex.lock ();
    default_channel_opts = channel_opts;
    mutex.unlock ();
}

mt_const void
ChannelManager::init (MomentServer * const mt_nonnull moment,
                      PagePool     * const mt_nonnull page_pool)
{
    this->moment = moment;
    this->page_pool = page_pool;

    {
        Ref<MConfig::Config> const config = moment->getConfig();
        confd_dirname = st_grab (new (std::nothrow) String (
                                config->getString_default ("moment/confd_dir", "/opt/moment/conf.d")));

        {
            ConstMemory const opt_name = "moment/playlist_json";
            MConfig::BooleanValue const val = config->getBoolean (opt_name);
            logI_ (_func, opt_name, ": ", config->getString (opt_name));
            if (val == MConfig::Boolean_Invalid)
                logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));

            if (val == MConfig::Boolean_False)
                serve_playlist_json = false;
            else
                serve_playlist_json = true;
        }

        {
            ConstMemory const opt_name = "moment/playlist_json_protocol";
            ConstMemory opt_val = config->getString (opt_name);
            if (opt_val.len() == 0)
                opt_val = "rtmp";

            StRef<String> val_lowercase = st_grab (new (std::nothrow) String (opt_val));
            Byte * const buf = val_lowercase->mem().mem();
            for (Size i = 0, i_end = val_lowercase->len(); i < i_end; ++i)
                buf [i] = (Byte) tolower (buf [i]);

            if (!equal (val_lowercase->mem(), "rtmpt"))
                val_lowercase = st_grab (new (std::nothrow) String ("rtmp"));

            logI_ (_func, opt_name, ": ", val_lowercase->mem());

            playlist_json_protocol = val_lowercase;
        }
    }

    deferred_reg.setDeferredProcessor (
            moment->getServerApp()->getServerContext()->getMainThreadContext()->getDeferredProcessor());

    HttpReqHandler::addHandler(std::string("server"), serverHttpRequest, this);
    AdminHttpReqHandler::addHandler(std::string("admin"), adminHttpRequest, this);
}

ChannelManager::ChannelManager ()
    : event_informer (this /* coderef_container */, &mutex),
      page_pool      (this /* coderef_container */),
      serve_playlist_json (true)
{
    channel_created_task.cb = CbDesc<DeferredProcessor::TaskCallback> (channelCreatedTask, this, this);
}

ChannelManager::~ChannelManager ()
{
    deferred_reg.release ();

    {
        ChannelCreationMessageList::iterator iter (channel_creation_messages);
        while (!iter.done()) {
            ChannelCreationMessage * const msg = iter.next ();
            delete msg;
        }
        channel_creation_messages.clear ();
    }
}

}

