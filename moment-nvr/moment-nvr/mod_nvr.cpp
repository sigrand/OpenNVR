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


#include <libmary/module_init.h>

#include <moment-nvr/moment_nvr_module.h>


using namespace M;
using namespace Moment;
using namespace MomentNvr;


static MomentNvrModule *moment_nvr;

static void momentNvrInit ()
{
    logD_ (_func_);

    Ref<MomentServer> const moment = MomentServer::getInstance();
    Ref<MConfig::Config> const config = moment->getConfig ();

    {
        ConstMemory const opt_name = "mod_nvr/enable";
        MConfig::BooleanValue const enable = config->getBoolean (opt_name);
        if (enable == MConfig::Boolean_Invalid) {
            logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
            return;
        }

        if (enable != MConfig::Boolean_True) {
            logI_ (_func, "NVR module is not enabled. "
                          "Set \"", opt_name, "\" option to \"y\" to enable.");
            return;
        }
    }

    moment_nvr = new (std::nothrow) MomentNvrModule;
    assert (moment_nvr);
    moment_nvr->init (moment);
}

static void momentNvrUnload ()
{
    logD_ (_func_);

    moment_nvr->unref ();
}


namespace M {

void libMary_moduleInit ()
{
    momentNvrInit ();
}

void libMary_moduleUnload ()
{
    momentNvrUnload ();
}

}

