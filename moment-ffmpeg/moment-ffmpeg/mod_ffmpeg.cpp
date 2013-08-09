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


#include <moment-ffmpeg/moment_ffmpeg_module.h>


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static void serverDestroy (void *_ffmpeg_module);

static MomentServer::Events const server_events = {
    NULL /* configReload */,
    serverDestroy
};

static void serverDestroy (void * const _ffmpeg_module)
{
    MomentFFmpegModule * const ffmpeg_module = static_cast <MomentFFmpegModule*> (_ffmpeg_module);

    logH_ (_func_);
    ffmpeg_module->unref ();
}

static void glibLoopThreadFunc (void * const /* cb_data */)
{
    updateTime();

    GMainLoop * const loop = g_main_loop_new (g_main_context_default() /* same as NULL? */, FALSE);
    g_main_loop_run (loop);
    g_main_loop_unref (loop);
    logI_ (_func, "done");
}

static void momentFFmpegInit ()
{
    logI_ (_func, "Initializing mod_ffmpeg");

    {
	Ref<Thread> const thread = grab (new (std::nothrow) Thread (
		CbDesc<Thread::ThreadFunc> (glibLoopThreadFunc, NULL, NULL)));
	if (!thread->spawn (false /* joinable */))
	    logE_ (_func, "Failed to spawn glib main loop thread: ", exc->toString());
    }

    MomentServer * const moment = MomentServer::getInstance();
    MConfig::Config * const config = moment->getConfig();

    MomentFFmpegModule * const ffmpeg_module = new (std::nothrow) MomentFFmpegModule;
    assert (ffmpeg_module);
    moment->getEventInformer()->subscribe (
            CbDesc<MomentServer::Events> (&server_events, ffmpeg_module, NULL));

    {
	ConstMemory const opt_name = "mod_ffmpeg/enable";
	MConfig::BooleanValue const enable = config->getBoolean (opt_name);
	if (enable == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return;
	}

	if (enable == MConfig::Boolean_False) {
	    logI_ (_func, "FFmpeg module is not enabled. "
		   "Set \"", opt_name, "\" option to \"y\" to enable.");
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_ffmpeg/ffmpeg_debug";
	MConfig::BooleanValue const ffmpeg_debug = config->getBoolean (opt_name);
	if (ffmpeg_debug == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name);
	    return;
	}

// TODO Honour gst_debug
#if 0
	if (gst_debug == MConfig::Boolean_True) {
	  // Initialization with gstreamer debugging output enabled.

	    int argc = 2;
	    char* argv [] = {
		(char*) "moment",
		(char*) "--gst-debug=*:3",
		NULL
	    };

	    char **argv_ = argv;
	    gst_init (&argc, &argv_);
	} else {
	    gst_init (NULL /* argc */, NULL /* argv */);
	}
#endif

//        libMomentGstInit ();
    }

    if (!ffmpeg_module->init (MomentServer::getInstance()))
	logE_ (_func, "ffmpeg_module->init() failed");
}

static void momentFFmpegUnload ()
{
    logI_ (_func, "Unloading mod_ffmpeg");
}

} // namespace MomentFFmpeg


#include <libmary/module_init.h>

namespace M {

void libMary_moduleInit ()
{
    MomentFFmpeg::momentFFmpegInit ();
}

void libMary_moduleUnload ()
{
    MomentFFmpeg::momentFFmpegUnload ();
}

}

