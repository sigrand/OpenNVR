#ifndef MOMENT__FFMPEG_RECPATH_CONFIG__H__
#define MOMENT__FFMPEG_RECPATH_CONFIG__H__

#include <string>
#include <map>
#include <moment/libmoment.h>
#include <moment-ffmpeg/ffmpeg_stream.h>

namespace MomentFFmpeg {

using namespace M;
using namespace Moment;

class FFmpegStream;

struct ConfigParam
{
    ConfigParam():quota(0),write_mode(0){}
    int quota;
    int write_mode; // 0 - read-write, 1 - read only
};

class RecpathConfig
{
    typedef std::map<std::string, ConfigParam > ConfigMap;

public:
    RecpathConfig();
    ~RecpathConfig();

    bool Init(const std::string & path_to_config, std::map<std::string, WeakRef<FFmpegStream> > * pStreams);
    bool LoadConfig(const std::string & path_to_config);

    std::string GetConfigJson();

    std::string GetNextPath();
    std::string GetNextPath(const std::string & prev_path);
    std::string GetNextPathForStream();

    bool IsPathExist(const std::string & path);

    bool IsInit();
    bool IsEmpty();

private:
    bool m_bIsEmpty;
    bool m_bIsInit;

    ConfigMap m_configs;
    std::string m_configJson;

    StateMutex m_mutex;
    std::map<std::string, WeakRef<FFmpegStream> > * m_pStreams;
};

}
#endif /* MOMENT__FFMPEG_RECPATH_CONFIG__H__ */
