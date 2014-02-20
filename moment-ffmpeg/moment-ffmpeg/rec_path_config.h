#ifndef MOMENT__FFMPEG_RECPATH_CONFIG__H__
#define MOMENT__FFMPEG_RECPATH_CONFIG__H__

#include <moment/libmoment.h>
#include <string>
#include <map>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

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

    bool LoadConfig(const std::string & path_to_config);

    std::string GetConfigJson();

    std::string GetNextPath();
    std::string GetNextPath(const std::string & prev_path);

    bool IsPathExist(const std::string & path);

    bool IsInit();
    bool IsEmpty();

private:
    bool m_bIsEmpty;
    bool m_bIsInit;

    ConfigMap m_configs;
    std::string m_configJson;

    StateMutex m_mutex;
};

}
#endif /* MOMENT__FFMPEG_RECPATH_CONFIG__H__ */
