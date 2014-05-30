
#ifndef MOMENT_FFMPEG__CHANNEL_CHECKER__H__
#define MOMENT_FFMPEG__CHANNEL_CHECKER__H__

#include <vector>
#include <map>
#include <string>
#include <utility>
#include <fstream>

#include <moment/libmoment.h>
#include <moment-ffmpeg/nvr_file_iterator.h>

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
    typedef std::map<std::string, ChChDiskTimes> ChannelFileDiskTimes; // [filename,[diskname,[start time, end time]]]
    typedef std::map<std::string, ChannelFileTimes> ChannelDiskFileTimes; // [diskname,[filename,[start time, end time]]]

    mt_const void init (Timers * mt_nonnull timers, const std::string & diskName, StRef<String> & channel_name);

    ChannelTimes GetChannelTimes ();
    ChannelFileTimes GetChannelFileTimes ();
    ChannelDiskFileTimes GetChannelDiskFileTimes ();
    ChannelFileDiskTimes GetChannelFileDiskTimes ();
    bool GetOldestFile (std::string & filename, std::string & disk);
    bool DeleteFromCache(const std::string & fileName);

    ChannelChecker ();
    ~ChannelChecker ();

private:
     StRef<String> m_channel_name;

     // cache of all records.
     //ChannelFileTimes m_chFileTimes;
     // cache is doubled for performance reason
     ChannelDiskFileTimes m_diskFileTimes;
     ChannelFileDiskTimes m_fileDiskTimes;

     StateMutex m_mutex;
     mt_const DataDepRef<Timers> m_timers;
     Timers::TimerKey m_timer_key;

     bool writeIdx(std::map<std::string, std::string> & files_changed);
     bool readIdx();

     CheckResult initCache ();
     CheckResult initCacheFromIdx ();
     CheckResult completeCache (bool bForceUpdate);
     CheckResult cleanCache ();
     CheckResult addRecordInCache (const std::string & path, const std::string & disk, bool bUpdate);

     void dumpData();

     static void refreshTimerTick (void *_self);

     typedef std::map<std::string, std::map<std::string, Uint64> > DiskFileSizes;
     DiskFileSizes m_occupSizes;
};

}

#endif //MOMENT_FFMPEG__CHANNEL_CHECKER__H__
