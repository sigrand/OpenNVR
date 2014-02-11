#ifndef MOMENT__FFMPEG_RECPATH_CONFIG__H__
#define MOMENT__FFMPEG_RECPATH_CONFIG__H__

#include <moment/libmoment.h>
#include <string>
#include <map>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

class RecpathConfig
{
    typedef std::map<std::string, std::vector<int> > ConfigMap; // [config_path, [quota, write_mode]], write_mode: 0 - readonly, 1 - read-write

public:
    RecpathConfig();
    ~RecpathConfig();

    bool LoadConfig(std::string path_to_config);

    std::string GetConfigJson();

    std::string GetNextPath(const char * prev_path = 0);

    bool IsPathExist(const char * path);

    bool IsEmpty();

private:
    bool m_bIsEmpty;

    ConfigMap m_configs;
    std::string m_configJson;

    StateMutex m_mutex;
};

}
#endif /* MOMENT__FFMPEG_RECPATH_CONFIG__H__ */
