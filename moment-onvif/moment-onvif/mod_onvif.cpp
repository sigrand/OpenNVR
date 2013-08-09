
#include <moment-onvif/moment_onvif_module.h>


using namespace M;
using namespace Moment;

namespace MomentOnvif {

static void serverDestroy (void *_onvif_module);

static MomentServer::Events const server_events = {
    NULL /* configReload */,
    serverDestroy
};

static void serverDestroy (void * const _onvif_module)
{
    MomentOnvifModule * const onvif_module = static_cast <MomentOnvifModule*> (_onvif_module);

    logH_ (_func_);
    onvif_module->unref ();
}

static void momentOnvifInit ()
{
    int onvifPort;
    logI_ (_func, "Initializing mod_onvif");

    MomentServer * const moment = MomentServer::getInstance();
    MConfig::Config * const config = moment->getConfig();

    MomentOnvifModule * const onvif_module = new (std::nothrow) MomentOnvifModule;
    assert (onvif_module);

    {
    	ConstMemory const opt_name = "mod_onvif/enable";
    	MConfig::BooleanValue const enable = config->getBoolean (opt_name);
    	if (enable == MConfig::Boolean_Invalid) {
    	    logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
    	    return;
    	}

    	if (enable == MConfig::Boolean_False) {
    	    logI_ (_func, "Onvif module is not enabled. "
    		   "Set \"", opt_name, "\" option to \"y\" to enable.");
    	    return;
    	}
    }

    {
        Uint64 port;
        ConstMemory const opt_name = "mod_onvif/port";
        MConfig::GetResult res = config->getUint64_default(opt_name, &port, 80);
        if (res == MConfig::GetResult::Invalid)
        {
            logI_ (_func, "Unable to get port for Onvif Module; module will be disabled"
               "Set \"", opt_name, "\" option to \"y\" to enable.");
            return;
        }
        onvifPort = port;
    }

    if (!onvif_module->init (MomentServer::getInstance(), onvifPort))
	   logE_ (_func, "onvif_module->init() failed");
}

static void momentOnvifUnload ()
{
    logI_ (_func, "Unloading mod_onvif");
}

} // namespace MomentOnvif


#include <libmary/module_init.h>

namespace M {

void libMary_moduleInit ()
{
    MomentOnvif::momentOnvifInit ();
}

void libMary_moduleUnload ()
{
    MomentOnvif::momentOnvifUnload ();
}

}

