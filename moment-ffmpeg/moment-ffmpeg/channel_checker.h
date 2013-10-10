
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

    mt_const void init (Vfs * const vfs, StRef<String> & record_dir);
    std::vector<std::pair<int,int>> * getChannelExistence (ConstMemory const channel_name);
    std::vector<ChannelFileSummary> * getChannelFilesExistence (ConstMemory const channel_name);
    ChannelChecker ();
    ~ChannelChecker ();

private:
     Vfs * vfs;
     StRef<String> m_record_dir;
     std::vector<std::pair<int,int>> existence;
     std::vector<ChannelFileSummary> files_existence;

     void concatenateSuccessiveIntervals();
     CheckResult check (ConstMemory const channel_name, CheckMode mode);
     CheckResult checkIdxFile (StRef<String> path, CheckMode mode);
     int readTime(Byte * buffer);
};

}

#endif //MOMENT_FFMPEG__CHANNEL_CHECKER__H__
