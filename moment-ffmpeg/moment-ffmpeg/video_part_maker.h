#ifndef MOMENT__FFMPEG_VIDEO_PART_MAKER__H__
#define MOMENT__FFMPEG_VIDEO_PART_MAKER__H__

#include <moment/libmoment.h>
#include <libmary/types.h>
#include <sstream>
#include <map>
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

    bool Init(ChannelChecker::ChannelFileDiskTimes * channelFileDiskTimes,
              std::string & channel_name,
              Time          const start_unixtime_sec,
              Time          const end_unixtime_sec,
              HTTPServerResponse * resp);

    bool IsInit();

    bool InternalInit(AVFormatContext * format_ctx);
    void InternalDeinit();

    bool Process();

private:

    bool tryOpenNextFile();

    FileReader      m_fileReader;
    // const?
    ChannelChecker::ChannelFileDiskTimes * m_pChannelFileDiskTimes;
    // const?
    ChannelChecker::ChannelFileDiskTimes::iterator m_itr;

    bool m_bIsInit;
    StRef<String>   m_filepath;
    Time nStartTime;
    Time nCurFileStartTime;
    Time nCurFileShift;
    Time nEndTime;

    AVFormatContext *	m_pOutFormatCtx;

    Uint64 m_Id;
    HTTPServerResponse * m_pResp;
    std::ostream* m_out;

public /*static functions*/:
    static bool SendBuffer(Uint64 id, const unsigned char *buf, int size);
    static bool GetVpmId(const char * pszFileName, Uint64 & id);

private /*static variables*/:

    static Uint64                               g_Id;
    static std::map<Uint64, VideoPartMaker *>	g_VpmMap;
    static Mutex                                g_MapMutex;
};

}

#endif /* MOMENT__FFMPEG_VIDEO_PART_MAKER__H__ */
