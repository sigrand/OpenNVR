
#ifndef MOMENT_FFMPEG__CHANNEL_CHECKER__H__
#define MOMENT_FFMPEG__CHANNEL_CHECKER__H__

#include <vector>
#include <string>
#include <utility>

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

    typedef struct _ChannelFileSummary
    {
        int start;
        int end;
        std::string path;

        _ChannelFileSummary()
        {
            this->start = 0;
            this->end = 0;
            this->path = "";
        }

        _ChannelFileSummary(int start, int end, const std::string & path)
        {
            this->start = start;
            this->end = end;
            this->path = path;
        }

    } ChannelFileSummary;

    mt_const void init (Timers * mt_nonnull timers, Vfs * const vfs, StRef<String> & record_dir, StRef<String> & channel_name);
    std::vector<std::pair<int,int>> * getChannelExistence ();
    std::vector<ChannelFileSummary> * getChannelFilesExistence ();
    ChannelChecker ();
    ~ChannelChecker ();

private:
     Vfs * vfs;
     StRef<String> m_record_dir;
     StRef<String> m_channel_name;
     std::vector<std::pair<int,int>> existence;
     std::vector<std::string> m_files;
     std::vector<ChannelFileSummary> files_existence;

     void concatenateSuccessiveIntervals();
     CheckResult check ();
     CheckResult checkIdxFile (StRef<String> path);
     int readTime(Byte * buffer);

     static void refreshTimerTick (void *_self);
};

}

#endif //MOMENT_FFMPEG__CHANNEL_CHECKER__H__
