#include <fstream>
#include <json/json.h>
#include <moment-ffmpeg/rec_path_config.h>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static LogGroup libMary_logGroup_recpath ("mod_ffmpeg.recpath", LogLevel::I);

RecpathConfig::RecpathConfig()
{
    m_bIsEmpty = true;
    m_configJson = "";
}

RecpathConfig::~RecpathConfig()
{
    m_configs.clear();
    m_bIsEmpty = true;
    m_configJson = "";
}

bool RecpathConfig::LoadConfig(std::string path_to_config)
{
    logD(recpath, _func_);
    m_mutex.lock();

    m_configs.clear();
    m_bIsEmpty = true;

    Json::Value root;   // will contains the root value after parsing.
    Json::Reader reader;

    std::ifstream config_file(path_to_config, std::ifstream::binary);
    if(!config_file.good())
    {
        logE_(_func_, "fail to load config");
        m_mutex.unlock();
        return false;
    }

    bool parsingSuccessful = reader.parse( config_file, root, false );
    if(!parsingSuccessful)
    {
        logE_(_func_, "fail to parse config");
        m_mutex.unlock();
        return false;
    }

    Json::Value configs = root["configs"];
    if(configs.empty())
    {
        logE_(_func_, "fail to find \"configs\" section");
        m_mutex.unlock();
        return false;
    }

    for( Json::ValueIterator itr = configs.begin() ; itr != configs.end() ; itr++ )
    {
        Json::Value value = (*itr);

        Json::Value path = value["path"];
//        Json::Value quota = value["quota"];
//        Json::Value mode = value["mode"];

        if(path.empty())// || quota.empty() || mode.empty())
        {
            logE_(_func_, "fail to parse params for section No ", itr.index());
            m_configs.clear();
            m_mutex.unlock();
            return false;
        }

        std::vector<int> vec;
//        vec.push_back(quota.asInt());
//        int nMode = !mode.asString().compare("ro")? 0:
//                    !mode.asString().compare("rw")? 1: -1;

//        vec.push_back(nMode);

        m_configs[path.asString()] = vec;
    }
    // dump config in log
    int i = 0;
    for(ConfigMap::const_iterator it = m_configs.begin(); it != m_configs.end(); ++it)
    {
        logD(recpath, _func_, "PathEntry ", i++);
        logD(recpath, _func_, "Path: ", it->first.c_str());
//        logD(recpath, _func_, "Quota: ", it->second[0]);
//        logD(recpath, _func_, "Mode: ", it->second[1]);
    }
    m_configJson = root.toStyledString();

    m_bIsEmpty = false;

    m_mutex.unlock();

    return true;
}

bool RecpathConfig::IsEmpty()
{
    return m_bIsEmpty;
}

std::string RecpathConfig::GetConfigJson()
{
    return m_configJson;
}

std::string RecpathConfig::GetNextPath(const char * prev_path)
{
    logD(recpath, _func_);

    m_mutex.lock();

    if(m_configs.size() == 0)
    {
        m_mutex.unlock();
        logD(recpath, _func_, "Config is empty");
        return std::string("");
    }

    std::string next_path;
    if(prev_path)
    {
        logD(recpath, _func_, "prev_path is ", prev_path);
        ConfigMap::iterator itr = m_configs.find(std::string(prev_path));
        if(itr == m_configs.end())
        {
            // we dont know last path, so dont know next, so return first
            next_path = m_configs.begin()->first;
            logD(recpath, _func_, "prev_path not exist, so return first path [", next_path.c_str(), "]");
        }
        else
        {
            itr++;
            if(itr == m_configs.end())
            {
                // that was last path in list, so return empty path
                next_path = "";
                logD(recpath, _func_, "prev_path is last path, so return empty path");
            }
            else
            {
                // return next path
                next_path = itr->first;
                logD(recpath, _func_, "return next path [", next_path.c_str(), "]");
            }
        }
    }
    else
    {
        // if there's no prev path, so return first
        ConfigMap::iterator itr1 = m_configs.begin();
        next_path = itr1->first;
        logD(recpath, _func_, "return first path [", next_path.c_str(), "]");
    }

    m_mutex.unlock();

    return next_path;
}

bool
RecpathConfig::IsPathExist(const char * path)
{
    logD(recpath, _func_);

    m_mutex.lock();

    ConfigMap::iterator itr = m_configs.find(std::string(path));
    bool bRes = (itr != m_configs.end());
    if(bRes)
        logD(recpath, _func_, "path [", path, "] exist");
    else
        logD(recpath, _func_, "path [", path, "] doesnt exist");

    m_mutex.unlock();

    return bRes;
}

}
