#ifndef MOMENT__FFMPEG_VIDEO_PART_MAKER__H__
#define MOMENT__FFMPEG_VIDEO_PART_MAKER__H__

#include <moment/libmoment.h>
#include <libmary/types.h>
#include <moment-ffmpeg/media_reader.h>
#include <moment-ffmpeg/ffmpeg_stream.h>
#include <moment-ffmpeg/channel_checker.h>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

class VideoPartMaker: public Object
{
public:

    VideoPartMaker();
    ~VideoPartMaker();

    bool Init(ChannelChecker::ChannelFileDiskTimes & channelFileDiskTimes,
              std::string & channel_name,
              Time          const start_unixtime_sec,
              Time          const end_unixtime_sec,
              std::string & filePathOut);

    bool IsInit();

    bool Process();

private:

    bool tryOpenNextFile();

    FileReader      m_fileReader;
    nvrData         m_nvrData;
    ChannelChecker::ChannelFileDiskTimes m_channelFileDiskTimes;
    ChannelChecker::ChannelFileDiskTimes::iterator m_itr;

    bool bIsInit;
    StRef<String>   m_filepath;
    Time nStartTime;
    Time nCurFileStartTime;
    Time nCurFileShift;
    Time nEndTime;

};

}

#endif /* MOMENT__FFMPEG_VIDEO_PART_MAKER__H__ */
