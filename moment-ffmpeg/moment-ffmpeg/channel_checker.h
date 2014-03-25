
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
    std::string diskName;
    ChChTimes times;
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

    typedef std::map<std::string,Uint64> DiskSizes; // [diskName, size (in Kb)] - size of each diskName
    typedef std::vector<ChChTimes> ChannelTimes; // [start time, end time]
    typedef std::map<std::string, ChChTimes> ChannelFileTimes; // [filename,[start time, end time]]
    typedef std::map<std::string, ChannelFileTimes> ChannelDiskFileTimes; // [diskname,[filename,[start time, end time]]]

    typedef std::map<std::string, ChChDiskTimes> ChannelFileDiskTimes; // [filename,[diskname,[start time, end time]]]

    mt_const void init (Timers * mt_nonnull timers, RecpathConfig * recpathConfig, StRef<String> & channel_name);

    // return by value because ChannelFileDiskTimes should be available from different threads and relatively long time
    ChannelTimes GetChannelTimes ();
    ChannelFileDiskTimes GetChannelFileDiskTimes ();
    DiskSizes GetDiskSizes ();
    bool DeleteFromCache(const std::string & dir_name, const std::string & fileName);

    ChannelChecker ();
    ~ChannelChecker ();

private:
     RecpathConfig * m_recpathConfig;
     StRef<String> m_channel_name;

     // cache of all records. it is doubled to keep different convenient(low demand cpu operations) variants for media reader and writing idx
     ChannelDiskFileTimes m_chDiskFileTimes;
     ChannelFileDiskTimes m_chFileDiskTimes;

     StateMutex m_mutex;
     mt_const DataDepRef<Timers> m_timers;
     Timers::TimerKey m_timer_key;

     bool writeIdx(const std::string & dir_name, std::vector<std::string> & files_changed);
     bool readIdx();

     CheckResult initCache ();
     CheckResult initCacheFromIdx ();
     CheckResult completeCache ();
     CheckResult cleanCache ();
     CheckResult updateCache(bool bForceUpdate);
     CheckResult addRecordInCache (const std::string & path, const std::string & record_dir, bool bUpdate);

     void dumpData();

     static void refreshTimerTick (void *_self);
};

}

#endif //MOMENT_FFMPEG__CHANNEL_CHECKER__H__
