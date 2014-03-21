
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

struct ChChTimes
{
    int timeStart;
    int timeEnd;
};

struct ChChDiskTimes
{
    ChChTimes times;
    std::string diskName;
};

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

    typedef std::map<std::string, ChChTimes > ChannelFileSummary; // [filename, [start time, end time]]
    typedef std::vector<ChChTimes> ChannelFileTimes; // [start time, end time]
    typedef std::map<std::string, ChChDiskTimes> ChannelFileDiskTimes; // [filename,[diskname,[start time, end time]]]
    typedef std::map<std::string,Uint64> DiskSizes; // [diskName, size (in Kb)] - size of each diskName

    mt_const void init (Timers * mt_nonnull timers, RecpathConfig * recpathConfig, StRef<String> & channel_name);

    // return by value because ChannelFileDiskTimes should be available from different threads and relatively long time
    ChannelFileTimes getChannelExistence ();
    ChannelFileDiskTimes getChannelFileDiskTimes ();
    DiskSizes getDiskSizes ();
    bool DeleteFromCache(const std::string & dir_name, const std::string & fileName);
    ChannelChecker ();
    ~ChannelChecker ();

private:
     RecpathConfig * m_recpathConfig;
     StRef<String> m_channel_name;

     ChannelFileDiskTimes m_chFileDiskTimes; // cache of all records

     StateMutex m_mutex;
     mt_const DataDepRef<Timers> m_timers;
     Timers::TimerKey m_timer_key;

     bool writeIdx(const ChannelFileSummary & files_existence, StRef<String> const dir_name, std::vector<std::string> & files_changed);
     bool readIdx();

     CheckResult initCache ();
     CheckResult initCacheFromIdx ();
     CheckResult completeCache ();
     CheckResult cleanCache ();
     CheckResult updateCache(bool bForceUpdate);
     CheckResult addRecordInCache (StRef<String> path, StRef<String> record_dir, bool bUpdate);
     ChannelFileSummary getChFileSummary (const std::string & dirname);

     void dumpData();

     static void refreshTimerTick (void *_self);
};

}

#endif //MOMENT_FFMPEG__CHANNEL_CHECKER__H__
