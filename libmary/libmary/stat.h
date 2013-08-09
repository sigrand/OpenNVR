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


#ifndef __LIBMARY__STAT__H___
#define __LIBMARY__STAT__H___


#include <libmary/types.h>
#include <libmary/referenced.h>
#include <libmary/list.h>
#include <libmary/hash.h>
#include <libmary/string.h>


namespace M {

class Stat
{
private:
    Mutex mutex;

public:
    enum ParamType
    {
        ParamType_Int64,
        ParamType_Double
    };

    class ParamKey
    {
    private:
        void *key;
    public:
        operator void* () const { return key; }
        ParamKey (void * const key) : key (key) {}
        ParamKey () : key (NULL) {}
    };

    class StatSlave : public virtual Referenced
    {
    public:
        virtual ParamKey createParam (ConstMemory param_name,
                                      ConstMemory param_desc,
                                      ParamType   param_type,
                                      Int64       int64_value,
                                      double      double_value) = 0;

        virtual void setInt (ParamKey param,
                             Int64    value) = 0;

        virtual void setDouble (ParamKey param,
                                double   value) = 0;
    };

    // TODO class InternalStatParam
    class StatParam : public HashEntry<>
    {
    public:
        mt_const Ref<String> param_name;
        mt_const Ref<String> param_desc;
        mt_const ParamType   param_type;
        mt_mutex (mutex) Int64  int64_value;
        mt_mutex (mutex) double double_value;

        ParamKey slave_param;
    };

    typedef Hash< StatParam,
                  ConstMemory,
                  MemberExtractor< StatParam,
                                   Ref<String>,
                                   &StatParam::param_name,
                                   ConstMemory,
                                   AccessorExtractor< String,
                                                      Memory,
                                                      &String::mem > >,
                  MemoryComparator<> >
            StatParamHash;

private:
    mt_mutex (mutex) StatParamHash stat_param_hash;

    mt_mutex (mutex) Ref<StatSlave> stat_slave;

public:
    mt_locks (mutex) void lock ()
    {
        mutex.lock ();
    }

    mt_unlocks (mutex) void unlock ()
    {
        mutex.unlock ();
    }

    ParamKey getParam (ConstMemory param_name);

    ParamKey getParam_unlocked (ConstMemory param_name);

    void getAllParams (List<StatParam> * mt_nonnull ret_params);

    StatParamHash* getStatParamHash_unlocked ()
    {
        return &stat_param_hash;
    }

    ParamKey createParam (ConstMemory param_name,
                          ConstMemory param_desc,
                          ParamType   param_type,
                          Int64       int64_value,
                          double      double_value);

    void setInt (ParamKey param,
                 Int64    value);

    void setDouble (ParamKey param,
                    double   value);

    void addInt (ParamKey param,
                 Int64    delta);

    void addDouble (ParamKey param,
                    double   delta);

    Int64 getInt (ParamKey param);

    double getDouble (ParamKey param);

    Int64 getInt_unlocked (ParamKey param);

    double getDouble_unlocked (ParamKey param);

    void inc (ParamKey param)
    {
        addInt (param, 1);
    }

    void dec (ParamKey param)
    {
        addInt (param, -1);
    }

    void setStatSlave (StatSlave * const stat_slave)
    {
        mutex.lock ();
        this->stat_slave = stat_slave;
        mutex.unlock ();
    }

    // FIXME Racy, since we access 'stat_slave' with 'mutex' unlocked.
    void releaseStatSlave ()
    {
        setStatSlave (NULL);
    }

    ~Stat ();
};

extern Stat *_libMary_stat;

static inline Stat* getStat ()
{
    return _libMary_stat;
}

}


#endif /* __LIBMARY__STAT__H___ */

