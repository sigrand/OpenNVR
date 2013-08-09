/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2012 Dmitry Shatrov

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include <libmary/stat.h>


namespace M {

Stat::ParamKey
Stat::getParam (ConstMemory const param_name)
{
    mutex.lock ();
    Stat::ParamKey const param = getParam_unlocked (param_name);
    mutex.unlock ();
    return param;
}

Stat::ParamKey
Stat::getParam_unlocked (ConstMemory const param_name)
{
    StatParam * const stat_param = stat_param_hash.lookup (param_name);
    return stat_param;
}

void
Stat::getAllParams (List<StatParam> * const mt_nonnull ret_params)
{
    mutex.lock ();

    StatParamHash::iter iter (stat_param_hash);
    while (!stat_param_hash.iter_done (iter)) {
        StatParam * const stat_param = stat_param_hash.iter_next (iter);

        StatParam * const dst = &ret_params->appendEmpty ()->data;
        dst->param_name   = stat_param->param_name;
        dst->param_desc   = stat_param->param_desc;
        dst->param_type   = stat_param->param_type;
        dst->int64_value  = stat_param->int64_value;
        dst->double_value = stat_param->double_value;
    }

    mutex.unlock ();
}

Stat::ParamKey
Stat::createParam (ConstMemory const param_name,
                   ConstMemory const param_desc,
                   ParamType   const param_type,
                   Int64       const int64_value,
                   double      const double_value)
{
    StatParam * const stat_param = new StatParam;
    stat_param->param_name = grab (new String (param_name));
    stat_param->param_desc = grab (new String (param_desc));
    stat_param->param_type = param_type;
    stat_param->int64_value  = int64_value;
    stat_param->double_value = double_value;

    if (stat_slave) {
        stat_param->slave_param = stat_slave->createParam (param_name,
                                                           param_desc,
                                                           param_type,
                                                           int64_value,
                                                           double_value);
    }

    mutex.lock ();
    stat_param_hash.add (stat_param);
    mutex.unlock ();

    return stat_param;
}

void
Stat::setInt (ParamKey const param,
              Int64    const value)
{
    StatParam * const stat_param = static_cast <StatParam*> ((void*) param);

    mutex.lock ();
    stat_param->int64_value = value;
    mutex.unlock ();

    if (stat_slave && stat_param->slave_param)
        stat_slave->setInt (stat_param->slave_param, value);
}

void
Stat::setDouble (ParamKey const param,
                 double   const value)
{
    StatParam * const stat_param = static_cast <StatParam*> ((void*) param);

    mutex.lock ();
    stat_param->double_value = value;
    mutex.unlock ();

    if (stat_slave && stat_param->slave_param)
        stat_slave->setDouble (stat_param->slave_param, value);
}

void
Stat::addInt (ParamKey const param,
              Int64    const delta)
{
    StatParam * const stat_param = static_cast <StatParam*> ((void*) param);

    mutex.lock ();
    stat_param->int64_value += delta;
    Int64 const value = stat_param->int64_value;
    mutex.unlock ();

    if (stat_slave && stat_param->slave_param)
        stat_slave->setInt (stat_param->slave_param, value);
}

void
Stat::addDouble (ParamKey const param,
                 double   const delta)
{
    StatParam * const stat_param = static_cast <StatParam*> ((void*) param);

    mutex.lock ();
    stat_param->double_value += delta;
    double const value = stat_param->double_value;
    mutex.unlock ();

    if (stat_slave && stat_param->slave_param)
        stat_slave->setDouble (stat_param->slave_param, value);
}

Int64
Stat::getInt (ParamKey const param)
{
    mutex.lock ();
    Int64 const res = getInt_unlocked (param);
    mutex.unlock ();
    return res;
}

double
Stat::getDouble (ParamKey const param)
{
    mutex.lock ();
    double const res = getDouble_unlocked (param);
    mutex.unlock ();
    return res;
}

Int64
Stat::getInt_unlocked (ParamKey const param)
{
    StatParam * const stat_param = static_cast <StatParam*> ((void*) param);
    return stat_param->int64_value;
}

double
Stat::getDouble_unlocked (ParamKey const param)
{
    StatParam * const stat_param = static_cast <StatParam*> ((void*) param);
    return stat_param->double_value;
}

Stat::~Stat ()
{
    mutex.lock ();
    assert (stat_slave == NULL);

    {
        StatParamHash::iter iter (stat_param_hash);
        while (!stat_param_hash.iter_done (iter)) {
            StatParam * const stat_param = stat_param_hash.iter_next (iter);
            delete stat_param;
        }
    }

    mutex.unlock ();
}

}

