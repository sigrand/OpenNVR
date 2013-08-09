/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov
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


#include <moment/libmoment.h>


using namespace M;

namespace Moment {

Result configGetUint64 (MConfig::Config * const mt_nonnull config,
                        ConstMemory       const opt_name,
                        Uint64          * const mt_nonnull ret_val,
                        Uint64            const default_val)
{
    *ret_val = default_val;

    if (!config->getUint64_default (opt_name, ret_val, default_val)) {
        logE_ (_func, "Invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        return Result::Failure;
    } else {
//        logI_ (_func, opt_name, ": ", *ret_val);
    }

    return Result::Success;
}

Result configSectionGetUint64 (MConfig::Section * mt_nonnull section,
                               ConstMemory       opt_name,
                               Uint64           * mt_nonnull ret_val,
                               Uint64            default_val)
{
    *ret_val = default_val;

    MConfig::Option * const opt = section->getOption (opt_name, false /* create */);
    if (!opt)
        return Result::Success;

    MConfig::Value * const val = opt->getValue();
    if (!val)
        return Result::Success;

    Uint64 res_val = 0;
    if (!strToUint64_safe (val->mem(), &res_val)) {
        logE_ (_func, "Invalid falue for ", opt_name, ": ", val->mem());
        return Result::Failure;
    }

    *ret_val = res_val;
    return Result::Success;
}

Result configGetBoolean (MConfig::Config * const mt_nonnull config,
                         ConstMemory       const opt_name,
                         bool            * const mt_nonnull ret_val,
                         bool              const default_val)
{
    *ret_val = default_val;

    MConfig::BooleanValue const val = config->getBoolean (opt_name);
    if (val == MConfig::Boolean_Invalid) {
        logE_ (_func, "Invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        return Result::Failure;
    } else
    if (val == MConfig::Boolean_True)
        *ret_val = true;
    else
    if (val == MConfig::Boolean_False)
        *ret_val = false;
    else
        assert (val == MConfig::Boolean_Default);

//    logI_ (_func, opt_name, ": ", *ret_val);
    return Result::Success;
}

Result configSectionGetBoolean (MConfig::Section * const mt_nonnull section,
                                ConstMemory        const opt_name,
                                bool             * const mt_nonnull ret_val,
                                bool               const default_val)
{
    *ret_val = default_val;

    MConfig::Option * const opt = section->getOption (opt_name);
    if (!opt)
        return Result::Success;

    MConfig::BooleanValue const val = opt->getBoolean();
    if (val == MConfig::Boolean_Invalid) {
        MConfig::Value * const mval = opt->getValue();
        logE_ (_func, "Invalid value for ", opt_name, ": ", (mval ? mval->getAsString()->mem() : ConstMemory()));
        return Result::Failure;
    } else
    if (val == MConfig::Boolean_True)
        *ret_val = true;
    else
    if (val == MConfig::Boolean_False)
        *ret_val = false;
    else
        assert (val == MConfig::Boolean_Default);

    return Result::Success;
}

void configWarnNoEffect (ConstMemory const opt_name)
{
    logI_ (_func, "Changing ", opt_name, " has no effect until the server is restarted");
}

}

