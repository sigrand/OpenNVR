/*  Moment Video Server - High performance media server
    Copyright (C) 2011, 2012 Dmitry Shatrov
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


#include <libmary/module_init.h>
#include <moment/libmoment.h>


using namespace M;
using namespace Moment;

static Ref<TestStreamGenerator> test_stream_generator;

static void momentTestInit ()
{
    logD_ (_func, "Initializing mod_test");

    Ref<MomentServer>       const moment     = MomentServer::getInstance();
    MConfig::Config       * const config     = moment->getConfig();
    CodeDepRef<ServerApp>   const server_app = moment->getServerApp();
    CodeDepRef<PagePool>    const page_pool  = moment->getPagePool();
    CodeDepRef<Timers>      const timers     = server_app->getServerContext()->getMainThreadContext()->getTimers();

    TestStreamGenerator::Options opts;

    {
	ConstMemory const opt_name = "mod_test/enable";
	MConfig::BooleanValue const enable = config->getBoolean (opt_name);
	if (enable == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return;
	}

	if (enable != MConfig::Boolean_True) {
	    logI_ (_func, "Test module (mod_test) is not enabled.");
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_test/frame_duration";
        if (!config->getUint64_default (opt_name, &opts.frame_duration, opts.frame_duration)) {
	    logE_ (_func, "Bad value for config option ", opt_name);
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_test/keyframe_interval";
        if (!config->getUint64_default (opt_name, &opts.keyframe_interval, opts.keyframe_interval)) {
	    logE_ (_func, "Bad value for config option ", opt_name);
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_test/frame_size";
        if (!config->getUint64_default (opt_name, &opts.frame_size, opts.frame_size)) {
	    logE_ (_func, "Bad value for config option ", opt_name);
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_test/prechunk_size";
        if (!config->getUint64_default (opt_name, &opts.prechunk_size, opts.prechunk_size)) {
	    logE_ (_func, "Bad value for config option ", opt_name);
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_test/start_timestamp";
        if (!config->getUint64_default (opt_name, &opts.start_timestamp, opts.start_timestamp)) {
	    logE_ (_func, "Bad value for config option ", opt_name);
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_test/burst_width";
	if (!config->getUint64_default (opt_name, &opts.burst_width, opts.burst_width)) {
	    logE_ (_func, "Bad value for config option ", opt_name);
	    return;
	}
    }

    {
	ConstMemory const opt_name = "mod_test/same_pages";
	MConfig::BooleanValue const val = config->getBoolean (opt_name);
	if (val == MConfig::Boolean_Invalid) {
	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
	    return;
	}

        if (val == MConfig::Boolean_True)
	    opts.use_same_pages = true;
	else
	if (val == MConfig::Boolean_False)
	    opts.use_same_pages = false;
    }

    ConstMemory const stream_name = config->getString_default ("mod_test/stream_name", "test");

    Ref<VideoStream> const video_stream = grab (new VideoStream);
    moment->addVideoStream (video_stream, stream_name);

    test_stream_generator = grab (new TestStreamGenerator);
    test_stream_generator->init (page_pool, timers, video_stream, &opts);
    test_stream_generator->start ();
}

static void momentTestUnload ()
{
}

namespace M {

void libMary_moduleInit ()
{
    momentTestInit ();
}

void libMary_moduleUnload ()
{
    momentTestUnload ();
}

} // namespace M

