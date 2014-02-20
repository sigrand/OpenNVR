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


#include <stdio.h>
#include <string>

#include <moment-ffmpeg/nvr_file_iterator.h>
#include <moment-ffmpeg/inc.h>
#include <moment-ffmpeg/time_checker.h>
#include <moment-ffmpeg/nvr_cleaner.h>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static LogGroup libMary_logGroup_cleaner ("mod_ffmpeg.nvr_cleaner", LogLevel::E);

void
NvrCleaner::doRemoveFiles (ConstMemory const filename, Vfs * vfs)
{
  // TODO Check return values;
    vfs->removeFile (filename);
    vfs->removeSubdirsForFilename (filename);
}

void
NvrCleaner::cleanupTimerTick (void * const _self)
{
    NvrCleaner * const self = static_cast <NvrCleaner*> (_self);

    logD (cleaner, _func, "0x", fmt_hex, (UintPtr) self);

    TimeChecker tc;tc.Start();

    std::string curPath = self->m_recpathConfig->GetNextPath();
    while(curPath.length() != 0)
    {
        StRef<String> strRecDir = st_makeString(curPath.c_str());
        ConstMemory record_dir = strRecDir->mem();
        logD(cleaner, _func_, "record_dir = ", record_dir);
        Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (record_dir);

        NvrFileIterator file_iter;
        file_iter.init (vfs, self->stream_name->mem(), 0 /* start_unixtime_sec */);

        Time const cur_unixtime_sec = getUnixtime();

      // TODO Convert cur_unixtime_sec to UTC and compare struct tm representations
      //      instead of raw unixtimes.

        while (true)
        {
            StRef<String> const filename = file_iter.getNext ();

        logD (cleaner, _func_, "current filename = ", filename);

            if (!filename)
                break;

            Time file_unixtime_sec;
            std::string stdStr(filename->cstr());
            std::string delimiter = "_";
            size_t pos = 0;
            std::string token;
            // find '_'
            pos = stdStr.rfind(delimiter);

            if(pos == std::string::npos)
                continue;

            token = stdStr.substr(0, pos);
            stdStr.erase(0, pos + delimiter.length());

            strToUint64_safe(stdStr.c_str(), &file_unixtime_sec, 10);

            file_unixtime_sec = file_unixtime_sec / 1000000000;

        if (file_unixtime_sec < cur_unixtime_sec
            && cur_unixtime_sec - file_unixtime_sec > self->max_age_sec)
        {
            StRef<String> const flv_filename = st_makeString (filename, ".flv");
            logD (cleaner, _func, "Removing ", flv_filename);
            self->doRemoveFiles (flv_filename->mem(), vfs);
        } else {
            logD (cleaner, _func, "end of removing");
            break;
        }
    }

        curPath = self->m_recpathConfig->GetNextPath(curPath);
    }

    Time t;tc.Stop(&t);
    logD (cleaner, _func_, "NvrCleaner.cleanupTimerTick exectime = [", t, "]");
}

mt_const void
NvrCleaner::init (Timers      * const mt_nonnull timers,
                  RecpathConfig * const mt_nonnull recpathConfig,
                  ConstMemory   const stream_name,
                  Time          const max_age_sec,
                  Time          const clean_interval_sec)
{
    //this->vfs = vfs;
    this->m_recpathConfig = recpathConfig;
    this->stream_name = st_grab (new (std::nothrow) String (stream_name));
    this->max_age_sec = max_age_sec;

    logD (cleaner, _func_, "stream_name = ", this->stream_name);
    logD (cleaner, _func_, "max_age_sec = ", this->max_age_sec);

    this->timers = timers;

    this->timer_key = timers->addTimer (CbDesc<Timers::TimerCallback> (cleanupTimerTick, this, this),
                      5    /* time_seconds */,
                      true /* periodical */,
                      false /* auto_delete */);

    MOMENT_FFMPEG__NVR_CLEANER
}

NvrCleaner::NvrCleaner(): timers(this), timer_key (NULL), m_recpathConfig(NULL)
{

}

NvrCleaner::~NvrCleaner()
{
    if (this->timer_key) {
        this->timers->deleteTimer (this->timer_key);
        this->timer_key = NULL;
    }
    m_recpathConfig = NULL;
}

}

