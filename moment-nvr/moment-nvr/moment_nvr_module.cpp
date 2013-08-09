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

#include <moment-nvr/moment_nvr_module.h>


using namespace M;
using namespace Moment;

namespace MomentNvr {

StRef<String> MomentNvrModule::recording_state_dir;

StRef<String>
MomentNvrModule::channelStateToJson (ChannelRecorder::ChannelState * const mt_nonnull channel_state,
                                     ConstMemory const seq)
{
    return st_makeString ("{ \"seq\": \"", seq, "\", "
                          "\"recording\": ", channel_state->recording, " }");
}

StRef<String>
MomentNvrModule::channelExistenceToJson(std::vector<std::pair<int,int>> * const mt_nonnull existence)
{
    std::ostringstream s;
    for(int i = 0; i < existence->size(); i++)
    {
        s << "{\n\"start\":";
        s << (*existence)[i].first;
        s << ",\n\"end\":";
        s << (*existence)[i].second;
        s << "\n}";
        if(i < existence->size() - 1)
            s << ",\n";
        else
            s << "\n";
    }
    return st_makeString("{\n"
                         "\"timings\" : [\n",
                         s.str().c_str(),
                         "]\n"
                         "}\n");
}

StRef<String>
MomentNvrModule::channelFilesExistenceToJson (
        std::vector<ChannelChecker::ChannelFileSummary> * const mt_nonnull files_existence)
{
     std::ostringstream s;
     for(int i = 0; i < files_existence->size(); i++)
     {
         s << "{\n\"path\":\"";
         s << (*files_existence)[i].path;
         s <<"\",\n\"start\":";
         s << (*files_existence)[i].start;
         s << ",\n\"end\":";
         s << (*files_existence)[i].end;
         s << "\n}";
         if(i < files_existence->size() - 1)
             s << ",\n";
         else
             s << "\n";
     }
     return st_makeString("{\n"
                          "\"filesSummary\" : [\n",
                          s.str().c_str(),
                          "]\n"
                          "}\n");
}

GetFileSession::Frontend const MomentNvrModule::get_file_session_frontend = {
    getFileSession_done
};

void
MomentNvrModule::getFileSession_done (Result   const res,
                                      void   * const _self)
{
    MomentNvrModule * const self = static_cast <MomentNvrModule*> (_self);

    logD_ (_func_);

    // TODO Destroy GetFileSession.
}

void
MomentNvrModule::doGetFile (HttpRequest * const mt_nonnull req,
                            Sender      * const mt_nonnull sender,
                            ConstMemory   const channel_name,
                            Time          const start_unixtime_sec,
                            Time          const duration_sec,
                            bool          const download)
{
    logD_ (_func, "channel: ", channel_name, ", "
           "start: ", start_unixtime_sec, ", "
           "duration: ", duration_sec);

    Ref<GetFileSession> const get_file_session = grab (new (std::nothrow) GetFileSession);
    {
        Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (record_dir->mem());
        get_file_session->init (moment,
                                req,
                                sender,
                                page_pool,
                                vfs,
                                channel_name,
                                start_unixtime_sec,
                                duration_sec,
                                download /* octet_stream_mime */,
                                CbDesc<GetFileSession::Frontend> (&get_file_session_frontend, this, this));
    }

    mutex.lock ();
#warning TODO GetFileSession lifetime
    get_file_sessions.append (get_file_session);
    mutex.unlock ();

    get_file_session->start ();
}

HttpService::HttpHandler const MomentNvrModule::http_handler =
{
    httpRequest,
    NULL /* httpMessageBody */
};

Result
MomentNvrModule::httpRequest (HttpRequest  * const mt_nonnull req,
                              Sender       * const mt_nonnull conn_sender,
                              Memory const & /* msg_body */,
                              void        ** const mt_nonnull /* ret_msg_data */,
                              void         * const _self)
{
    MomentNvrModule * const self = static_cast <MomentNvrModule*> (_self);

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
    } else
    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "channel_state"))
    {
        ConstMemory const channel_name = req->getParameter ("stream");
        ConstMemory const seq = req->getParameter ("seq");

        ChannelRecorder::ChannelState channel_state;
        ChannelRecorder::ChannelResult const res =
                self->channel_recorder->getChannelState (channel_name, &channel_state);
        if (res == ChannelRecorder::ChannelResult_ChannelNotFound) {
            ConstMemory const reply_body = "404 Channel Not Found (mod_nvr)";
            conn_sender->send (self->page_pool,
                               true /* do_flush */,
                               MOMENT_SERVER__404_HEADERS (reply_body.len()),
                               "\r\n",
                               reply_body);
            logA_ ("mod_nvr 404 ", req->getClientAddress(), " ", req->getRequestLine());
            goto _return;
        } else
        if (res == ChannelRecorder::ChannelResult_Failure) {
            ConstMemory const reply_body = "500 Internal Server Error (mod_nvr)";
            conn_sender->send (self->page_pool,
                               true /* do_flush */,
                               MOMENT_SERVER__404_HEADERS (reply_body.len()),
                               "\r\n",
                               reply_body);
            logA_ ("mod_nvr 500 ", req->getClientAddress(), " ", req->getRequestLine());
            goto _return;
        }
        assert (res == ChannelRecorder::ChannelResult_Success);

        StRef<String> const reply_body = channelStateToJson (&channel_state, seq);
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__OK_HEADERS ("text/html", reply_body->len()),
                           "\r\n",
                           reply_body->mem());

        logA_ ("mod_nvr 200 ", req->getClientAddress(), " ", req->getRequestLine());
    } else
    if (req->getNumPathElems() >= 2
        && (equal (req->getPath (1), "file") ||
            stringHasSuffix (req->getPath (1), ".mp4", NULL /* ret_str */)))
    {
        ConstMemory const channel_name = req->getParameter ("stream");

        Uint64 start_unixtime_sec = 0;
        if (!strToUint64_safe (req->getParameter ("start"), &start_unixtime_sec, 10 /* base */)) {
            logE_ (_func, "Bad \"start\" request parameter value");
            goto _bad_request;
        }

        Uint64 duration_sec = 0;
        if (!strToUint64_safe (req->getParameter ("duration"), &duration_sec, 10 /* base */)) {
            logE_ (_func, "Bad \"duration\" request parameter value");
            goto _bad_request;
        }

        bool const download = req->hasParameter ("download");
        self->doGetFile (req, conn_sender, channel_name, start_unixtime_sec, duration_sec, download);
        return Result::Success;
    } else
    if (req->getNumPathElems() >= 2 && (equal (req->getPath (1), "existence"))) {
        ConstMemory const channel_name = req->getParameter ("stream");
        // ConstMemory const seq = req->getParameter ("seq"); TODO doesn't know meaning of sequence, so omit it

        std::vector<std::pair<int,int>> * channel_existence = self->channel_checker->getChannelExistence (channel_name);

        if (channel_existence == NULL) {
            ConstMemory const reply_body = "404 Channel Not Found (mod_nvr)";
            conn_sender->send (self->page_pool,
                               true /* do_flush */,
                               MOMENT_SERVER__404_HEADERS (reply_body.len()),
                               "\r\n",
                               reply_body);
            logA_ ("mod_nvr 404 ", req->getClientAddress(), " ", req->getRequestLine());
            goto _return;
        }

        StRef<String> const reply_body = channelExistenceToJson (channel_existence);
        logD_ (_func, "reply: ", reply_body);
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__OK_HEADERS ("text/html", reply_body->len()),
                           "\r\n",
                           reply_body->mem());
    }else {
        logE_ (_func, "Unknown request: ", req->getFullPath());

        ConstMemory const reply_body = "404 Not Found (mod_nvr)";
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__404_HEADERS (reply_body.len()),
                           "\r\n",
                           reply_body);

        logA_ ("mod_nvr 404 ", req->getClientAddress(), " ", req->getRequestLine());
    }

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

HttpService::HttpHandler const MomentNvrModule::admin_http_handler =
{
    adminHttpRequest,
    NULL /* httpMessageBody */
};

Result
MomentNvrModule::adminHttpRequest (HttpRequest  * const mt_nonnull req,
                                   Sender       * const mt_nonnull conn_sender,
                                   Memory const & /* msg_body */,
                                   void        ** const mt_nonnull /* ret_msg_data */,
                                   void         * const _self)
{
    MomentNvrModule * const self = static_cast <MomentNvrModule*> (_self);

    logD_ (_func, req->getRequestLine());

    MOMENT_SERVER__HEADERS_DATE

    if (req->getNumPathElems() >= 2
        && (equal (req->getPath (1), "rec_on") ||
            equal (req->getPath (1), "rec_off")))
    {
        ConstMemory const channel_name = req->getParameter ("stream");
        ConstMemory const seq = req->getParameter ("seq");

        ChannelRecorder::ChannelState channel_state;

        bool const set_on = equal (req->getPath (1), "rec_on");

        if(!recording_state_dir->isNullString())
        {
            StRef<String> const path = st_makeString (recording_state_dir->mem(), "/", channel_name);

            bool file_exist = false;
            if (FILE *file = fopen(path->cstr(), "r")) {
                fclose(file);
                file_exist = true;
            }

            if(file_exist && set_on)
            {
                if( remove( path->cstr() ) != 0 )
                    logE_ (_func, "could not delete file", path->cstr());
                else
                    logD_ (_func, "removed file", path->cstr());
            }
            else if (!file_exist && !set_on)
            {
                FILE * fp = fopen( path->cstr(), "w");
                if(fp == NULL)
                    logE_ (_func, "could not create file [", path->cstr(),"]\n");
                else
                {
                    logD_ (_func, "created file [", path->cstr(),"]\n");
                    fclose(fp);
                }
            }
        }


        ChannelRecorder::ChannelResult res = self->channel_recorder->setRecording (channel_name, set_on);
        if (res == ChannelRecorder::ChannelResult_Success)
            res = self->channel_recorder->getChannelState (channel_name, &channel_state);

        if (res == ChannelRecorder::ChannelResult_ChannelNotFound) {
            ConstMemory const reply_body = "404 Channel Not Found (mod_nvr_admin)";
            conn_sender->send (self->page_pool,
                               true /* do_flush */,
                               MOMENT_SERVER__404_HEADERS (reply_body.len()),
                               "\r\n",
                               reply_body);
            logA_ ("mod_nvr_admin 404 ", req->getClientAddress(), " ", req->getRequestLine());
            goto _return;
        } else
        if (res == ChannelRecorder::ChannelResult_Failure) {
            ConstMemory const reply_body = "500 Internal Server Error (mod_nvr_admin)";
            conn_sender->send (self->page_pool,
                               true /* do_flush */,
                               MOMENT_SERVER__404_HEADERS (reply_body.len()),
                               "\r\n",
                               reply_body);
            logA_ ("mod_nvr_admin 500 ", req->getClientAddress(), " ", req->getRequestLine());
            goto _return;
        }
        assert (res == ChannelRecorder::ChannelResult_Success);

        StRef<String> const reply_body = channelStateToJson (&channel_state, seq);
        logD_ (_func, "reply: ", reply_body);
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__OK_HEADERS ("text/html", reply_body->len()),
                           "\r\n",
                           reply_body->mem());

        logA_ ("mod_nvr_admin 200 ", req->getClientAddress(), " ", req->getRequestLine());
    } else
    if(req->getNumPathElems() >= 2 &&
            (equal (req->getPath (1), "files_existence"))) {
        ConstMemory const channel_name = req->getParameter ("stream");
        std::vector<ChannelChecker::ChannelFileSummary> * files_existence =
            self->channel_checker->getChannelFilesExistence (channel_name);

        if (files_existence == NULL) {
            ConstMemory const reply_body = "404 Channel Not Found (mod_nvr)";
            conn_sender->send (self->page_pool,
                               true /* do_flush */,
                               MOMENT_SERVER__404_HEADERS (reply_body.len()),
                               "\r\n",
                               reply_body);
            logA_ ("mod_nvr 404 ", req->getClientAddress(), " ", req->getRequestLine());
            goto _return;
        }

        StRef<String> const reply_body = channelFilesExistenceToJson (files_existence);
        logD_ (_func, "reply: ", reply_body);
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__OK_HEADERS ("text/html", reply_body->len()),
                           "\r\n",
                           reply_body->mem());
    } else {
        logE_ (_func, "Unknown request: ", req->getFullPath());

        ConstMemory const reply_body = "404 Not Found (mod_nvr_admin)";
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__404_HEADERS (reply_body.len()),
                           "\r\n",
                           reply_body);

        logA_ ("mod_nvr 404 ", req->getClientAddress(), " ", req->getRequestLine());
    }

    goto _return;

#if 0
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
#endif

_return:
    if (!req->getKeepalive())
        conn_sender->closeAfterFlush ();

    return Result::Success;
}

mt_const void
MomentNvrModule::init (MomentServer * const mt_nonnull moment)
{
    this->moment = moment;

    Ref<MConfig::Config> const config = moment->getConfig ();

    ConstMemory record_dir_mem;
    {
        ConstMemory const opt_name = "mod_nvr/record_dir";
        bool record_dir_is_set = false;
        record_dir_mem = config->getString (opt_name, &record_dir_is_set);
        if (!record_dir_is_set) {
            logE_ (_func, opt_name, " config option is not set, disabling mod_nvr");
            return;
        }
        logI_ (_func, opt_name, ": ", record_dir_mem);
    }
    record_dir = st_grab (new (std::nothrow) String (record_dir_mem));

    ConstMemory recording_state_dir_mem;
    {
        ConstMemory const opt_name = "mod_nvr/recording_state_dir";
        bool recording_state_dir_is_set = false;
        recording_state_dir_mem = config->getString (opt_name, &recording_state_dir_is_set);
        if (!recording_state_dir_is_set) {
            logE_ (_func, "Invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        }
        else{
            logI_ (_func, opt_name, ": ", recording_state_dir_mem);
        }
    }
    recording_state_dir = st_grab (new (std::nothrow) String (recording_state_dir_mem));

    Uint64 file_duration_sec = 3600;
    {
        ConstMemory const opt_name = "mod_nvr/file_duration";
        MConfig::GetResult const res =
                config->getUint64_default (opt_name, &file_duration_sec, file_duration_sec);
        if (!res)
            logE_ (_func, "Invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        else
            logI_ (_func, opt_name, ": ", file_duration_sec);
    }

    Uint64 max_age_minutes = 120;
    {
        ConstMemory const opt_name = "mod_nvr/max_age";
        MConfig::GetResult const res =
                config->getUint64_default (opt_name, &max_age_minutes, max_age_minutes);
        if (!res)
            logE_ (_func, "Invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        else
            logI_ (_func, opt_name, ": ", max_age_minutes);
    }
    Uint64 max_age_sec = max_age_minutes * 60;

    Uint64 clean_interval_sec = 5;
    {
        ConstMemory const opt_name = "mod_nvr/clean_interval";
        MConfig::GetResult const res =
                config->getUint64_default (opt_name, &clean_interval_sec, clean_interval_sec);
        if (!res)
            logE_ (_func, "Invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        else
            logI_ (_func, opt_name, ": ", clean_interval_sec);
    }

    page_pool = moment->getPagePool();

    Ref<NamingScheme> const naming_scheme =
            grab (new (std::nothrow) DefaultNamingScheme (file_duration_sec));
    Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (record_dir_mem);

    MOMENT_NVR__INIT

    channel_recorder = grab (new (std::nothrow) ChannelRecorder);
    channel_recorder->init (moment, vfs, naming_scheme, max_age_sec, clean_interval_sec, recording_state_dir);

    media_viewer = grab (new (std::nothrow) MediaViewer);
    media_viewer->init (moment, vfs);

    channel_checker = grab (new (std::nothrow) ChannelChecker);
    channel_checker->init (vfs);

    moment->getHttpService()->addHttpHandler (
            CbDesc<HttpService::HttpHandler> (&http_handler, this, this),
            "mod_nvr",
            true /* preassembly */,
            1 << 20 /* 1 Mb */ /* preassembly_limit */,
            true /* parse_body_params */);

    moment->getAdminHttpService()->addHttpHandler (
            CbDesc<HttpService::HttpHandler> (&admin_http_handler, this, this),
            "mod_nvr_admin",
            true /* preassembly */,
            1 << 20 /* 1 Mb */ /* preassembly_limit */,
            true /* parse_body_params */);
}

MomentNvrModule::MomentNvrModule ()
    : page_pool (this /* coderef_container */)
{
}

}

