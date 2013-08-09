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


#include <moment/slave_media_source.h>

#include <moment/moment_server.h>


namespace Moment {

using namespace M;

static LogGroup libMary_logGroup_session ("MomentServer.session", LogLevel::I);

MomentServer* MomentServer::instance = NULL;


// ___________________________ HTTP request handlers ___________________________

HttpService::HttpHandler const MomentServer::admin_http_handler = {
    adminHttpRequest,
    NULL /* httpMessageBody */
};

Result
MomentServer::adminHttpRequest (HttpRequest   * const mt_nonnull req,
                                Sender        * const mt_nonnull conn_sender,
                                Memory const  &msg_body,
                                void         ** const mt_nonnull /* ret_msg_data */,
                                void          * const _self)
{
    MomentServer * const self = static_cast <MomentServer*> (_self);

    logD_ (_func_);

    MOMENT_SERVER__HEADERS_DATE

    if (req->getNumPathElems() >= 2
	&& equal (req->getPath (1), "stat"))
    {
        PagePool::PageListHead page_list;

        self->page_pool->printToPages (
                &page_list,
                "<html>"
                "<body>"
                "<p>Stats</p>"
                "<table>");
        {
            List<Stat::StatParam> stat_params;
            getStat()->getAllParams (&stat_params);

            List<Stat::StatParam>::iter iter (stat_params);
            while (!stat_params.iter_done (iter)) {
                Stat::StatParam * const stat_param = &stat_params.iter_next (iter)->data;

                self->page_pool->printToPages (
                        &page_list,
                        "<tr>"
                            "<td>", stat_param->param_name, "</td>");

                if (stat_param->param_type == Stat::ParamType_Int64) {
                    self->page_pool->printToPages (
                            &page_list,
                            "<td>", stat_param->int64_value, "</td>");
                } else {
                    assert (stat_param->param_type == Stat::ParamType_Double);
                    self->page_pool->printToPages (
                            &page_list,
                            "<td>", stat_param->double_value, "</td>");
                }

                self->page_pool->printToPages (
                        &page_list,
                            "<td>", stat_param->param_desc, "</td>"
                        "</tr>");
            }
        }
        self->page_pool->printToPages (
                &page_list,
                "</table>"
                "</body>"
                "</html>");

        Size const data_len = PagePool::countPageListDataLen (page_list.first, 0 /* msg_offset */);

        conn_sender->send (
                self->page_pool,
                false /* do_flush */,
                MOMENT_SERVER__OK_HEADERS ("text/html", data_len),
                "\r\n");
        conn_sender->sendPages (self->page_pool, page_list.first, true /* do_flush */);

        logA_ ("file 200 ", req->getClientAddress(), " ", req->getRequestLine());
    } else {
        HttpHandlerEntryList::iterator iter (self->admin_http_handlers);
        while (!iter.done()) {
            HttpHandlerEntry * const handler_entry = iter.next ();

            if (handler_entry->cb && handler_entry->cb->httpRequest) {
                HttpRequestResult res;
                if (!handler_entry->cb.call_ret (&res, handler_entry->cb->httpRequest, req, conn_sender, msg_body))
                    continue;

                if (res == HttpRequestResult::Success)
                    return Result::Success;

                assert (res == HttpRequestResult::NotFound);
            }
        }

	logE_ (_func, "Unknown admin HTTP request: ", req->getFullPath());

	ConstMemory const reply_body = "Unknown command";
	conn_sender->send (self->page_pool,
			   true /* do_flush */,
			   MOMENT_SERVER__404_HEADERS (reply_body.len()),
			   "\r\n",
			   reply_body);

	logA_ ("moment_server__admin 404 ", req->getClientAddress(), " ", req->getRequestLine());
    }

    if (!req->getKeepalive())
        conn_sender->closeAfterFlush();

    return Result::Success;
}

HttpService::HttpHandler const MomentServer::server_http_handler = {
    serverHttpRequest,
    NULL /* httpMessageBody */
};

Result
MomentServer::serverHttpRequest (HttpRequest   * const mt_nonnull req,
                                 Sender        * const mt_nonnull conn_sender,
                                 Memory const  &msg_body,
                                 void         ** const mt_nonnull /* ret_msg_data */,
                                 void          * const _self)
{
    MomentServer * const self = static_cast <MomentServer*> (_self);

    logD_ (_func_);

    MOMENT_SERVER__HEADERS_DATE;

    {
        HttpHandlerEntryList::iterator iter (self->server_http_handlers);
        while (!iter.done()) {
            HttpHandlerEntry * const handler_entry = iter.next ();

            if (handler_entry->cb && handler_entry->cb->httpRequest) {
                HttpRequestResult res;
                if (!handler_entry->cb.call_ret (&res, handler_entry->cb->httpRequest, req, conn_sender, msg_body))
                    continue;

                if (res == HttpRequestResult::Success)
                    return Result::Success;

                assert (res == HttpRequestResult::NotFound);
            }
        }

	logE_ (_func, "Unknown server HTTP request: ", req->getFullPath());

	ConstMemory const reply_body = "Unknown command";
	conn_sender->send (self->page_pool,
			   true /* do_flush */,
			   MOMENT_SERVER__404_HEADERS (reply_body.len()),
			   "\r\n",
			   reply_body);

	logA_ ("moment_server__server 404 ", req->getClientAddress(), " ", req->getRequestLine());
    }

    if (!req->getKeepalive())
        conn_sender->closeAfterFlush();

    return Result::Success;
}

void
MomentServer::addAdminRequestHandler (CbDesc<HttpRequestHandler> const &cb)
{
    HttpHandlerEntry * const handler_entry = new (std::nothrow) HttpHandlerEntry;
    assert (handler_entry);
    handler_entry->cb = cb;
    admin_http_handlers.append (handler_entry);
}

void
MomentServer::addServerRequestHandler (CbDesc<HttpRequestHandler> const &cb)
{
    HttpHandlerEntry * const handler_entry = new (std::nothrow) HttpHandlerEntry;
    assert (handler_entry);
    handler_entry->cb = cb;
    server_http_handlers.append (handler_entry);
}


// ___________________________ Video stream handlers ___________________________

namespace {
    class InformVideoStreamAdded_Data
    {
    public:
        VideoStream * const video_stream;
        ConstMemory   const stream_name;

        InformVideoStreamAdded_Data (VideoStream * const video_stream,
                                     ConstMemory   const stream_name)
            : video_stream (video_stream),
              stream_name (stream_name)
        {
        }
    };
}

void
MomentServer::informVideoStreamAdded (VideoStreamHandler * const vs_handler,
                                      void               * const cb_data,
                                      void               * const _inform_data)
{
    if (vs_handler->videoStreamAdded) {
        InformVideoStreamAdded_Data * const inform_data =
                static_cast <InformVideoStreamAdded_Data*> (_inform_data);
        vs_handler->videoStreamAdded (inform_data->video_stream,
                                      inform_data->stream_name,
                                      cb_data);
    }
}

#if 0
void
MomentServer::fireVideoStreamAdded (VideoStream * const mt_nonnull video_stream,
                                    ConstMemory   const stream_name)
{
    InformVideoStreamAdded_Data inform_data (video_stream, stream_name);
    video_stream_informer.informAll (informVideoStreamAdded, &inform_data);
}
#endif

mt_mutex (mutex) void
MomentServer::notifyDeferred_VideoStreamAdded (VideoStream * const mt_nonnull video_stream,
                                               ConstMemory  const stream_name)
{
    VideoStreamAddedNotification * const notification = &vs_added_notifications.appendEmpty()->data;
    notification->video_stream = video_stream;
    notification->stream_name = grab (new (std::nothrow) String (stream_name));

    vs_inform_reg.scheduleTask (&vs_added_inform_task, false /* permanent */);
}

mt_mutex (mutex) void
MomentServer::notifyDeferred_StreamClosed (VideoStream * const mt_nonnull stream)
{
    StreamClosedNotification * const notification = &vs_closed_notifications.appendEmpty()->data;
    notification->stream = stream;

    vs_inform_reg.scheduleTask (&vs_closed_inform_task, false /* permanent */);
}

bool
MomentServer::videoStreamAddedInformTask (void * const _self)
{
    MomentServer * const self = static_cast <MomentServer*> (_self);

    self->mutex.lock ();
    while (!self->vs_added_notifications.isEmpty()) {
        VideoStreamAddedNotification * const notification = &self->vs_added_notifications.getFirst();

        Ref<VideoStream> video_stream;
        video_stream.setNoRef ((VideoStream*) notification->video_stream);
        notification->video_stream.setNoUnref ((VideoStream*) NULL);

        Ref<String> stream_name;
        stream_name.setNoRef ((String*) notification->stream_name);
        notification->stream_name.setNoUnref ((String*) NULL);

        self->vs_added_notifications.remove (self->vs_added_notifications.getFirstElement());

        InformVideoStreamAdded_Data inform_data (video_stream, stream_name->mem());
        mt_unlocks_locks (self->mutex) self->video_stream_informer.informAll_unlocked (informVideoStreamAdded, &inform_data);
    }
    self->mutex.unlock ();

    return false /* Do not reschedule */;
}

bool
MomentServer::streamClosedInformTask (void * const _self)
{
    MomentServer * const self = static_cast <MomentServer*> (_self);

    self->mutex.lock ();
    while (!self->vs_closed_notifications.isEmpty()) {
        StreamClosedNotification * const notification = &self->vs_closed_notifications.getFirst();

        Ref<VideoStream> stream;
        stream.setNoRef ((VideoStream*) notification->stream);
        notification->stream.setNoUnref ((VideoStream*) NULL);

        self->vs_closed_notifications.remove (self->vs_closed_notifications.getFirstElement());
        self->mutex.unlock ();

        stream->close ();

        self->mutex.lock ();
    }
    self->mutex.unlock ();

    return false /* Do not reschedule */;
}

MomentServer::VideoStreamHandlerKey
MomentServer::addVideoStreamHandler (CbDesc<VideoStreamHandler> const &vs_handler)
{
    VideoStreamHandlerKey vs_handler_key;
    vs_handler_key.sbn_key = video_stream_informer.subscribe (vs_handler);
    return vs_handler_key;
}

mt_mutex (mutex) MomentServer::VideoStreamHandlerKey
MomentServer::addVideoStreamHandler_unlocked (CbDesc<VideoStreamHandler> const &vs_handler)
{
    VideoStreamHandlerKey vs_handler_key;
    vs_handler_key.sbn_key = video_stream_informer.subscribe_unlocked (vs_handler);
    return vs_handler_key;
}

void
MomentServer::removeVideoStreamHandler (VideoStreamHandlerKey vs_handler_key)
{
    video_stream_informer.unsubscribe (vs_handler_key.sbn_key);
}

mt_mutex (mutex) void
MomentServer::removeVideoStreamHandler_unlocked (VideoStreamHandlerKey vs_handler_key)
{
    video_stream_informer.unsubscribe_unlocked (vs_handler_key.sbn_key);
}

// _____________________________________________________________________________


namespace {
    class InformRtmpCommandMessage_Data {
    public:
	RtmpConnection       * const conn;
	VideoStream::Message * const msg;
	ConstMemory const    &method_name;
	AmfDecoder           * const amf_decoder;

	InformRtmpCommandMessage_Data (RtmpConnection       * const conn,
				       VideoStream::Message * const msg,
				       ConstMemory const    &method_name,
				       AmfDecoder           * const amf_decoder)
	    : conn (conn),
	      msg (msg),
	      method_name (method_name),
	      amf_decoder (amf_decoder)
	{
	}
    };
}

void
MomentServer::ClientSession::informRtmpCommandMessage (Events * const events,
						       void   * const cb_data,
						       void   * const _inform_data)
{
    if (events->rtmpCommandMessage) {
        InformRtmpCommandMessage_Data * const inform_data =
                static_cast <InformRtmpCommandMessage_Data*> (_inform_data);
        events->rtmpCommandMessage (inform_data->conn,
                                    inform_data->msg,
                                    inform_data->method_name,
                                    inform_data->amf_decoder,
                                    cb_data);
    }
}

void
MomentServer::ClientSession::informClientDisconnected (Events * const events,
						       void   * const cb_data,
						       void   * const /* _inform_data */)
{
    if (events->clientDisconnected)
        events->clientDisconnected (cb_data);
}

bool
MomentServer::ClientSession::isConnected_subscribe (CbDesc<Events> const &cb)
{
    mutex.lock ();
    if (disconnected) {
	mutex.unlock ();
	return false;
    }
    event_informer.subscribe_unlocked (cb);
    mutex.unlock ();
    return true;
}

void
MomentServer::ClientSession::setBackend (CbDesc<Backend> const &cb)
{
    backend = cb;
}

void
MomentServer::ClientSession::fireRtmpCommandMessage (RtmpConnection       * const mt_nonnull conn,
						     VideoStream::Message * const mt_nonnull msg,
						     ConstMemory            const method_name,
						     AmfDecoder           * const mt_nonnull amf_decoder)
{
    InformRtmpCommandMessage_Data inform_data (conn, msg, method_name, amf_decoder);
    event_informer.informAll (informRtmpCommandMessage, &inform_data);
}

MomentServer::ClientSession::ClientSession ()
    : disconnected (false),
      event_informer (this, &mutex)
{
    logD (session, _func, "0x", fmt_hex, (UintPtr) this);
}

MomentServer::ClientSession::~ClientSession ()
{
    logD (session, _func, "0x", fmt_hex, (UintPtr) this);
}

namespace {
    class InformClientConnected_Data {
    public:
	MomentServer::ClientSession * const client_session;
	ConstMemory const &app_name;
	ConstMemory const &full_app_name;

	InformClientConnected_Data (MomentServer::ClientSession * const client_session,
				    ConstMemory const &app_name,
				    ConstMemory const &full_app_name)
	    : client_session (client_session),
	      app_name (app_name),
	      full_app_name (full_app_name)
	{
	}
    };
}

void
MomentServer::ClientEntry::informClientConnected (ClientHandler * const client_handler,
						  void          * const cb_data,
						  void          * const _inform_data)
{
    if (client_handler->clientConnected) {
        InformClientConnected_Data * const inform_data =
                static_cast <InformClientConnected_Data*> (_inform_data);
        client_handler->clientConnected (inform_data->client_session,
                                         inform_data->app_name,
                                         inform_data->full_app_name,
                                         cb_data);
    }
}

void
MomentServer::ClientEntry::fireClientConnected (ClientSession * const client_session,
						ConstMemory     const app_name,
						ConstMemory     const full_app_name)
{
    InformClientConnected_Data inform_data (client_session, app_name, full_app_name);
    event_informer.informAll (informClientConnected, &inform_data);
}

// @ret_path_tail should contain original path.
mt_mutex (mutex) MomentServer::ClientEntry*
MomentServer::getClientEntry_rec (ConstMemory   const path,
				  ConstMemory * const mt_nonnull ret_path_tail,
				  Namespace   * const mt_nonnull nsp)
{
    Byte const *delim = (Byte const *) memchr (path.mem(), '/', path.len());
    ConstMemory next_name;
    if (delim) {
	next_name = path.region (0, delim - path.mem());
	Namespace::NamespaceHash::EntryKey const next_nsp_key = nsp->namespace_hash.lookup (next_name);
	if (next_nsp_key) {
	    Namespace * const next_nsp = next_nsp_key.getData();
	    ClientEntry * const client_entry = getClientEntry_rec (path.region (delim - path.mem() + 1),
								   ret_path_tail,
								   next_nsp);
	    if (client_entry)
		return client_entry;
	}
    } else {
	next_name = path;
    }

    Namespace::ClientEntryHash::EntryKey const client_entry_key = nsp->client_entry_hash.lookup (next_name);
    if (!client_entry_key)
	return NULL;

    *ret_path_tail = (*ret_path_tail).region ((next_name.mem() + next_name.len()) - (*ret_path_tail).mem());
    return client_entry_key.getData();
}

mt_mutex (mutex) MomentServer::ClientEntry*
MomentServer::getClientEntry (ConstMemory  path,
			      ConstMemory * const mt_nonnull ret_path_tail,
			      Namespace   * const mt_nonnull nsp)
{
    if (path.len() > 0 && path.mem() [0] == '/')
	path = path.region (1);

    *ret_path_tail = path;
    return getClientEntry_rec (path, ret_path_tail, nsp);
}

mt_throws Result
MomentServer::loadModules ()
{
#ifndef LIBMARY_PLATFORM_WIN32
    ConstMemory module_path = config->getString ("moment/module_path");
    if (module_path.len() == 0)
	module_path = LIBMOMENT_PREFIX "/moment-1.0";

    logD_ (_func, "module_path: ", module_path);

    Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (module_path);
    if (!vfs)
	return Result::Failure;

    Ref<Vfs::VfsDirectory> const dir = vfs->openDirectory (ConstMemory());
    if (!dir)
	return Result::Failure;

    StringHash<EmptyBase> loaded_names;

    Ref<String> const mod_gst_name  = makeString (module_path, "/libmoment-gst-1.0");
    Ref<String> const mod_stat_name = makeString (module_path, "/libmoment-stat-1.0");
    Ref<String> const mod_onvif_name  = makeString (module_path, "/libmoment-onvif-1.0");
    Ref<String> const mod_wsdd_name  = makeString (module_path, "/libWsDiscovery");
    Ref<String> const mod_osdk_name  = makeString (module_path, "/libOnvifSDK");

    bool loading_mod_stat = true;
    for (;;) {
        for (;;) {
            Ref<String> dir_entry;
            if (!dir->getNextEntry (dir_entry))
                return Result::Failure;
            if (!dir_entry)
                break;

            Ref<String> const stat_path = makeString (module_path, "/", dir_entry->mem());
            ConstMemory const entry_name = stat_path->mem();

            Vfs::FileStat stat_data;
            if (!vfs->stat (dir_entry->mem(), &stat_data)) {
                logE_ (_func, "Could not stat ", stat_path);
                continue;
            }

            // TODO Find rightmost slash, then skip one dot.
            ConstMemory module_name = entry_name;
            {
                void *dot_ptr = memchr ((void*) entry_name.mem(), '.', entry_name.len());
                // XXX Dirty.
                // Skipping the first dot (belongs to "moment-1.0" substring).
                if (dot_ptr)
                    dot_ptr = memchr ((void*) ((Byte const *) dot_ptr + 1), '.', entry_name.len() - ((Byte const *) dot_ptr - entry_name.mem()) - 1);
                // Skipping the second dot (-1.0 in library version).
                if (dot_ptr)
                    dot_ptr = memchr ((void*) ((Byte const *) dot_ptr + 1), '.', entry_name.len() - ((Byte const *) dot_ptr - entry_name.mem()) - 1);
#ifdef LIBMARY_PLATFORM_WIN32
                // TEST: skipping the third dot.
                // TODO The dots should be skipped from the end of the string!
                if (dot_ptr)
                    dot_ptr = memchr ((void*) ((Byte const *) dot_ptr + 1), '.', entry_name.len() - ((Byte const *) dot_ptr - entry_name.mem()) - 1);
#endif

                if (dot_ptr)
                    module_name = entry_name.region (0, (Byte const *) dot_ptr - entry_name.mem());
            }

            if (equal (module_name, mod_gst_name->mem()) ||
                equal (module_name, mod_onvif_name->mem()) ||
                equal (module_name, mod_wsdd_name->mem()) ||
                equal (module_name, mod_osdk_name->mem()) )
                continue;

            if (loading_mod_stat) {
                if (!equal (module_name, mod_stat_name->mem()))
                    continue;
            }

            if (stat_data.file_type == Vfs::FileType::RegularFile &&
                !loaded_names.lookup (module_name))
            {
                loaded_names.add (module_name, EmptyBase());

                logD_ (_func, "loading module ", module_name);

                if (!loadModule (module_name))
                    logE_ (_func, "Could not load module ", module_name, ": ", exc->toString());
            }
        }

        if (loading_mod_stat) {
            loading_mod_stat = false;
            dir->rewind ();
            continue;
        }

        break;
    }

    {
      // Loading mod_onvif after most of the modules, so that
      // onvif dependencies WsDiscovery and OnvifSDK were already loaded
        assert (!loaded_names.lookup (mod_wsdd_name->mem()));
        loaded_names.add (mod_wsdd_name->mem(), EmptyBase());
        assert (!loaded_names.lookup (mod_osdk_name->mem()));
        loaded_names.add (mod_osdk_name->mem(), EmptyBase());
        assert (!loaded_names.lookup (mod_onvif_name->mem()));
        loaded_names.add (mod_onvif_name->mem(), EmptyBase());

        logD_ (_func, "loading onvif-related modules (forced pre last) ");

        if (!loadModule (mod_wsdd_name->mem()))
            logE_ (_func, "Could not load module ", mod_wsdd_name, ": ", exc->toString());
        if (!loadModule (mod_osdk_name->mem()))
            logE_ (_func, "Could not load module ", mod_osdk_name, ": ", exc->toString());
        if (!loadModule (mod_onvif_name->mem()))
            logE_ (_func, "Could not load module ", mod_onvif_name, ": ", exc->toString());
    }

    {
      // Loading mod_gst last, so that it deinitializes first.
      // We count on the fact that M::Informer prepends new subscribers
      // to the beginning of subscriber list, which is hacky, because
      // M::Informer has no explicit guarantees for that.
      //
      // This is important for proper deinitialization. Ideally, the order
      // of module deinitialization should not matter.
      // The process of deinitialization needs extra thought.

	assert (!loaded_names.lookup (mod_gst_name->mem()));
        loaded_names.add (mod_gst_name->mem(), EmptyBase());

        logD_ (_func, "loading module (forced last) ", mod_gst_name);
        if (!loadModule (mod_gst_name->mem()))
            logE_ (_func, "Could not load module ", mod_gst_name, ": ", exc->toString());
    }
#endif /* LIBMARY_PLATFORM_WIN32 */

#ifdef LIBMARY_PLATFORM_WIN32
    {
        if (!loadModule ("../lib/bin/libmoment-stat-1.0-0.dll"))
            logE_ (_func, "Could not load mod_stat (win32)");

        if (!loadModule ("../lib/bin/libmoment-file-1.0-0.dll"))
            logE_ (_func, "Could not load mod_file (win32)");

        if (!loadModule ("../lib/bin/libmoment-hls-1.0-0.dll"))
            logE_ (_func, "Could not load mod_hls (win32)");

        if (!loadModule ("../lib/bin/libmoment-rtmp-1.0-0.dll"))
            logE_ (_func, "Could not load mod_rtmp (win32)");

        if (!loadModule ("../lib/bin/libmoment-gst-1.0-0.dll"))
            logE_ (_func, "Could not load mod_gst (win32)");

        if (!loadModule ("../lib/bin/libmoment-transcoder-1.0-0.dll"))
            logE_ (_func, "Could not load mod_transcoder (win32)");

        if (!loadModule ("../lib/bin/libmoment-mychat-1.0-0.dll"))
            logE_ (_func, "Could not load mychat module (win32)");

        if (!loadModule ("../lib/bin/libmoment-test-1.0-0.dll"))
            logE_ (_func, "Could not load mychat module (win32)");

        if (!loadModule ("../lib/bin/libmoment-auth-1.0-0.dll"))
            logE_ (_func, "Could not load mod_auth (win32)");

        if (!loadModule ("../lib/bin/libmoment-nvr-1.0-0.dll"))
            logE_ (_func, "Could not load mod_nvr (win32)");

        if (!loadModule ("../lib/bin/libmoment-lectorium-1.0-0.dll"))
            logE_ (_func, "Could not load lectorium (win32)");
    }
#endif

    return Result::Success;
}

ServerApp* MomentServer::getServerApp ()
    { return server_app; }

PagePool* MomentServer::getPagePool ()
    { return page_pool; }

HttpService* MomentServer::getHttpService ()
    { return http_service; }

HttpService* MomentServer::getAdminHttpService ()
    { return admin_http_service; }

ServerThreadPool* MomentServer::getRecorderThreadPool ()
    { return recorder_thread_pool; }

ServerThreadPool* MomentServer::getReaderThreadPool ()
    { return reader_thread_pool; }

Storage* MomentServer::getStorage ()
    { return storage; }

MomentServer* MomentServer::getInstance ()
    { return instance; }


// __________________________________ Config ___________________________________

Ref<MConfig::Config>
MomentServer::getConfig ()
{
    config_mutex.lock ();
    Ref<MConfig::Config> const tmp_config = config;
    config_mutex.unlock ();
    return tmp_config;
}

Ref<MConfig::Varlist>
MomentServer::getDefaultVarlist ()
{
    config_mutex.lock ();
    Ref<MConfig::Varlist> const tmp_varlist = default_varlist;
    config_mutex.unlock ();
    return tmp_varlist;
}

mt_mutex (config_mutex) MConfig::Config*
MomentServer::getConfig_unlocked ()
{
    return config;
}

mt_one_of(( mt_const, mt_mutex (config_mutex) )) void
MomentServer::parseDefaultVarlist (MConfig::Config * mt_nonnull const new_config)
{
    releaseDefaultVarHash ();

    Ref<MConfig::Varlist> const new_varlist = grab (new (std::nothrow) MConfig::Varlist);
    MConfig::Section * const varlist_section = new_config->getSection ("moment/vars", false /* create */);
    if (varlist_section)
        MConfig::parseVarlistSection (varlist_section, new_varlist);

    default_varlist = new_varlist;

    {
	ConstMemory const opt_name = "moment/this_http_server_addr";
	ConstMemory opt_val = config->getString (opt_name);
	logI_ (_func, opt_name, ":  ", opt_val);
	if (!opt_val.mem())
            opt_val = "127.0.0.1:8080";

        new_varlist->addEntry ("HttpAddr",
                               opt_val,
                               true  /* with_value */,
                               false /* enable_section */,
                               false /* disable_section */);
    }

    {
	ConstMemory const opt_name = "moment/this_rtmp_server_addr";
	ConstMemory opt_val = config->getString (opt_name);
	logI_ (_func, opt_name, ":  ", opt_val);
	if (!opt_val.mem())
            opt_val = "127.0.0.1:1935";

        new_varlist->addEntry ("RtmpAddr",
                               opt_val,
                               true  /* with_value */,
                               false /* enable_section */,
                               false /* disable_section */);
    }

    {
	ConstMemory const opt_name = "moment/this_rtmpt_server_addr";
	ConstMemory opt_val = config->getString (opt_name);
	logI_ (_func, opt_name, ":  ", opt_val);
	if (!opt_val.mem())
            opt_val = "127.0.0.1:8080";

        new_varlist->addEntry ("RtmptAddr",
                               opt_val,
                               true  /* with_value */,
                               false /* enable_section */,
                               false /* disable_section */);
    }

    MConfig::Varlist::VarList::iterator iter (new_varlist->var_list);
    while (!iter.done()) {
        MConfig::Varlist::Var * const var = iter.next ();
        VarHashEntry * const entry = new (std::nothrow) VarHashEntry;
        assert (entry);
        entry->var = var;

        if (VarHashEntry * const old_entry = default_var_hash.lookup (var->getName())) {
            default_var_hash.remove (old_entry);
            delete old_entry;
        }

        default_var_hash.add (entry);
    }
}

mt_mutex (config_mutex) void
MomentServer::releaseDefaultVarHash ()
{
    VarHash::iterator iter (default_var_hash);
    while (!iter.done()) {
        VarHashEntry * const entry = iter.next ();
        delete entry;
    }

    default_var_hash.clear ();
}

void
MomentServer::setConfig (MConfig::Config * mt_nonnull const new_config)
{
    config_mutex.lock ();
    config = new_config;
    parseDefaultVarlist (new_config);

    // TODO Simplify synchronization: require the client to call getConfig() when handling configReload().
    //      'config_mutex' will then become unnecessary (! _vast_ simplification).
    //      Pass MomentServer pointer arg for convenience.
    // ^^^ The idea may be not that good at all.

    // setConfig() может вызываться параллельно
    // каждый setConfig() завершится только после уведомления всех слушателей
    // без mutex не будет установленного порядка уведомления
    // вызовы setConfig() могут быть вложенными, в этом случае параметр new_config не работает.
    // два параллельных вызова setConfig => путаница, какой конфиг реально в силе?
    // НО при этом реально в силе только один конфиг.
    // Я за то, чтобы _явно_ сериализовать обновления конфига и не накладывать лишний ограничений.
    // _но_ в этом случае не будет возможности определить момент, когда новый конфиг
    // полностью применён.

    fireConfigReload (new_config);
    config_mutex.unlock ();
}

void
MomentServer::configLock ()
{
    config_mutex.lock ();
}

void
MomentServer::configUnlock ()
{
    config_mutex.unlock ();
}

// _____________________________________________________________________________


Ref<MomentServer::ClientSession>
MomentServer::rtmpClientConnected (ConstMemory const &path,
				   RtmpConnection    * const mt_nonnull conn,
				   IpAddress   const &client_addr)
{
    logD (session, _func_);

    ConstMemory path_tail;

    Ref<AuthSession> auth_session;
    if (auth_backend) {
        auth_backend.call_ret< Ref<AuthSession> > (&auth_session,
                                                   auth_backend->newAuthSession);
    }

    mutex.lock ();
    Ref<ClientEntry> const client_entry = getClientEntry (path, &path_tail, &root_namespace);

    Ref<ClientSession> const client_session = grab (new (std::nothrow) ClientSession);
    client_session->weak_rtmp_conn = conn;
    client_session->unsafe_rtmp_conn = conn;
    client_session->client_addr = client_addr;
    client_session->auth_session = auth_session;
    client_session->processing_connected_event = true;

    client_session_list.append (client_session);
    client_session->ref ();
    mutex.unlock ();

    logD (session, _func, "client_session refcount before: ", client_session->getRefCount());
    if (client_entry)
	client_entry->fireClientConnected (client_session, path_tail, path);

    client_session->mutex.lock ();
    client_session->processing_connected_event = false;
    if (client_session->disconnected) {
	client_session->event_informer.informAll_unlocked (client_session->informClientDisconnected, NULL /* inform_data */);
	client_session->mutex.unlock ();
	return NULL;
    }
    client_session->mutex.unlock ();

    logD (session, _func, "client_session refcount after: ", client_session->getRefCount());
    return client_session;
}

void
MomentServer::clientDisconnected (ClientSession * const mt_nonnull client_session)
{
    logD (session, _func, "client_session refcount before: ", client_session->getRefCount());

    client_session->mutex.lock ();

    client_session->disconnected = true;
    // To avoid races, we defer invocation of "client disconnected" callbacks
    // until all "client connected" callbacks are called.
    if (!client_session->processing_connected_event) {
	client_session->event_informer.informAll_unlocked (
                ClientSession::informClientDisconnected, NULL /* inform_data */);
    }

    Ref<String> const auth_key    = client_session->auth_key;
    Ref<String> const stream_name = client_session->stream_name;

    client_session->mutex.unlock ();

    mutex.lock ();
    if (client_session->video_stream_key)
	removeVideoStream_unlocked (client_session->video_stream_key);

    client_session_list.remove (client_session);

    Cb<AuthBackend> const tmp_auth_backend = auth_backend;
    mutex.unlock ();

    if (auth_key && tmp_auth_backend) {
        if (!tmp_auth_backend.call (tmp_auth_backend->authSessionDisconnected,
                                    /*(*/
                                        client_session->auth_session,
                                        (auth_key ? auth_key->mem() : ConstMemory()),
                                        client_session->client_addr,
                                        (stream_name ? stream_name->mem() : ConstMemory())
                                    /*)*/))
        {
            logW_ ("authorization backend gone");
            goto _return;
        }
    }

_return:
    logD (session, _func, "client_session refcount after: ", client_session->getRefCount());
    client_session->unref ();
}

void
MomentServer::disconnect (ClientSession * const mt_nonnull client_session)
{
    CodeDepRef<RtmpConnection> const rtmp_conn = client_session->getRtmpConnection();
    if (!rtmp_conn)
	return;

    rtmp_conn->closeAfterFlush ();
}


// ______________________________ startWatching() ______________________________

namespace {
struct StartWatching_Data : public Referenced
{
    WeakRef<MomentServer> weak_moment;

    Ref<String> stream_name;
    Ref<String> stream_name_with_params;
    Ref<String> auth_key;
    IpAddress client_addr;

    Ref<VideoStream> video_stream;

    Cb<MomentServer::StartWatchingCallback> cb;
};
}

static void startWatching_completeNotFound (StartWatching_Data * const data,
                                            bool                 const call_cb = true)
{
    logA_ ("moment NOT_FOUND ", data->client_addr, " watch ", data->stream_name_with_params);
    if (call_cb)
        data->cb.call_ ((VideoStream*) NULL);
}

static void startWatching_completeOk (StartWatching_Data * const data,
                                      ConstMemory          const restream_reply,
                                      bool                 const call_cb,
                                      Ref<VideoStream>   * const ret_stream)
{
    if (!data->video_stream) {
        if (!restream_reply.len())
            return startWatching_completeNotFound (data, call_cb);

        {
            Ref<MomentServer> const self = data->weak_moment.getRef();
            if (!self)
                return startWatching_completeNotFound (data, call_cb);

            data->video_stream = self->startRestreaming (data->stream_name->mem(), restream_reply);
            if (!data->video_stream)
                return startWatching_completeNotFound (data, call_cb);

            if (ret_stream)
                *ret_stream = data->video_stream;
        }
    } else {
        Ref<MomentServer> const self = data->weak_moment.getRef();
        if (!self)
            return startWatching_completeNotFound (data, call_cb);

        self->mutex.lock ();
        ++data->video_stream->moment_data.use_count;
        self->mutex.unlock ();
    }

    logA_ ("moment OK ", data->client_addr, " watch ", data->stream_name_with_params);

    if (call_cb)
        data->cb.call_ (data->video_stream);
}

static void startWatching_completeDenied (StartWatching_Data * const data,
                                          bool                 const call_cb = true)
{
    logA_ ("moment DENIED ", data->client_addr, " watch ", data->stream_name_with_params);
    if (call_cb)
        data->cb.call_ ((VideoStream*) NULL);
}

static void startWatching_completeGone (StartWatching_Data * const data,
                                        bool                 const call_cb = true)
{
    logA_ ("moment GONE ", data->client_addr, " watch ", data->stream_name_with_params);
    if (call_cb)
        data->cb.call_ ((VideoStream*) NULL);
}

static void startWatching_startWatchingRet (VideoStream *video_stream,
                                            void        *_data);

static bool startWatching_checkAuthorization (bool                       restream,
                                              StartWatching_Data        * mt_nonnull data,
                                              MomentServer              * mt_nonnull moment,
                                              MomentServer::AuthSession * mt_nonnull auth_session,
                                              bool                      * mt_nonnull ret_authorized,
                                              StRef<String>             * mt_nonnull ret_restream_reply);

bool
MomentServer::startWatching (ClientSession    * const mt_nonnull client_session,
                             ConstMemory        const stream_name,
                             ConstMemory        const stream_name_with_params,
                             ConstMemory        const auth_key,
                             CbDesc<StartWatchingCallback> const &cb,
                             Ref<VideoStream> * const mt_nonnull ret_video_stream)
{
    *ret_video_stream = NULL;

    client_session->mutex.lock ();
    if (client_session->auth_key && !equal (client_session->auth_key->mem(), auth_key))
        logW_ (_func, "WARNING: Different auth keys for the same client session");

    client_session->auth_key    = grab (new (std::nothrow) String (auth_key));
    client_session->stream_name = grab (new (std::nothrow) String (stream_name));
    client_session->mutex.unlock ();

    Ref<VideoStream> video_stream;

    Ref<StartWatching_Data> const data = grab (new (std::nothrow) StartWatching_Data);
    data->weak_moment = this;
    data->stream_name = grab (new (std::nothrow) String (stream_name));
    data->stream_name_with_params = grab (new (std::nothrow) String (stream_name_with_params));
    data->client_addr = client_session->client_addr;
    data->cb = cb;

    if (auth_key.len() > 0)
        data->auth_key = grab (new (std::nothrow) String (auth_key));

    if (client_session->backend
        && client_session->backend->startWatching)
    {
        logD (session, _func, "calling backend->startWatching()");

        bool complete = false;
        if (!client_session->backend.call_ret<bool> (
                    &complete,
                    client_session->backend->startWatching,
                    /*(*/
                        stream_name,
                        stream_name_with_params,
                        client_session->client_addr,
                        CbDesc<StartWatchingCallback> (startWatching_startWatchingRet,
                                                       data,
                                                       NULL,
                                                       data),
                        &video_stream
                    /*)*/))
        {
            startWatching_completeGone (data, false /* call_cb */);
            *ret_video_stream = NULL;
            return true;
        }

        if (!complete)
            return false;

        if (!video_stream) {
            startWatching_completeNotFound (data, false /* call_cb */);
            *ret_video_stream = NULL;
            return true;
        }

        data->video_stream = video_stream;
        startWatching_completeOk (data,
                                  ConstMemory() /* restream_reply */,
                                  false         /* call_cb */,
                                  NULL          /* ret_stream */);

        *ret_video_stream = video_stream;
        return true;
    }

    logD (session, _func, "default path");

    video_stream = getVideoStream (stream_name);
    if (!video_stream && !enable_restreaming) {
        startWatching_completeNotFound (data, false /* call_cb */);
        *ret_video_stream = NULL;
        return true;
    }

    data->video_stream = video_stream;

    bool authorized = false;
    StRef<String> restream_reply;
    if (!startWatching_checkAuthorization (!video_stream /* restream */,
                                           data,
                                           this,
                                           client_session->auth_session,
                                           &authorized,
                                           &restream_reply))
    {
        return false;
    }

    if (!authorized) {
        startWatching_completeDenied (data, false /* call_cb */);
        *ret_video_stream = NULL;
        return true;
    }

    startWatching_completeOk (data,
                              (restream_reply ? restream_reply->mem() : ConstMemory()),
                              false /* call_cb */,
                              &video_stream);

    *ret_video_stream = video_stream;
    return true;
}

static void startWatching_startWatchingRet (VideoStream * const video_stream,
                                            void        * const _data)
{
    StartWatching_Data * const data = static_cast <StartWatching_Data*> (_data);
    data->video_stream = video_stream;

    if (!video_stream) {
        startWatching_completeNotFound (data);
        return;
    }

    startWatching_completeOk (data,
                              ConstMemory() /* restream_reply */,
                              true          /* call_cb */,
                              NULL          /* ret_stream */);
}

static void startWatching_checkAuthorizationRet (bool         authorized,
                                                 ConstMemory  reply_str,
                                                 void        *_data);

static bool startWatching_checkAuthorization (bool                        const restream,
                                              StartWatching_Data        * const mt_nonnull data,
                                              MomentServer              * const mt_nonnull moment,
                                              MomentServer::AuthSession * const mt_nonnull auth_session,
                                              bool                      * const mt_nonnull ret_authorized,
                                              StRef<String>             * const mt_nonnull ret_restream_reply)
{
    *ret_authorized = false;
    *ret_restream_reply = NULL;

    bool const complete =
            moment->checkAuthorization (
                    auth_session,
                    (restream ? MomentServer::AuthAction_WatchRestream : MomentServer::AuthAction_Watch),
                    data->stream_name->mem(),
                    (data->auth_key ? data->auth_key->mem() : ConstMemory()),
                    data->client_addr,
                    CbDesc<MomentServer::CheckAuthorizationCallback> (
                            startWatching_checkAuthorizationRet,
                            data,
                            NULL,
                            data),
                    ret_authorized,
                    ret_restream_reply);
    if (!complete)
        return false;

    return true;
}

static void startWatching_checkAuthorizationRet (bool          const authorized,
                                                 ConstMemory   const reply_str,
                                                 void        * const _data)
{
    StartWatching_Data * const data = static_cast <StartWatching_Data*> (_data);

    if (!authorized) {
        startWatching_completeDenied (data);
        return;
    }

    startWatching_completeOk (data, reply_str, true /* call_cb */, NULL /* ret_stream */);
}


// _____________________________ startStreaming() ______________________________

Result MomentServer::setClientSessionVideoStream (ClientSession * const client_session,
                                                  VideoStream   * const video_stream,
                                                  ConstMemory     const stream_name)
{
    client_session->mutex.lock ();
    // Checking 'disconnected' first to avoid calling addVideoStream
    // for streams from disconnected clients, which is a likely thing
    // to do if we add before checking. If we check first, then the
    // likelihood is much smaller.
    if (client_session->disconnected) {
        client_session->mutex.unlock ();
        return Result::Failure;
    }
    client_session->mutex.unlock ();

    VideoStreamKey const video_stream_key = addVideoStream (video_stream, stream_name);

    client_session->mutex.lock ();
    if (client_session->disconnected) {
	client_session->mutex.unlock ();

	removeVideoStream (video_stream_key);
        return Result::Failure;
    }
    client_session->video_stream_key = video_stream_key;
    client_session->mutex.unlock ();

    return Result::Success;
}

namespace {
struct StartStreaming_Data : public Referenced
{
    WeakRef<MomentServer> weak_moment;
    WeakRef<MomentServer::ClientSession> weak_client_session;

    Ref<VideoStream> video_stream;

    Ref<String> stream_name;
    Ref<String> auth_key;
    IpAddress client_addr;

    Cb<MomentServer::StartStreamingCallback> cb;
};
}

static void startStreaming_completeOk (StartStreaming_Data * const data,
                                       bool                  const call_cb = true)
{
    logA_ ("moment OK ", data->client_addr, " stream ", data->stream_name);
    if (call_cb)
        data->cb.call_ (Result::Success);
}

static void startStreaming_completeDenied (StartStreaming_Data * const data,
                                           bool                  const call_cb = true)
{
    logA_ ("moment DENIED ", data->client_addr, " stream ", data->stream_name);
    if (call_cb)
        data->cb.call_ (Result::Failure);
}

static void startStreaming_completeGone (StartStreaming_Data * const data,
                                         bool                  const call_cb = true)
{
    logA_ ("moment GONE ", data->client_addr, " stream ", data->stream_name);
    if (call_cb)
        data->cb.call_ (Result::Failure);
}

static void startStreaming_startStreamingRet (Result  res,
                                              void   *_data);

static bool startStreaming_checkAuthorization (StartStreaming_Data       *data,
                                               MomentServer              *moment,
                                               MomentServer::AuthSession *auth_session,
                                               bool                      *ret_authorized);

bool
MomentServer::startStreaming (ClientSession    * const mt_nonnull client_session,
			      ConstMemory        const stream_name,
                              ConstMemory        const auth_key,
                              VideoStream      * const mt_nonnull video_stream,
			      RecordingMode      const rec_mode,
                              CbDesc<StartStreamingCallback> const &cb,
                              Result           * const mt_nonnull ret_res)
{
    *ret_res = Result::Failure;

    client_session->mutex.lock ();
    if (client_session->auth_key && !equal (client_session->auth_key->mem(), auth_key))
        logW_ (_func, "WARNING: Different auth keys for the same client session");

    client_session->auth_key    = grab (new (std::nothrow) String (auth_key));
    client_session->stream_name = grab (new (std::nothrow) String (stream_name));
    client_session->mutex.unlock ();

    Ref<StartStreaming_Data> const data = grab (new (std::nothrow) StartStreaming_Data);
    data->weak_moment = this;
    data->weak_client_session = client_session;
    data->video_stream = video_stream;
    data->stream_name = grab (new (std::nothrow) String (stream_name));
    data->client_addr = client_session->client_addr;
    data->cb = cb;

    if (auth_key.len() > 0)
        data->auth_key = grab (new (std::nothrow) String (auth_key));

    if (client_session->backend
	&& client_session->backend->startStreaming)
    {
	logD (session, _func, "calling backend->startStreaming()");

        Result res;
        bool complete = false;
	if (!client_session->backend.call_ret<bool> (
                    &complete,
                    client_session->backend->startStreaming,
                    /*(*/
                        stream_name,
                        client_session->client_addr,
                        video_stream,
                        rec_mode,
                        CbDesc<StartStreamingCallback> (startStreaming_startStreamingRet,
                                                        data,
                                                        NULL,
                                                        data),
                        &res
                    /*)*/))
	{
            startStreaming_completeGone (data, false /* call_cb */);
            *ret_res = Result::Failure;
            return true;
	}

        if (!complete)
            return false;

        if (!res) {
            startStreaming_completeDenied (data, false /* call_cb */);
            *ret_res = Result::Failure;
            return true;
        }

        startStreaming_completeOk (data, false /* call_cb */);
        *ret_res = Result::Success;
        return true;
    }

    logD (session, _func, "default path");

    if (!publish_all_streams) {
        startStreaming_completeDenied (data, false /* call_cb */);
        *ret_res = Result::Failure;
        return true;
    }

    bool authorized = false;
    if (!startStreaming_checkAuthorization (data, this, client_session->auth_session, &authorized))
        return false;

    if (!authorized) {
        startStreaming_completeDenied (data, false /* call_cb */);
        *ret_res = Result::Failure;
        return true;
    }

    if (!setClientSessionVideoStream (client_session,
                                      video_stream,
                                      stream_name))
    {
        startStreaming_completeGone (data, false /* call_cb */);
        *ret_res = Result::Failure;
        return true;
    }

    startStreaming_completeOk (data, false /* call_cb */);
    *ret_res = Result::Success;
    return true;
}

static void startStreaming_startStreamingRet (Result   const res,
                                              void   * const _data)
{
    StartStreaming_Data * const data = static_cast <StartStreaming_Data*> (_data);

    if (!res) {
        startStreaming_completeDenied (data);
        return;
    }

    startStreaming_completeOk (data);
}

static void startStreaming_checkAuthorizationRet (bool         authorized,
                                                  ConstMemory  reply_str,
                                                  void        *_data);

static bool startStreaming_checkAuthorization (StartStreaming_Data       * const data,
                                               MomentServer              * const moment,
                                               MomentServer::AuthSession * const auth_session,
                                               bool                      * const ret_authorized)
{
    *ret_authorized = false;

    bool authorized = false;
    StRef<String> reply_str;
    bool const complete =
            moment->checkAuthorization (
                    auth_session,
                    MomentServer::AuthAction_Stream,
                    data->stream_name->mem(),
                    (data->auth_key ? data->auth_key->mem() : ConstMemory()),
                    data->client_addr,
                    CbDesc<MomentServer::CheckAuthorizationCallback> (startStreaming_checkAuthorizationRet,
                                                                      data,
                                                                      NULL,
                                                                      data),
                    &authorized,
                    &reply_str);
    if (!complete)
        return false;

    if (!authorized) {
        *ret_authorized = false;
        return true;
    }

    *ret_authorized = true;
    return true;
}

static void startStreaming_checkAuthorizationRet (bool          const authorized,
                                                  ConstMemory   const /* reply_str */,
                                                  void        * const _data)
{
    StartStreaming_Data * const data = static_cast <StartStreaming_Data*> (_data);

    if (!authorized) {
        startStreaming_completeDenied (data);
        return;
    }

    Ref<MomentServer> const moment = data->weak_moment.getRef ();
    Ref<MomentServer::ClientSession> const client_session = data->weak_client_session.getRef ();

    if (!moment || !client_session) {
        startStreaming_completeGone (data);
        return;
    }

    if (!moment->setClientSessionVideoStream (client_session,
                                              data->video_stream,
                                              data->stream_name->mem()))
    {
        startStreaming_completeGone (data);
        return;
    }

    startStreaming_completeOk (data);
}

// _____________________________________________________________________________


void
MomentServer::decStreamUseCount (VideoStream * const stream)
{
    logD_ (_func, "stream 0x", fmt_hex, (UintPtr) stream);

    if (!stream)
        return;

    VideoStream::MomentServerData * const stream_data = &stream->moment_data;

    mutex.lock ();
    if (!stream_data->stream_info) {
        mutex.unlock ();
        return;
    }

    assert (stream_data->use_count > 0);
    --stream_data->use_count;

    if (stream_data->use_count == 0) {
        StreamInfo * const stream_info = static_cast <StreamInfo*> (stream_data->stream_info.ptr());
        if (RestreamInfo * const restream_info = stream_info->restream_info) {
            mt_unlocks (mutex) stopRestreaming (restream_info);
            return;
        }
    }

    mutex.unlock ();
}

mt_mutex (mutex) MomentServer::ClientHandlerKey
MomentServer::addClientHandler_rec (CbDesc<ClientHandler> const &cb,
				    ConstMemory const           &path_,
				    Namespace                   * const nsp)
{
  // This code is pretty much like M::HttpService::addHttpHandler_rec()

    ConstMemory path = path_;
    if (path.len() > 0 && path.mem() [0] == '/')
	path = path.region (1);

    Byte const *delim = (Byte const *) memchr (path.mem(), '/', path.len());
    if (!delim) {
	ClientEntry *client_entry;
	{
	    Namespace::ClientEntryHash::EntryKey client_entry_key = nsp->client_entry_hash.lookup (path);
	    if (!client_entry_key) {
		Ref<ClientEntry> const new_entry = grab (new (std::nothrow) ClientEntry);
		new_entry->parent_nsp = nsp;
		new_entry->client_entry_key = nsp->client_entry_hash.add (path, new_entry);
		client_entry = new_entry;
	    } else {
		client_entry = client_entry_key.getData();
	    }
	}

	ClientHandlerKey client_handler_key;
	client_handler_key.client_entry = client_entry;
	client_handler_key.sbn_key = client_entry->event_informer.subscribe (cb);
	return client_handler_key;
    }

    ConstMemory const next_nsp_name = path.region (0, delim - path.mem());
    Namespace::NamespaceHash::EntryKey const next_nsp_key =
	    nsp->namespace_hash.lookup (next_nsp_name);
    Namespace *next_nsp;
    if (next_nsp_key) {
	next_nsp = next_nsp_key.getData();
    } else {
	Ref<Namespace> const new_nsp = grab (new (std::nothrow) Namespace);
	new_nsp->parent_nsp = nsp;
	new_nsp->namespace_hash_key = nsp->namespace_hash.add (next_nsp_name, new_nsp);
	next_nsp = new_nsp;
    }

    return addClientHandler_rec (cb, path.region (delim - path.mem() + 1), next_nsp);
}

MomentServer::ClientHandlerKey
MomentServer::addClientHandler (CbDesc<ClientHandler> const &cb,
				ConstMemory           const &path)
{
    mutex.lock ();
    ClientHandlerKey const client_handler_key = addClientHandler_rec (cb, path, &root_namespace);
    mutex.unlock ();
    return client_handler_key;
}

void
MomentServer::removeClientHandler (ClientHandlerKey const client_handler_key)
{
    mutex.lock ();

    Ref<ClientEntry> const client_entry = client_handler_key.client_entry;

    bool remove_client_entry = false;
    client_entry->mutex.lock ();
    client_entry->event_informer.unsubscribe_unlocked (client_handler_key.sbn_key);
//#warning This condition looks strange.
    if (!client_entry->event_informer.gotSubscriptions_unlocked ()) {
	remove_client_entry = true;
    }
    client_entry->mutex.unlock ();

    if (remove_client_entry) {
	client_entry->parent_nsp->client_entry_hash.remove (client_entry->client_entry_key);

	{
	    Namespace *nsp = client_entry->parent_nsp;
	    while (nsp && nsp->parent_nsp) {
		if (nsp->client_entry_hash.isEmpty() &&
		    nsp->namespace_hash.isEmpty())
		{
		    Namespace * const tmp_nsp = nsp;
		    nsp = nsp->parent_nsp;
		    nsp->namespace_hash.remove (tmp_nsp->namespace_hash_key);
		}
	    }
	}
    }

    mutex.unlock ();
}

Ref<VideoStream>
MomentServer::getVideoStream (ConstMemory const path)
{
  StateMutexLock l (&mutex);
    return getVideoStream_unlocked (path);
}

mt_mutex (mutex) Ref<VideoStream>
MomentServer::getVideoStream_unlocked (ConstMemory const path)
{
    VideoStreamHash::EntryKey const entry_key = video_stream_hash.lookup (path);
    if (!entry_key)
	return NULL;

    VideoStreamEntry * const stream_entry = (*entry_key.getDataPtr())->stream_list.getFirst();
    assert (stream_entry);

    if (stream_entry->displaced)
        return NULL;

    return stream_entry->video_stream;
}

mt_mutex (mutex) MomentServer::VideoStreamKey
MomentServer::addVideoStream_unlocked (VideoStream * const stream,
                                       ConstMemory   const path)
{
    logD_ (_func, "name: ", path, ", stream 0x", fmt_hex, (UintPtr) stream);

    VideoStreamEntry * const stream_entry = new (std::nothrow) VideoStreamEntry (stream);
    assert (stream_entry);

    {
        VideoStreamHash::EntryKey const entry_key = video_stream_hash.lookup (path);
        if (entry_key) {
            stream_entry->entry_key = entry_key;

            StRef<StreamHashEntry> * const hash_entry = entry_key.getDataPtr();
            assert (!(*hash_entry)->stream_list.isEmpty());
            if (new_streams_on_top) {
                (*hash_entry)->stream_list.getFirst()->displaced = true;
                notifyDeferred_StreamClosed ((*hash_entry)->stream_list.getFirst()->video_stream);
                (*hash_entry)->stream_list.prepend (stream_entry);
            } else
                (*hash_entry)->stream_list.append (stream_entry);
        } else {
            StRef<StreamHashEntry> const hash_entry = st_grab (new (std::nothrow) StreamHashEntry);
            hash_entry->stream_list.append (stream_entry);

            stream_entry->entry_key = video_stream_hash.add (path, hash_entry);
        }
    }

    notifyDeferred_VideoStreamAdded (stream, path);
    return VideoStreamKey (stream_entry);
}

MomentServer::VideoStreamKey
MomentServer::addVideoStream (VideoStream * const stream,
			      ConstMemory   const path)
{
    mutex.lock ();
    MomentServer::VideoStreamKey const stream_key = addVideoStream_unlocked (stream, path);
    mutex.unlock ();
    return stream_key;
}

mt_mutex (mutex) void
MomentServer::removeVideoStream_unlocked (VideoStreamKey const vs_key)
{
    logD_ (_func, "name: ", vs_key.stream_entry->entry_key.getKey(), ", "
           "stream 0x", fmt_hex, (UintPtr) vs_key.stream_entry->video_stream.ptr());

    VideoStreamHash::EntryKey const hash_key = vs_key.stream_entry->entry_key;
    StreamHashEntry * const hash_entry = *hash_key.getDataPtr();
    hash_entry->stream_list.remove (vs_key.stream_entry);
    if (hash_entry->stream_list.isEmpty()) {
        logD_ (_func, "last stream ", hash_key.getKey());
        video_stream_hash.remove (hash_key);
    }
}

void
MomentServer::removeVideoStream (VideoStreamKey const video_stream_key)
{
    mutex.lock ();
    removeVideoStream_unlocked (video_stream_key);
    mutex.unlock ();
}

Ref<VideoStream>
MomentServer::getMixVideoStream ()
{
    return mix_video_stream;
}

FetchConnection::Frontend const MomentServer::restream__fetch_conn_frontend = {
    restreamFetchConnDisconnected
};

void
MomentServer::restreamFetchConnDisconnected (bool             * const mt_nonnull ret_reconnect,
                                             Time             * const mt_nonnull /* ret_reconnect_after_millisec */,
                                             Ref<VideoStream> * const mt_nonnull /* ret_new_stream */,
                                             void             * const _restream_info)
{
    logD_ (_func_);

    *ret_reconnect = false;

    RestreamInfo * const restream_info = static_cast <RestreamInfo*> (_restream_info);
    Ref<MomentServer> const self = restream_info->weak_moment.getRef ();
    if (!self)
        return;

    self->mutex.lock ();
    mt_unlocks (mutex) self->stopRestreaming (restream_info);
}

Ref<VideoStream>
MomentServer::startRestreaming (ConstMemory const stream_name,
                                ConstMemory const uri)
{
    logD_ (_func, "stream_name: ", stream_name, ", uri: ", uri);

    Ref<FetchProtocol> const fetch_proto = getFetchProtocolForUri (uri);
    if (!fetch_proto) {
        logE_ (_func, "Could not get fetch protocol for uri: ", uri);
        return NULL;
    }

    mutex.lock ();

    Ref<VideoStream> stream = getVideoStream_unlocked (stream_name);
    if (stream) {
        logD_ (_func, "Stream \"", stream_name, "\" already exists");
        ++stream->moment_data.use_count;
        mutex.unlock ();
        return stream;
    }

    stream = grab (new (std::nothrow) VideoStream);
    logD_ (_func, "Created stream 0x", fmt_hex, (UintPtr) stream.ptr(), ", "
           "stream_name: ", stream_name);

    VideoStream::MomentServerData * const stream_data = &stream->moment_data;
    stream_data->use_count = 1;
    stream_data->stream_info = grab (new (std::nothrow) StreamInfo);
    StreamInfo * const stream_info = static_cast <StreamInfo*> (stream_data->stream_info.ptr());
    stream_info->restream_info = grab (new (std::nothrow) RestreamInfo);
    stream_info->restream_info->weak_moment = this;
    stream_info->restream_info->unsafe_stream = stream;
    stream_info->restream_info->stream_key = addVideoStream_unlocked (stream, stream_name);
    stream_info->restream_info->fetch_conn =
            fetch_proto->connect (stream,
                                  uri,
                                  CbDesc<FetchConnection::Frontend> (
                                          &restream__fetch_conn_frontend,
                                          stream_info->restream_info,
                                          stream_info->restream_info));
    mutex.unlock ();

    return stream;
}

mt_unlocks (mutex) void
MomentServer::stopRestreaming (RestreamInfo * const mt_nonnull restream_info)
{
    logD_ (_func_);

    Ref<VideoStream> stream;

    if (restream_info->stream_key) {
        stream = restream_info->unsafe_stream;
        removeVideoStream_unlocked (restream_info->stream_key);
        restream_info->stream_key = VideoStreamKey();
    }
    restream_info->fetch_conn = NULL;

    if (stream && stream->moment_data.stream_info) {
        if (StreamInfo * const stream_info =
                    static_cast <StreamInfo*> (stream->moment_data.stream_info.ptr()))
        {
            stream_info->restream_info = NULL;
        }
    }

    mutex.unlock ();

    if (stream)
        stream->close ();
}

namespace {
    class InformPageRequest_Data {
    public:
	MomentServer::PageRequest * const page_req;
	ConstMemory const path;
	ConstMemory const full_path;

	MomentServer::PageRequestResult result;

	InformPageRequest_Data (MomentServer::PageRequest * const page_req,
				ConstMemory const path,
				ConstMemory const full_path)
	    : page_req (page_req),
	      path (path),
	      full_path (full_path),
	      result (MomentServer::PageRequestResult::Success)
	{
	}
    };
}

void
MomentServer::PageRequestHandlerEntry::informPageRequest (PageRequestHandler * const handler,
							  void               * const cb_data,
							  void               * const _inform_data)
{
    InformPageRequest_Data * const inform_data =
	    static_cast <InformPageRequest_Data*> (_inform_data);

    assert (handler->pageRequest);
    PageRequestResult const res = handler->pageRequest (inform_data->page_req,
							inform_data->path,
							inform_data->full_path,
							cb_data);
    if (inform_data->result == PageRequestResult::Success)
	inform_data->result = res;
}

MomentServer::PageRequestResult
MomentServer::PageRequestHandlerEntry::firePageRequest (PageRequest * const page_req,
							ConstMemory   const path,
							ConstMemory   const full_path)
{
    InformPageRequest_Data inform_data (page_req, path, full_path);
    event_informer.informAll (informPageRequest, &inform_data);
    return inform_data.result;
}

MomentServer::PageRequestHandlerKey
MomentServer::addPageRequestHandler (CbDesc<PageRequestHandler> const &cb,
				     ConstMemory path)
{
    PageRequestHandlerEntry *handler_entry;
    GenericInformer::SubscriptionKey sbn_key;

    mutex.lock ();

    PageRequestHandlerHash::EntryKey const hash_key = page_handler_hash.lookup (path);
    if (hash_key) {
	handler_entry = hash_key.getData();
    } else {
	handler_entry = new (std::nothrow) PageRequestHandlerEntry;
	handler_entry->hash_key = page_handler_hash.add (path, handler_entry);
    }

    sbn_key = handler_entry->event_informer.subscribe (cb);

    ++handler_entry->num_handlers;

    mutex.unlock ();

    PageRequestHandlerKey handler_key;
    handler_key.handler_entry = handler_entry;
    handler_key.sbn_key = sbn_key;

    return handler_key;
}

void
MomentServer::removePageRequestHandler (PageRequestHandlerKey handler_key)
{
    PageRequestHandlerEntry * const handler_entry = handler_key.handler_entry;

    mutex.lock ();

    handler_entry->event_informer.unsubscribe (handler_key.sbn_key);
    --handler_entry->num_handlers;
    if (handler_entry->num_handlers == 0)
	page_handler_hash.remove (handler_entry->hash_key);

    mutex.unlock ();
}

MomentServer::PageRequestResult
MomentServer::processPageRequest (PageRequest * const page_req,
				  ConstMemory   const path)
{
    logD_ (_func, "path: ", path);

    mutex.lock ();

    PageRequestHandlerHash::EntryKey const hash_key = page_handler_hash.lookup (path);
    if (!hash_key) {
	mutex.unlock ();
        logD_ (_func, "no page handlers");
	return PageRequestResult::Success;
    }

    Ref<PageRequestHandlerEntry> const handler = hash_key.getData();

    mutex.unlock ();

    logD_ (_func, "firing page request");
    PageRequestResult const res = handler->firePageRequest (page_req,
							    path,
							    path /* full_path */);

    return res;
}

void
MomentServer::addPushProtocol (ConstMemory    const protocol_name,
                               PushProtocol * const mt_nonnull push_protocol)
{
    mutex.lock ();
    push_protocol_hash.add (protocol_name, push_protocol);
    mutex.unlock ();
}

void
MomentServer::addFetchProtocol (ConstMemory     const protocol_name,
                                FetchProtocol * const mt_nonnull fetch_protocol)
{
    mutex.lock ();
    fetch_protocol_hash.add (protocol_name, fetch_protocol);
    mutex.unlock ();
}

Ref<PushProtocol>
MomentServer::getPushProtocolForUri (ConstMemory const uri)
{
    logD_ (_func, uri);

    ConstMemory protocol_name;
    {
        Count i = 0;
        for (Count const i_end = uri.len(); i < i_end; ++i) {
            if (uri.mem() [i] == ':')
                break;
        }
        protocol_name = uri.region (0, i);
    }

    Ref<PushProtocol> push_protocol;
    {
        mutex.lock ();
        PushProtocolHash::EntryKey const push_protocol_key = push_protocol_hash.lookup (protocol_name);
        if (push_protocol_key) {
            push_protocol = push_protocol_key.getData();
        }
        mutex.unlock ();
    }

    if (!push_protocol) {
        logE_ (_func, "Push protocol not found: ", protocol_name);
        return NULL;
    }

    return push_protocol;
}

Ref<FetchProtocol>
MomentServer::getFetchProtocolForUri (ConstMemory const uri)
{
    ConstMemory protocol_name;
    {
        Count i = 0;
        for (Count const i_end = uri.len(); i < i_end; ++i) {
            if (uri.mem() [i] == ':')
                break;
        }
        protocol_name = uri.region (0, i);
    }

    Ref<FetchProtocol> fetch_protocol;
    {
        mutex.lock ();
        FetchProtocolHash::EntryKey const fetch_protocol_key = fetch_protocol_hash.lookup (protocol_name);
        if (fetch_protocol_key) {
            fetch_protocol = fetch_protocol_key.getData();
        }
        mutex.unlock ();
    }

    if (!fetch_protocol) {
        logE_ (_func, "Fetch protocol not found: ", protocol_name);
        return NULL;
    }

    return fetch_protocol;
}

Ref<MediaSource>
MomentServer::createMediaSource (CbDesc<MediaSource::Frontend> const &frontend,
                                 Timers            * const timers,
                                 DeferredProcessor * const deferred_processor,
                                 PagePool          * const page_pool,
                                 VideoStream       * const video_stream,
                                 VideoStream       * const mix_video_stream,
                                 Time                const initial_seek,
                                 ChannelOptions    * const channel_opts,
                                 PlaybackItem      * const playback_item)
{
    if (playback_item->spec_kind == PlaybackItem::SpecKind::Slave) {
        Ref<SlaveMediaSource> const slave = grab (new (std::nothrow) SlaveMediaSource);
        slave->init (this, playback_item->stream_spec->mem(), video_stream, frontend);
        return slave;
    }

    if (!media_source_provider)
        return NULL;

    return media_source_provider->createMediaSource (frontend,
                                                     timers,
                                                     deferred_processor,
                                                     page_pool,
                                                     video_stream,
                                                     mix_video_stream,
                                                     initial_seek,
                                                     channel_opts,
                                                     playback_item);
}

bool
MomentServer::checkAuthorization (AuthSession   * const auth_session,
                                  AuthAction      const auth_action,
                                  ConstMemory     const stream_name,
                                  ConstMemory     const auth_key,
                                  IpAddress       const client_addr,
                                  CbDesc<CheckAuthorizationCallback> const &cb,
                                  bool          * const mt_nonnull ret_authorized,
                                  StRef<String> * const mt_nonnull ret_reply_str)
{
    *ret_authorized = false;

    mutex.lock ();
    Cb<AuthBackend> const tmp_auth_backend = auth_backend;
    mutex.unlock ();

    if (!tmp_auth_backend) {
        *ret_authorized = true;
        return true;
    }

    bool complete = false;
    bool authorized = false;
    if (!tmp_auth_backend.call_ret<bool> (&complete,
                                          tmp_auth_backend->checkAuthorization,
                                          /*(*/
                                              auth_session,
                                              auth_action,
                                              stream_name,
                                              auth_key,
                                              client_addr,
                                              cb,
                                              &authorized,
                                              ret_reply_str
                                          /*)*/))
    {
        logW_ ("authorization backend gone");
        *ret_authorized = true;
        return true;
    }

    if (!complete)
        return false;

    *ret_authorized = authorized;
    return true;
}

void
MomentServer::dumpStreamList ()
{
#if 0
// Deprecated

    log__ (_func_);

    {
      StateMutexLock l (&mutex);

	VideoStreamHash::iter iter (video_stream_hash);
	while (!video_stream_hash.iter_done (iter)) {
	    VideoStreamHash::EntryKey const entry = video_stream_hash.iter_next (iter);
	    log__ (_func, "    ", entry.getKey());
	}
    }

    log__ (_func_, "done");
#endif
}

mt_locks (mutex) void
MomentServer::lock ()
{
    mutex.lock ();
}

mt_unlocks (mutex) void
MomentServer::unlock ()
{
    mutex.unlock ();
}

Result
MomentServer::init (ServerApp        * const mt_nonnull server_app,
		    PagePool         * const mt_nonnull page_pool,
		    HttpService      * const mt_nonnull http_service,
		    HttpService      * const mt_nonnull admin_http_service,
		    MConfig::Config  * const mt_nonnull config,
		    ServerThreadPool * const mt_nonnull recorder_thread_pool,
                    ServerThreadPool * const mt_nonnull reader_thread_pool,
		    Storage          * const mt_nonnull storage,
                    ChannelManager   * const channel_manager)
{
    this->server_app           = server_app;
    this->page_pool            = page_pool;
    this->http_service         = http_service;
    this->admin_http_service   = admin_http_service;
    this->recorder_thread_pool = recorder_thread_pool;
    this->reader_thread_pool   = reader_thread_pool;
    this->storage              = storage;

    this->weak_channel_manager = channel_manager;

    this->config = config;
    parseDefaultVarlist (config);

    mix_video_stream = grab (new (std::nothrow) VideoStream);

    vs_inform_reg.setDeferredProcessor (server_app->getServerContext()->getMainThreadContext()->getDeferredProcessor());

    {
	ConstMemory const opt_name = "moment/publish_all";
	MConfig::BooleanValue const value = config->getBoolean (opt_name);
	if (value == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name),
		   ", assuming \"", publish_all_streams, "\"");
	} else {
	    if (value == MConfig::Boolean_False)
		publish_all_streams = false;
	    else
		publish_all_streams = true;

	    logD_ (_func, opt_name, ": ", publish_all_streams);
	}
    }

    {
        ConstMemory const opt_name = "moment/restreaming";
        MConfig::BooleanValue const value = config->getBoolean (opt_name);
        if (value == MConfig::Boolean_Invalid) {
            logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name),
                   ", assuming \"", enable_restreaming, "\"");
        } else {
            if (value == MConfig::Boolean_True)
                enable_restreaming = true;
            else
                enable_restreaming = false;

            logD_ (_func, opt_name, ": ", enable_restreaming);
        }
    }

    {
        ConstMemory const opt_name = "moment/new_streams_on_top";
        MConfig::BooleanValue const value = config->getBoolean (opt_name);
        if (value == MConfig::Boolean_Invalid) {
            logE_ (_func, "Invalid value for ", opt_name, ":", config->getString (opt_name),
                   ", assuming \"", new_streams_on_top, "\"");
        } else {
            if (value == MConfig::Boolean_True)
                new_streams_on_top = true;
            else
            if (value == MConfig::Boolean_False)
                new_streams_on_top = false;

            logD_ (_func, opt_name, ": ", new_streams_on_top);
        }
    }

    admin_http_service->addHttpHandler (
	    CbDesc<HttpService::HttpHandler> (&admin_http_handler, this, this),
	    "admin");

    http_service->addHttpHandler (
            CbDesc<HttpService::HttpHandler> (&server_http_handler, this, this),
            "server");

    if (!loadModules ())
	logE_ (_func, "Could not load modules");

    return Result::Success;
}

MomentServer::MomentServer ()
    : event_informer        (this /* coderef_container */, &mutex),
      video_stream_informer (this /* coderef_container */, &mutex),
      server_app            (NULL),
      page_pool             (NULL),
      http_service          (NULL),
      recorder_thread_pool  (NULL),
      reader_thread_pool    (NULL),
      storage               (NULL),
      publish_all_streams   (true),
      enable_restreaming    (false),
      new_streams_on_top    (true),
      config                (NULL),
      media_source_provider (this /* coderef_container */)
{
    instance = this;

    vs_added_inform_task.cb =
            CbDesc<DeferredProcessor::TaskCallback> (videoStreamAddedInformTask, this, this);
    vs_closed_inform_task.cb =
            CbDesc<DeferredProcessor::TaskCallback> (streamClosedInformTask, this, this);
}

MomentServer::~MomentServer ()
{
    logD_ (_func_);

    mutex.lock ();
    mutex.unlock ();

    vs_inform_reg.release ();

    fireDestroy ();

    if (server_app)
        server_app->release ();

    config_mutex.lock ();
    releaseDefaultVarHash ();
    config_mutex.unlock ();
}

}

