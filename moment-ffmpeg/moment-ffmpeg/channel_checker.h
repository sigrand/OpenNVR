
#ifndef MOMENT_FFMPEG__CHANNEL_CHECKER__H__
#define MOMENT_FFMPEG__CHANNEL_CHECKER__H__

#include <vector>
#include <map>
#include <string>
#include <utility>
#include <fstream>

#include <moment/libmoment.h>
#include <moment-ffmpeg/nvr_file_iterator.h>
#include <moment-ffmpeg/rec_path_config.h>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

class ChannelChecker : public Object
{
public:
    enum CheckResult
    {
        CheckResult_Success,
        CheckResult_Failure,
        CheckResult_ChannelNotFound
    };

    enum CheckMode
    {
        CheckMode_Timings,
        CheckMode_FileSummary
    };

    typedef std::map<std::string, std::pair<int,int> > ChannelFileSummary; // [filename, [start time, end time]]
    typedef std::vector<std::pair<int,int> > ChannelFileTimes; // [start time, end time]
    typedef std::map<std::string,std::pair<std::string,std::pair<int,int> > > ChannelFileDiskTimes; // [filename,[diskname,[start time, end time]]]

    mt_const void init (Timers * mt_nonnull timers, RecpathConfig * recpathConfig, StRef<String> & channel_name);
    ChannelFileTimes getChannelExistence ();
    ChannelFileDiskTimes getChannelFileDiskTimes ();
    bool DeleteFromCache(const std::string & dir_name, const std::string & fileName);
    ChannelChecker ();
    ~ChannelChecker ();

private:
     RecpathConfig * m_recpathConfig;
     StRef<String> m_channel_name;

     ChannelFileDiskTimes m_chFileDiskTimes;
     ChannelFileTimes m_chTimes;

     StateMutex m_mutex;
     mt_const DataDepRef<Timers> m_timers;
     Timers::TimerKey m_timer_key;

     bool writeIdx(ChannelFileSummary & files_existence, StRef<String> const dir_name, StRef<String> const channel_name);
     bool readIdx(StRef<String> const dir_name, StRef<String> const channel_name);
     void concatenateSuccessiveIntervals();
     CheckResult initCache ();
     CheckResult completeCache (bool bUpdate); // bUpdate means if it is true then update record that exists in cache
     CheckResult cleanCache ();
     CheckResult recreateExistence ();
     CheckResult addRecordInCache (StRef<String> path, StRef<String> record_dir, bool bUpdate);
     ChannelFileSummary getChFileSummary (const std::string & dirname);
     bool updateLastRecordInCache();

     int readTime(Byte * buffer);

     void dumpData();

     static void refreshTimerTick (void *_self);
};

}

#endif //MOMENT_FFMPEG__CHANNEL_CHECKER__H__
