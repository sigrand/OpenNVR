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


#include "naming_scheme.h"

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

StRef<String>
DefaultNamingScheme::getPath (ConstMemory   const channel_name,
                              const timeval & tv)
{
    const Time unixtime_sec = tv.tv_sec;

    struct tm tm;
    if (!unixtimeToStructTm (unixtime_sec, &tm)) {
        logE_ (_func, "unixtimeToStructTm() failed");
        return NULL;
    }

    unsigned const    day_duration = 60 * 60 * 24;
    unsigned const   hour_duration = 60 * 60;
    unsigned const minute_duration = 60;
    Time file_duration_sec_local = file_duration_sec;

    Format fmt;
    fmt.min_digits = 2;

    StRef<String> res_str;

    if (file_duration_sec_local >= day_duration)
    {
        file_duration_sec_local = day_duration - 1;
    }
    if (file_duration_sec_local < minute_duration)
    {
        file_duration_sec_local = minute_duration;
    }

    res_str = st_makeString (tm.tm_year + 1900, "/", fmt, tm.tm_mon + 1, "/", tm.tm_mday, "/", tm.tm_hour, "/", tm.tm_hour, tm.tm_min, tm.tm_sec);

    Time unixtimeprefix = (Time)tv.tv_sec * 1000000000LL + (Time)tv.tv_usec * 1000;
    res_str = st_makeString (channel_name, "/", res_str, "_", unixtimeprefix);

    return res_str;
}

Result FileNameToUnixTimeStamp::Convert(const StRef<String> & fileName, /*output*/ Time & timeOfRecord)
{
    if(fileName == NULL || !fileName->len())
        return Result::Failure;

    Result res = Result::Success;
    std::string stdStr(fileName->cstr());
    std::string delimiter1 = "_";
    std::string delimiter2 = ".";
    size_t pos = 0;
    std::string token;
    pos = stdStr.rfind(delimiter1);

    if(pos != std::string::npos)
    {
        token = stdStr.substr(0, pos);
        stdStr.erase(0, pos + delimiter1.length());
        pos = stdStr.rfind(delimiter2);

        if(pos != std::string::npos)
        {
            token = stdStr.substr(0, pos);
            res = strToUint64_safe(token.c_str(), &timeOfRecord);
        }
        else
        {
            logE_(_func_,"Didn't find '.' symbol in file_name = ", fileName->mem());
            res = Result::Failure;
        }
    }
    else
    {
        logE_(_func_,"Didn't find '_' symbol in file_name = ", fileName->mem());
        res = Result::Failure;
    }

    return res;
}

}

