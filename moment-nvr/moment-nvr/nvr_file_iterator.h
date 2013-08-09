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


#ifndef MOMENT_NVR__NVR_FILE_ITERATOR__H__
#define MOMENT_NVR__NVR_FILE_ITERATOR__H__


#include <moment/libmoment.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

mt_unsafe class NvrFileIterator
{
private:
    Ref<Vfs> vfs;
    StRef<String> stream_name;

    bool got_first;
    // year/month/day/hour/minute/seconds
    unsigned cur_pos [6];

    static StRef<String> makePathForDepth (ConstMemory  stream_name,
                                           unsigned     depth,
                                           unsigned    * mt_nonnull pos);

    StRef<String> getNext_rec (Vfs::VfsDirectory * mt_nonnull parent_dir,
                               ConstMemory        parent_dir_name,
                               unsigned           depth,
                               bool               parent_pos_match);

    void doSetCurPos (Time start_unixtime_sec);

public:
    StRef<String> getNext ();

    void reset (Time start_unixtime_sec);

    void init (Vfs         * mt_nonnull vfs,
               ConstMemory  stream_name,
               Time         start_unixtime_sec);

    NvrFileIterator ()
        : got_first (false)
    {}
};

}


#endif /* MOMENT_NVR__NVR_FILE_ITERATOR__H__ */

