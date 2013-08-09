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


#include <libmary/libmary.h>

#include <cstdio>

#ifndef LIBMARY_PLATFORM_WIN32
#include <unistd.h>
#endif
#include <errno.h>

#ifdef MOMENT_GPERFTOOLS
#include <gperftools/profiler.h>
#endif

#include <moment/libmoment.h>
#include <moment/inc.h>


using namespace M;
using namespace Moment;


namespace {

struct Options {
    bool        help;
    bool        daemonize;
    Ref<String> config_filename;
    Ref<String> log_filename;
    LogLevel    loglevel;
    Uint64      exit_after;
    bool        gst_debug;
    bool        show_version;

    Options ()
	: help         (false),
	  daemonize    (false),
	  loglevel     (LogLevel::Info),
	  exit_after   ((Uint64) -1),
          gst_debug    (false),
          show_version (false)
    {}
};

Options options;

const Count default__page_pool__min_pages     = 512;
const Time  default__http__keepalive_timeout  =  60;

class MomentInstance : public Object
{
private:
    StateMutex mutex;
    Mutex config_reload_mutex;

    PagePool page_pool;

    ServerApp server_app;
    HttpService http_service;

    HttpService separate_admin_http_service;
    HttpService *admin_http_service_ptr;

    FixedThreadPool recorder_thread_pool;
    FixedThreadPool reader_thread_pool;

    LocalStorage storage;

    MomentServer moment_server;

    mt_mutex (mutex) Timers::TimerKey exit_timer;

    void doExit (ConstMemory reason);

    static void exitTimerTick (void *_self);

    void ctl_startProfiler (ConstMemory filename);
    void ctl_stopProfiler  ();

    void ctl_exit     (ConstMemory reason);
    void ctl_abort    (ConstMemory reason);
    void ctl_segfault (ConstMemory reason);

#ifndef LIBMARY_PLATFORM_WIN32
    LinePipe line_pipe;

  mt_iface (LinePipe::Frontend)
    static LinePipe::Frontend const ctl_pipe_frontend;

    static void ctl_line (ConstMemory  line,
                          void        *_self);
  mt_iface_end
#endif

  mt_iface (HttpService::HttpHandler)
    static HttpService::HttpHandler const ctl_http_handler;

    static Result ctlHttpRequest (HttpRequest   * mt_nonnull req,
                                  Sender        * mt_nonnull conn_sender,
                                  Memory const  & /* msg_body */,
                                  void         ** mt_nonnull /* ret_msg_data */,
                                  void          *_self);
  mt_iface_end

  mt_iface (MomentServer::Events)
    static MomentServer::Events const moment_server_events;

    static void configReload (MConfig::Config *new_config,
                              void            *_self);
  mt_iface_end


  // ____________________________ Config reloading _____________________________

    class MomentConfigParams : public Referenced
    {
    public:
        Uint64 min_pages;
        Uint64 num_threads;
        Uint64 num_file_threads;

        StRef<String> profile_filename;
        StRef<String> ctl_filename;
        Uint64 ctl_pipe_reopen_timeout;

        Uint64 http_keepalive_timeout;
        bool   no_keepalive_conns;

        IpAddress http_bind_addr;
        bool      http_bind_valid;

        IpAddress http_admin_bind_addr;
        bool      http_admin_bind_valid;
    };

    Ref<MomentConfigParams> cur_params;

    Result initiateConfigReload ();

    Ref<MConfig::Config> loadConfig ();

    void doConfigReload (MConfig::Config * mt_nonnull new_config);

    static Result processConfig (MConfig::Config    *config,
                                 MomentConfigParams *params);

    void applyConfigParams (MConfig::Config *new_config);

  // ___________________________________________________________________________


public:
    int run ();

    void exit (ConstMemory reason);

    MomentInstance ()
        : page_pool    (this /* coderef_container */, 4096 /* page_size */, default__page_pool__min_pages),
          server_app   (this /* coderef_container */),
          http_service (this /* coderef_container */),
          separate_admin_http_service (this /* coderef_container */),
          admin_http_service_ptr (&separate_admin_http_service),
          recorder_thread_pool (this /* coderef_container */),
          reader_thread_pool   (this /* coderef_container */),
          storage (this /* coderef_container */)
#ifndef LIBMARY_PLATFORM_WIN32
          , line_pipe    (this /* coderef_container */)
#endif
    {}
};

static void
printUsage ()
{
    outs->print ("Usage: moment [options]\n"
		  "Options:\n"
		  "  -c --config <config_file>  Configuration file to use (default: /opt/moment/moment.conf)\n"
		  "  -l --log <log_file>        Log file to use (default: /var/log/moment.log)\n"
		  "  --loglevel <loglevel>      Loglevel, one of A/S/D/I/W/E/H/F/N (default: I, \"Info\")\n"
#ifndef LIBMARY_PLATFORM_WIN32
		  "  -d --daemonize             Daemonize (run in the background as a daemon).\n"
#endif
		  "  --exit-after <number>      Exit after specified timeout in seconds.\n"
                  "  --version                  Output version information and exit.\n"
		  "  -h --help                  Show this help message.\n");
    outs->flush ();
}

static bool
cmdline_help (char const * /* short_name */,
	      char const * /* long_name */,
	      char const * /* value */,
	      void       * /* opt_data */,
	      void       * /* cb_data */)
{
    options.help = true;
    return true;
}

static bool
cmdline_daemonize (char const * /* short_name */,
		   char const * /* long_name */,
		   char const * /* value */,
		   void       * /* opt_data */,
		   void       * /* cb_data */)
{
    options.daemonize = true;
    return true;
}

static bool
cmdline_config (char const * /* short_name */,
		char const * /* long_name */,
		char const *value,
		void       * /* opt_data */,
		void       * /* cb_data */)
{
    options.config_filename = grab (new (std::nothrow) String (value));
    return true;
}

static bool
cmdline_log (char const * /* short_name */,
	     char const * /* long_name */,
	     char const *value,
	     void       * /* opt_data */,
	     void       * /* cb_data */)
{
    options.log_filename = grab (new (std::nothrow) String (value));
    return true;
}

static bool
cmdline_loglevel (char const * /* short_name */,
		  char const * /* long_name */,
		  char const *value,
		  void       * /* opt_data */,
		  void       * /* cb_data */)
{
    ConstMemory const value_mem = ConstMemory (value, value ? strlen (value) : 0);
    if (!LogLevel::fromString (value_mem, &options.loglevel)) {
	logE_ (_func, "Invalid loglevel name \"", value_mem, "\", using \"Info\"");
	options.loglevel = LogLevel::Info;
    }
    return true;
}

static bool
cmdline_exit_after (char const * /* short_name */,
		    char const * /* long_name */,
		    char const *value,
		    void       * /* opt_data */,
		    void       * /* cb_data */)
{
    if (!strToUint64_safe (value, &options.exit_after)) {
	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --exit-after (number expected): ", exc->toString());
	exit (EXIT_FAILURE);
    }

    logD_ (_func, "options.exit_after: ", options.exit_after);
    return true;
}

static bool
cmdline_gst_debug (char const * /* short_name */,
                   char const * /* long_name */,
                   char const * /* value */,
                   void       * /* opt_data */,
                   void       * /* cb_data */)
{
    options.gst_debug = true;
    return true;
}

static bool
cmdline_keyfile (char const * /* short_name */,
                 char const * /* long_name */,
                 char const * const value,
                 void       * const opt_data,
                 void       * /* cb_data */)
{
    if (opt_data) {
        StRef<String> * const str = static_cast < StRef<String>* > (opt_data);
        *str = st_grab (new (std::nothrow) String (value));
    }
    return true;
}

static bool
cmdline_version (char const * /* short_name */,
                 char const * /* long_name */,
                 char const * /* value */,
                 void       * /* opt_data */,
                 void       * /* cb_data */)
{
    options.show_version = true;
    return true;
}

void
MomentInstance::doExit (ConstMemory const reason)
{
    logI_ (_func, "exiting: ", reason);
    server_app.stop ();
}

void
MomentInstance::exitTimerTick (void * const _self)
{
    MomentInstance * const self = static_cast <MomentInstance*> (_self);

    logI_ (_func, "Exit timer expired (", options.exit_after, " seconds)");
    self->mutex.lock ();
    if (self->exit_timer) {
        self->server_app.getServerContext()->getMainThreadContext()->getTimers()->deleteTimer (self->exit_timer);
        self->exit_timer = NULL;
    }
    self->mutex.unlock ();

    self->doExit ("exit timer timeout");
}

#ifndef MOMENT_GPERFTOOLS
static char const * const gperftools_errmsg =
        "gperftools profiler is disabled. "
        "Configure moment with --enable-gperftools and rebuild to enable.";
#endif

void
MomentInstance::ctl_startProfiler (ConstMemory const filename)
{
#ifdef MOMENT_GPERFTOOLS
    logD_ (_func, "calling ProfilerStart()");
    ProfilerStart (String (filename).cstr());
#else
    logD_ (_func, gperftools_errmsg);
#endif
}

void
MomentInstance::ctl_stopProfiler ()
{
#ifdef MOMENT_GPERFTOOLS
    logD_ (_func, "calling ProfilerStop()");
    ProfilerStop ();
    ProfilerFlush ();
#else
    logD_ (_func, gperftools_errmsg);
#endif
}

void
MomentInstance::ctl_exit (ConstMemory const reason)
{
    doExit (reason);
}

void
MomentInstance::ctl_abort (ConstMemory const reason)
{
    logI_ (_func, "aborting: ", reason);
    abort ();
}

void
MomentInstance::ctl_segfault (ConstMemory const reason)
{
    logI_ (_func, "segfaulting: ", reason);
    *(int*)0=0;
    unreachable ();
    abort ();
}

#ifndef LIBMARY_PLATFORM_WIN32
LinePipe::Frontend const MomentInstance::ctl_pipe_frontend = {
    ctl_line
};

void MomentInstance::ctl_line (ConstMemory   const line,
                               void        * const _self)
{
    MomentInstance * const self = static_cast <MomentInstance*> (_self);

    logD_ (_func, line);

    {
        ConstMemory const str = "profiler_start";
        if (line.len() >= str.len()) {
            if (equal (line.region (0, str.len()), str)) {
                self->mutex.lock ();
                Ref<MomentConfigParams> const params = self->cur_params;
                self->mutex.unlock ();

                ConstMemory profile_filename = params->profile_filename->mem();
                if (line.len() > str.len() + 1)
                    profile_filename = line.region (str.len() + 1);

                self->ctl_startProfiler (profile_filename);
                return;
            }
        }
    }

    if (equal (line, "profiler_stop")) {
        self->ctl_stopProfiler ();
        return;
    }

    if (equal (line, "exit")) {
        self->ctl_exit ("ctl");
        return;
    }

    if (equal (line, "abort")) {
        self->ctl_abort ("ctl");
        return;
    }

    if (equal (line, "segfault")) {
        self->ctl_segfault ("ctl");
        return;
    }

    if (equal (line, "reload")) {
        if (!self->initiateConfigReload ()) {
            logE_ (_func, "Could not reload config");
        }
        return;
    }

    logW_ (_func, "WARNING: Unknown control command: ", line);
}
#endif

HttpService::HttpHandler const MomentInstance::ctl_http_handler = {
    ctlHttpRequest,
    NULL /* httpMessageBody */
};

Result
MomentInstance::ctlHttpRequest (HttpRequest   * const mt_nonnull req,
                                Sender        * const mt_nonnull conn_sender,
                                Memory const  & /* msg_body */,
                                void         ** const mt_nonnull /* ret_msg_data */,
                                void          * const _self)
{
    MomentInstance * const self = static_cast <MomentInstance*> (_self);

    logD_ (_func_);

    MOMENT_SERVER__HEADERS_DATE;

    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "config_reload"))
    {
        ConstMemory reply;
        if (self->initiateConfigReload ()) {
            reply = "OK";
        } else {
            logE_ (_func, "Could not reload config");
            reply = "ERROR";
        }

        conn_sender->send (
                &self->page_pool,
                true /* do_flush */,
                MOMENT_SERVER__OK_HEADERS ("text/html", reply.len() /* content_length */),
                "\r\n",
                reply);

        logA_ ("moment_ctl 200 ", req->getClientAddress(), " ", req->getRequestLine());
    } else {
	logE_ (_func, "Unknown admin HTTP request: ", req->getFullPath());

	ConstMemory const reply_body = "Unknown command";
	conn_sender->send (&self->page_pool,
			   true /* do_flush */,
			   MOMENT_SERVER__404_HEADERS (reply_body.len()),
			   "\r\n",
			   reply_body);

	logA_ ("moment_ctl 404 ", req->getClientAddress(), " ", req->getRequestLine());
    }

    if (!req->getKeepalive())
        conn_sender->closeAfterFlush();

    return Result::Success;
}

MomentServer::Events const MomentInstance::moment_server_events = {
    configReload,
    NULL /* destroy */
};

void
MomentInstance::configReload (MConfig::Config * const new_config,
                              void            * const _self)
{
    MomentInstance * const self = static_cast <MomentInstance*> (_self);
    self->doConfigReload (new_config);
}


// _____________________________ Config reloading ______________________________

Result
MomentInstance::initiateConfigReload ()
{
    Ref<MConfig::Config> const new_config = loadConfig ();
    if (!new_config) {
        logE_ (_func, "Could not load config");
        return Result::Failure;
    }

    moment_server.setConfig (new_config);
    return Result::Success;
}

Ref<MConfig::Config>
MomentInstance::loadConfig ()
{
    Ref<MConfig::Config> const new_config = grab (new (std::nothrow) MConfig::Config);

    ConstMemory const config_filename = options.config_filename ?
                                                options.config_filename->mem() :
                                                ConstMemory (
#ifdef LIBMARY_PLATFORM_WIN32
                                                        "moment.conf.txt"
#else
                                                        "/opt/moment/moment.conf"
#endif
                                                        );

// Solve win32 local8bit encoding first, then uncomment.
//    logI_ (_func, "config file: ", config_filename);

    if (!MConfig::parseConfig (config_filename, new_config)) {
        logE_ (_func, "Failed to parse config file ", config_filename);
        return NULL;
    }

    return new_config;
}

void
MomentInstance::doConfigReload (MConfig::Config * const new_config)
{
    logD_ (_func_);

    config_reload_mutex.lock ();

    if (logLevelOn_ (LogLevel::High)) {
        logLock ();
        log_unlocked__ (_func_);
        new_config->dump (logs);
        logUnlock ();
    }

    applyConfigParams (new_config);

    config_reload_mutex.unlock ();
}

static char const opt_name__min_pages[]               = "moment/min_pages";
static char const opt_name__num_threads[]             = "moment/num_threads";
static char const opt_name__num_file_threads[]        = "moment/num_file_threads";
static char const opt_name__profile[]                 = "moment/profile";
static char const opt_name__ctl_pipe[]                = "moment/ctl_pipe";
static char const opt_name__ctl_pipe_reopen_timeout[] = "moment/ctl_pipe_reopen_timeout";
static char const opt_name__http_keepalive_timeout[]  = "http/keepalive_timeout";
static char const opt_name__http_no_keepalive_conns[] = "http/no_keepalive_conns";
static char const opt_name__http_http_bind[]          = "http/http_bind";
static char const opt_name__http_admin_bind[]         = "http/admin_bind";

Result
MomentInstance::processConfig (MConfig::Config    * const config,
                               MomentConfigParams * const params)
{
    Result res = Result::Success;

    if (!configGetUint64 (config, opt_name__min_pages, &params->min_pages, default__page_pool__min_pages))
        res = Result::Failure;
    logI_ (_func, opt_name__min_pages, ": ", params->min_pages);

    if (!configGetUint64 (config, opt_name__num_threads, &params->num_threads, 0))
        res = Result::Failure;
    logI_ (_func, opt_name__num_threads, ": ", params->num_threads);

    if (!configGetUint64 (config, opt_name__num_file_threads, &params->num_file_threads, 0))
        res = Result::Failure;
    logI_ (_func, opt_name__num_file_threads, ": ", params->num_file_threads);

    params->profile_filename = st_grab (new (std::nothrow) String (
            config->getString_default (opt_name__profile, "/opt/moment/moment_profile")));
    params->ctl_filename = st_grab (new (std::nothrow) String (
            config->getString_default (opt_name__ctl_pipe, "/opt/moment/moment_ctl")));

    if (!configGetUint64 (config, opt_name__ctl_pipe_reopen_timeout, &params->ctl_pipe_reopen_timeout, 1))
        res = Result::Failure;
    logI_ (_func, opt_name__ctl_pipe_reopen_timeout, params->ctl_pipe_reopen_timeout);

    if (!configGetUint64 (config,
                          opt_name__http_keepalive_timeout,
                          &params->http_keepalive_timeout,
                          default__http__keepalive_timeout))
    {
        res = Result::Failure;
    }
    logI_ (_func, opt_name__http_keepalive_timeout, ": ", params->http_keepalive_timeout);

    if (!configGetBoolean (config, opt_name__http_no_keepalive_conns, &params->no_keepalive_conns, false))
        res = Result::Failure;
    logI_ (_func, opt_name__http_no_keepalive_conns, ": ", params->no_keepalive_conns);

    {
        params->http_bind_valid = false;

        ConstMemory const opt_name = opt_name__http_http_bind; 
        ConstMemory const opt_val = config->getString_default (opt_name, ":8080");
        logI_ (_func, opt_name, ": ", opt_val);
        if (opt_val.len() == 0) {
            logI_ (_func, "HTTP service is not bound to any port "
                   "and won't accept any connections. "
                   "Set \"", opt_name, "\" option to bind the service.");
        } else {
            if (!setIpAddress_default (opt_val,
                                       ConstMemory() /* default_host */,
                                       8080          /* default_port */,
                                       true          /* allow_any_host */,
                                       &params->http_bind_addr))
            {
                logE_ (_func, "setIpAddress_default() failed (http)");
                res = Result::Failure;
            } else {
                params->http_bind_valid = true;
            }
        }
    }

    {
        params->http_admin_bind_valid = false;

        ConstMemory const opt_name = opt_name__http_admin_bind; 
        ConstMemory const opt_val = config->getString_default (opt_name, ":8080");
        logI_ (_func, opt_name, ": ", opt_val);
        if (opt_val.len() == 0) {
            logI_ (_func, "HTTP service is not bound to any port "
                   "and won't accept any connections. "
                   "Set \"", opt_name, "\" option to bind the service.");
        } else {
            if (!setIpAddress_default (opt_val,
                                       ConstMemory() /* default_host */,
                                       8080          /* default_port */,
                                       true          /* allow_any_host */,
                                       &params->http_admin_bind_addr))
            {
                logE_ (_func, "setIpAddress_default() failed (http)");
                res = Result::Failure;
            } else {
                params->http_admin_bind_valid = true;
            }
        }
    }

    return res;
}

void MomentInstance::applyConfigParams (MConfig::Config * const new_config)
{
    Ref<MomentConfigParams> const params = grab (new (std::nothrow) MomentConfigParams);
    if (!processConfig (new_config, params)) {
        logE_ (_func, "Bad config. Server configuration has not been updated.");
        return;
    }

    mutex.lock ();
    Ref<MomentConfigParams> const old_params = cur_params;
    cur_params = params;
    mutex.unlock ();

    if (old_params && old_params->min_pages != params->min_pages)
        configWarnNoEffect (opt_name__min_pages);

    if (old_params && old_params->num_threads != params->num_threads)
        configWarnNoEffect (opt_name__num_threads);

    if (old_params && old_params->num_file_threads != params->num_file_threads)
        configWarnNoEffect (opt_name__num_file_threads);

    if (old_params && !equal (old_params->profile_filename->mem(), params->profile_filename->mem()))
        configWarnNoEffect (opt_name__profile);

    if (old_params && !equal (old_params->ctl_filename->mem(), params->ctl_filename->mem()))
        configWarnNoEffect (opt_name__ctl_pipe);

    if (old_params && old_params->ctl_pipe_reopen_timeout != params->ctl_pipe_reopen_timeout)
        configWarnNoEffect (opt_name__ctl_pipe_reopen_timeout);

    if (old_params
        && (old_params->http_bind_valid != params->http_bind_valid
            || (params->http_bind_valid && (old_params->http_bind_addr != params->http_bind_addr))))
    {
        configWarnNoEffect (opt_name__http_http_bind);
    }

    if (old_params
        && (old_params->http_admin_bind_valid != params->http_admin_bind_valid
            || (params->http_admin_bind_valid && (old_params->http_admin_bind_addr != params->http_admin_bind_addr))))
    {
        configWarnNoEffect (opt_name__http_admin_bind);
    }

    http_service.setConfigParams (params->http_keepalive_timeout * 1000000 /* microseconds */,
                                  params->no_keepalive_conns);
    if (admin_http_service_ptr != &http_service) {
        admin_http_service_ptr->setConfigParams (params->http_keepalive_timeout * 1000000 /* microseconds */,
                                                 params->no_keepalive_conns);
    }
}

// _____________________________________________________________________________


static void
serverApp_threadStarted (void * const /* cb_data */)
{
#ifdef MOMENT_GPERFTOOLS
    logD_ (_func, "calling ProfilerRegisterThread()");
    ProfilerRegisterThread ();
#endif
}

static ServerApp::Events const server_app_events = {
    serverApp_threadStarted
};

int
MomentInstance::run ()
{
    int ret_res = 0;

    {
	ConstMemory const log_filename = options.log_filename ?
						 options.log_filename->mem() :
						 ConstMemory ("/var/log/moment.log");
        if (log_filename.len() > 0) {
            // We never deallocate 'log_file' after log file is opened successfully.
            NativeFile * const log_file = new (std::nothrow) NativeFile ();
            assert (log_file);
            if (!log_file->open (log_filename,
                                 File::OpenFlags::Create | File::OpenFlags::Append,
                                 File::AccessMode::WriteOnly))
            {
                logE_ (_func, "Could not open log file \"", log_filename, "\": ", exc->toString());
                delete log_file;
            } else {
                logI_ (_func, "Log file is at ", log_filename);
                // We never deallocate 'logs'
                delete logs;
                logs = new (std::nothrow) BufferedOutputStream (log_file, 4096);
                assert (logs);
            }
        }
    }

    MOMENT__PREINIT

    Ref<MConfig::Config> const config = loadConfig ();
    if (!config) {
        logE_ (_func, "Could not load config");
        return EXIT_FAILURE;
    }

    if (logLevelOn_ (LogLevel::High)) {
        logLock ();
        log_unlocked__ (_func_);
        config->dump (logs);
        logUnlock ();
    }

    MOMENT__INIT

    Ref<MomentConfigParams> const params = grab (new (std::nothrow) MomentConfigParams);
    if (!processConfig (config, params)) {
        logE_ (_func, "Bad config. Exiting.");
        return EXIT_FAILURE;
    }
    mutex.lock ();
    cur_params = params;
    mutex.unlock ();

    server_app.getEventInformer()->subscribe (CbDesc<ServerApp::Events> (&server_app_events, NULL, NULL));

    if (!server_app.init ()) {
	logE_ (_func, "server_app.init() failed: ", exc->toString());
	return EXIT_FAILURE;
    }

    page_pool.setMinPages (params->min_pages);
    server_app.setNumThreads (params->num_threads);
    recorder_thread_pool.setNumThreads (params->num_file_threads);
    reader_thread_pool.setNumThreads (params->num_file_threads /* TODO Separate config parameter? */);

    if (params->http_bind_valid) {
	if (!http_service.init (server_app.getServerContext()->getMainThreadContext()->getPollGroup(),
				server_app.getServerContext()->getMainThreadContext()->getTimers(),
                                server_app.getServerContext()->getMainThreadContext()->getDeferredProcessor(),
				&page_pool,
				params->http_keepalive_timeout * 1000000 /* microseconds */,
				params->no_keepalive_conns))
	{
	    logE_ (_func, "http_service.init() failed: ", exc->toString());
	    return EXIT_FAILURE;
	}

        if (!http_service.bind (params->http_bind_addr)) {
            logE_ (_func, "http_service.bind() failed (http): ", exc->toString());
            return EXIT_FAILURE;
        }

        if (!http_service.start ()) {
            logE_ (_func, "http_service.start() failed (http): ", exc->toString());
            return EXIT_FAILURE;
        }
    }

    if (params->http_admin_bind_valid) {
        if (params->http_admin_bind_addr == params->http_bind_addr) {
            admin_http_service_ptr = &http_service;
        } else {
            if (!separate_admin_http_service.init (server_app.getServerContext()->getMainThreadContext()->getPollGroup(),
                                                   server_app.getServerContext()->getMainThreadContext()->getTimers(),
                                                   server_app.getServerContext()->getMainThreadContext()->getDeferredProcessor(),
                                                   &page_pool,
                                                   params->http_keepalive_timeout * 1000000 /* microseconds */,
                                                   params->no_keepalive_conns))
            {
                logE_ (_func, "admin_http_service.init() failed: ", exc->toString());
                return EXIT_FAILURE;
            }

            if (!separate_admin_http_service.bind (params->http_admin_bind_addr)) {
                logE_ (_func, "http_service.bind() failed (admin): ", exc->toString());
                return EXIT_FAILURE;
            }

            if (!separate_admin_http_service.start ()) {
                logE_ (_func, "http_service.start() failed (admin): ", exc->toString());
                return EXIT_FAILURE;
            }
        }
    }

    admin_http_service_ptr->addHttpHandler (
	    CbDesc<HttpService::HttpHandler> (&ctl_http_handler, this, this),
	    "ctl");

    recorder_thread_pool.setMainThreadContext (server_app.getServerContext()->getMainThreadContext());
    if (!recorder_thread_pool.spawn ()) {
	logE_ (_func, "recorder_thread_pool.spawn() failed");
	return EXIT_FAILURE;
    }

    reader_thread_pool.setMainThreadContext (server_app.getServerContext()->getMainThreadContext());
    if (!reader_thread_pool.spawn ()) {
        logE_ (_func, "reader_thread_pool.spawn() failed");
        return EXIT_FAILURE;
    }

    Ref<ChannelManager> const channel_manager = grab (new (std::nothrow) ChannelManager);
    if (!moment_server.init (&server_app,
			     &page_pool,
			     &http_service,
			     admin_http_service_ptr,
			     config,
			     &recorder_thread_pool,
                             &reader_thread_pool,
			     &storage,
                             channel_manager))
    {
	logE_ (_func, "moment_server.init() failed: ", exc->toString());
	ret_res = EXIT_FAILURE;
	goto _stop_recorder;
    }

    moment_server.getEventInformer()->subscribe (
            CbDesc<MomentServer::Events> (&moment_server_events, this, this));

    if (options.exit_after != (Uint64) -1) {
	logI_ (_func, "options.exit_after: ", options.exit_after);
	mutex.lock ();
	exit_timer = server_app.getServerContext()->getMainThreadContext()->getTimers()->addTimer (
                exitTimerTick,
                this /* cb_data */,
                this /* coderef_container */,
                options.exit_after,
                false /* periodical */);
	mutex.unlock ();
    }

#ifndef LIBMARY_PLATFORM_WIN32
    if (!line_pipe.init (params->ctl_filename->mem(),
                         CbDesc<LinePipe::Frontend> (&ctl_pipe_frontend, this, this),
                         server_app.getServerContext()->getMainThreadContext()->getPollGroup(),
                         server_app.getServerContext()->getMainThreadContext()->getDeferredProcessor(),
                         server_app.getServerContext()->getMainThreadContext()->getTimers(),
                         params->ctl_pipe_reopen_timeout * 1000 /* reopen_timeout_millisec */))
    {
        logE_ (_func, "could not initialize ctl pipe: ", exc->toString());
    }
#endif

  {
    channel_manager->init (&moment_server, &page_pool);
    {
        // TODO Parse 'streams' config section for these.
        Ref<ChannelOptions> const default_channel_opts = grab (new (std::nothrow) ChannelOptions);
        Ref<PlaybackItem> const default_item = grab (new (std::nothrow) PlaybackItem);
        default_channel_opts->default_item = default_item;

#warning call setDefaultChannelOptions() on config reload
        channel_manager->setDefaultChannelOptions (default_channel_opts);
    }
    {
        if (!channel_manager->loadConfigFull ()) {
            logE_ (_func, "loadConfigFull() failed");
            ret_res = EXIT_FAILURE;
            goto _stop_recorder;
        }
    }

    if (!server_app.run ()) {
	logE_ (_func, "server_app.run() failed: ", exc->toString());
	ret_res = EXIT_FAILURE;
	goto _stop_recorder;
    }
  }

    logI_ (_func, "done");

_stop_recorder:
    recorder_thread_pool.stop ();
    reader_thread_pool.stop ();

    return ret_res;
}

} // namespace {}

int main (int argc, char **argv)
{
    libMaryInit ();

    {
	unsigned const num_opts = 9;
	CmdlineOption opts [num_opts];

	opts [0].short_name = "h";
	opts [0].long_name  = "help";
	opts [0].with_value = false;
	opts [0].opt_data   = NULL;
	opts [0].opt_callback = cmdline_help;

	opts [1].short_name = "d";
	opts [1].long_name  = "daemonize";
	opts [1].with_value = false;
	opts [1].opt_data   = NULL;
	opts [1].opt_callback = cmdline_daemonize;

	opts [2].short_name = "c";
	opts [2].long_name  = "config";
	opts [2].with_value = true;
	opts [2].opt_data   = NULL;
	opts [2].opt_callback = cmdline_config;

	opts [3].short_name = "l";
	opts [3].long_name  = "log";
	opts [3].with_value = true;
	opts [3].opt_data   = NULL;
	opts [3].opt_callback = cmdline_log;

	opts [4].short_name = "NULL";
	opts [4].long_name  = "loglevel";
	opts [4].with_value = true;
	opts [4].opt_data   = NULL;
	opts [4].opt_callback = cmdline_loglevel;

	opts [5].short_name = NULL;
	opts [5].long_name  = "exit-after";
	opts [5].with_value = true;
	opts [5].opt_data   = NULL;
	opts [5].opt_callback = cmdline_exit_after;

        opts [6].short_name = NULL;
        opts [6].long_name  = "gst-debug";
        opts [6].with_value = false;
        opts [6].opt_data   = NULL;
        opts [6].opt_callback = cmdline_gst_debug;

        opts [7].short_name = NULL;
        opts [7].long_name  = "keyfile";
        opts [7].with_value = true;
        opts [7].opt_data   = MOMENT__KEYFILE_DATA;
        opts [7].opt_callback = cmdline_keyfile;

        opts [8].short_name = NULL;
        opts [8].long_name  = "version";
        opts [8].with_value = false;
        opts [8].opt_data   = NULL;
        opts [8].opt_callback = cmdline_version;

	ArrayIterator<CmdlineOption> opts_iter (opts, num_opts);
	parseCmdline (&argc, &argv, opts_iter, NULL /* callback */, NULL /* callbackData */);
    }

    if (options.show_version) {
        setGlobalLogLevel (LogLevel::Failure);
        outs->println ("moment "
#ifdef MOMENT_RELEASE_TAG
                       MOMENT_RELEASE_TAG
#else
                       "unknown"
#endif
                );
        return 0;
    }

    if (options.help) {
        setGlobalLogLevel (LogLevel::Failure);
	printUsage ();
	return 0;
    }
#ifdef MOMENT_GSTREAMER
    libMomentGstInit (options.gst_debug ? ConstMemory ("--gst-debug=*:3") : ConstMemory());
#endif
    setGlobalLogLevel (options.loglevel);

    // Note that the log file is not opened yet (logs == errs).
    if (options.daemonize) {
#ifdef LIBMARY_PLATFORM_WIN32
        logW_ (_func, "Daemonization is not supported on Windows");
#else
	logI_ (_func, "Daemonizing. Server log is at /var/log/moment.log");
	int const res = daemon (1 /* nochdir */, 0 /* noclose */);
	if (res == -1)
	    logE_ (_func, "daemon() failed: ", errnoString (errno));
	else
	if (res != 0)
	    logE_ (_func, "Unexpected return value from daemon(): ", res);
#endif
    }

    Ref<MomentInstance> const moment_instance = grab (new (std::nothrow) MomentInstance);
    return moment_instance->run ();
}

