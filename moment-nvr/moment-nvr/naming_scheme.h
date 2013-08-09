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


#ifndef MOMENT_NVR__NAMING_SCHEME__H__
#define MOMENT_NVR__NAMING_SCHEME__H__


#include <moment/libmoment.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

struct NamingScheme : public Referenced
{
    virtual StRef<String> getPath (ConstMemory  channel_name,
                                   Time         unixtime_sec,
                                   Time        *ret_next_unixtime_sec) = 0;
};

class DefaultNamingScheme : public NamingScheme
{
private:
    Time          const file_duration_sec;

public:
    StRef<String> getPath (ConstMemory  channel_name,
                           Time         unixtime_sec,
                           Time        *ret_next_unixtime_sec);

    DefaultNamingScheme (Time const file_duration_sec)
        : file_duration_sec (file_duration_sec)
    {}
};

}


#endif /* MOMENT_NVR__NAMING_SCHEME__H__ */

