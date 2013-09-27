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


#include <moment-nvr/naming_scheme.h>
#include <string>


using namespace M;
using namespace Moment;

namespace MomentNvr {

StRef<String>
DefaultNamingScheme::getPath (ConstMemory   const channel_name,
                              Time          const unixtime_sec,
                              Time        * const ret_next_unixtime_sec)
{
    struct tm tm;
    if (!unixtimeToStructTm (unixtime_sec, &tm)) {
        logE_ (_func, "unixtimeToStructTm() failed");
        return NULL;
    }

    unsigned const    day_duration = 3600 * 24;
    unsigned const   hour_duration = 3600;
    unsigned const minute_duration = 60;

    Format fmt;
    fmt.min_digits = 2;

    StRef<String> res_str;

    //  previous file    gmtime   next file boundary 
    //    boundary          |             |
    //       |              v             |
    // ______|____________________________|_____________________
    //        \____________/^
    //              |       |
    //              |   unixtime
    //              |
    //           offset

    if (file_duration_sec >= day_duration) {
        // The offset is approximate but close.
        // There may a few seconds of difference, and we don't account for that here,
        // rounding to the neares day instead.
        unsigned const offset = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;

        *ret_next_unixtime_sec = unixtime_sec + (day_duration - offset);
        res_str = st_makeString (tm.tm_year + 1900, "/", fmt, tm.tm_mon + 1, "/", tm.tm_mday);
    } else
    if (file_duration_sec >= hour_duration) {
        unsigned hours = file_duration_sec / hour_duration;
        unsigned const offset = (tm.tm_hour % hours) * 3600 + tm.tm_min * 60 + tm.tm_sec;
        *ret_next_unixtime_sec = unixtime_sec + (hours * 3600 - offset);
        logD_ (_func, "tm.tm_mday: ", tm.tm_mday, ", tm.tm_hour: ", tm.tm_hour, ", hour: ", tm.tm_hour - tm.tm_hour % hours);
        res_str = st_makeString (tm.tm_year + 1900, "/", fmt, tm.tm_mon + 1, "/", tm.tm_mday, "/", tm.tm_hour - tm.tm_hour % hours);
    } else
    if (file_duration_sec >= minute_duration) {
        unsigned minutes = file_duration_sec / minute_duration;
        unsigned const offset = (tm.tm_min % minutes) * 60 + tm.tm_sec;
        *ret_next_unixtime_sec = unixtime_sec + (minutes * 60 - offset);
        res_str = st_makeString (tm.tm_year + 1900, "/", fmt, tm.tm_mon + 1, "/", tm.tm_mday, "/", tm.tm_hour, "/", tm.tm_min - tm.tm_min % minutes);
    } else {
        unsigned const offset = tm.tm_sec % file_duration_sec;
        *ret_next_unixtime_sec = unixtime_sec + (file_duration_sec - offset);
        res_str = st_makeString (tm.tm_year + 1900, "/", fmt, tm.tm_mon + 1, "/", tm.tm_mday, "/", tm.tm_hour, "/", tm.tm_min, "/", tm.tm_sec - offset);
    }

    if (*ret_next_unixtime_sec == unixtime_sec)
        *ret_next_unixtime_sec = unixtime_sec + 1;

    res_str = st_makeString (channel_name, "/", res_str);
//    logD_ (_func, "res_str: ", res_str);

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
    pos = stdStr.find(delimiter1);

    if(pos != std::string::npos)
    {
        token = stdStr.substr(0, pos);
        stdStr.erase(0, pos + delimiter1.length());
        pos = stdStr.find(delimiter2);

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

