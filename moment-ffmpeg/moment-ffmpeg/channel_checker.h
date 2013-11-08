
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

    typedef std::map<std::string, std::pair<int,int> > ChannelFileSummary;

    mt_const void init (Timers * mt_nonnull timers, Vfs * const vfs, StRef<String> & record_dir, StRef<String> & channel_name);
    std::vector<std::pair<int,int>> * getChannelExistence ();
    ChannelFileSummary * getChannelFilesExistence ();
    bool writeIdx(ChannelFileSummary & files_existence,
                  StRef<String> const dir_name, StRef<String> const channel_name);
    bool readIdx(ChannelFileSummary & files_existence,
                 StRef<String> const dir_name, StRef<String> const channel_name);
    ChannelChecker ();
    ~ChannelChecker ();

private:
     Vfs * vfs;
     StRef<String> m_record_dir;
     StRef<String> m_channel_name;
     std::vector<std::pair<int,int>> existence;
     ChannelFileSummary files_existence;

     mt_const DataDepRef<Timers> timers;
     Timers::TimerKey timer_key;

     void concatenateSuccessiveIntervals();
     CheckResult initCache ();
     CheckResult completeCache ();
     CheckResult cleanCache ();
     CheckResult recreateExistence ();
     CheckResult addRecordInCache (StRef<String> path);

     int readTime(Byte * buffer);

     void dumpData();

     static void refreshTimerTick (void *_self);
};

}

#endif //MOMENT_FFMPEG__CHANNEL_CHECKER__H__
