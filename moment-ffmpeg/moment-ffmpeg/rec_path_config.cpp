#include <fstream>
#include <limits>
#include <json/json.h>
#include <moment-ffmpeg/memory_dispatcher.h>
#include <moment-ffmpeg/rec_path_config.h>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

#define MINFREESIZE 1024 * 200 // in KB

static LogGroup libMary_logGroup_recpath ("mod_ffmpeg.recpath", LogLevel::I);

RecpathConfig::RecpathConfig()
{
    m_bIsInit = false;
    m_configJson = "";
    m_pStreams = NULL;
}

RecpathConfig::~RecpathConfig()
{
    m_configs.clear();
    m_bIsInit = false;
    m_configJson = "";
    m_pStreams = NULL;
}

bool RecpathConfig::Init(const std::string & path_to_config,
                         std::map<std::string, WeakRef<FFmpegStream> > * pStreams)
{
    if(m_bIsInit || !pStreams)
        return false;

    m_pStreams = pStreams;

    return LoadConfig(path_to_config);
}

bool RecpathConfig::LoadConfig(const std::string & path_to_config)
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
        m_mutex.unlock();
        logE_(_func_, "fail to load config");
        return false;
    }

    bool parsingSuccessful = reader.parse( config_file, root, false );
    if(!parsingSuccessful)
    {
        m_mutex.unlock();
        logE_(_func_, "fail to parse config");
        return false;
    }

    Json::Value configs = root["configs"];
    if(configs.empty())
    {
        m_mutex.unlock();
        logE_(_func_, "fail to find \"configs\" section");
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
            m_configs.clear();
            m_mutex.unlock();
            logE_(_func_, "fail to parse params for section No ", itr.index());
            return false;
        }

        m_configs[path.asString()] = ConfigParam();
    }

    // dump config in log
    int i = 0;
    for(ConfigMap::const_iterator it = m_configs.begin(); it != m_configs.end(); ++it)
    {
        logD(recpath, _func_, "PathEntry ", i++);
        logD(recpath, _func_, "Path: ", it->first.c_str());
//        logD(recpath, _func_, "Quota: ", it->second.quota);
//        logD(recpath, _func_, "Mode: ", it->second.write_mode);
    }
    m_configJson = root.toStyledString();

    m_bIsInit = true;

    m_mutex.unlock();

    return true;
}

bool RecpathConfig::IsEmpty()
{
    return !m_configs.size();
}

bool RecpathConfig::IsInit()
{
    return m_bIsInit;
}

std::string RecpathConfig::GetConfigJson()
{
    return m_configJson;
}

std::string RecpathConfig::GetNextPath()
{
    return GetNextPath(std::string());
}

std::string RecpathConfig::GetNextPath(const std::string & prev_path)
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
    if(prev_path.size())
    {
        logD(recpath, _func_, "prev_path is ", prev_path.c_str());
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

std::string RecpathConfig::GetNextPathForStream()
{
    logD(recpath, _func_);

    m_mutex.lock();

    if(m_configs.size() == 0)
    {
        m_mutex.unlock();
        logD(recpath, _func_, "Config is empty");
        return std::string("");
    }
    else
        logD(recpath, _func_, "Config size is ", m_configs.size());

    std::string next_path;
    while(true)
    {
        for(ConfigMap::const_iterator it = m_configs.begin(); it != m_configs.end(); ++it)
        {
            Int64 freeSize = MemoryDispatcher::Instance().GetDiskFreeSizeFromDiskname(it->first);
            logD(recpath, _func_, "diskname is ", it->first.c_str());
            logD(recpath, _func_, "freeSize is ", freeSize);
            if(freeSize > MINFREESIZE)
            {
                next_path = it->first;
                logD(test, _func_, "next_path is ", next_path.c_str());
                break;
            }
        }

        if(next_path.size())
        {
            break;
        }
        else
        {
            logD(test, _func_, "next_path is empty");
        }

        // cleaning
        logD(test, _func_, "CLEANING");
        ChannelChecker::ChannelFileDiskTimes channelFileDiskTimes;
        std::map<std::string, WeakRef<FFmpegStream> >::iterator itFFStream = m_pStreams->begin();
        for(itFFStream; itFFStream != m_pStreams->end(); itFFStream++)
        {
            logD(recpath, _func_, "channel_name = ", itFFStream->first.c_str());
            Ref<ChannelChecker> channelChecker = itFFStream->second.getRefPtr()->GetChannelChecker();
            std::pair<std::string, ChChDiskTimes> oldestFileDiskTimes = channelChecker->GetOldestFileDiskTimes();
            logD(recpath, _func_, "filename = ", oldestFileDiskTimes.first.c_str());
            logD(recpath, _func_, "diskname = ", oldestFileDiskTimes.second.diskName.c_str());
            logD(recpath, _func_, "beginTime = ", oldestFileDiskTimes.second.times.timeStart);
            logD(recpath, _func_, "endTime = ", oldestFileDiskTimes.second.times.timeEnd);
            if(oldestFileDiskTimes.first.size())
            {
                channelFileDiskTimes.insert(oldestFileDiskTimes);
            }
        }

        // find most older file
        int minTime = std::numeric_limits<int>::max();
        std::string fileName;
        std::string diskName;
        ChannelChecker::ChannelFileDiskTimes::iterator iter = channelFileDiskTimes.begin();
        for(iter; iter != channelFileDiskTimes.end(); iter++)
        {
            if(minTime > iter->second.times.timeStart)
            {
                minTime = iter->second.times.timeStart;
                fileName = iter->first;
                diskName = iter->second.diskName;
            }
        }

        logD(recpath, _func_, "minTime = ", minTime);
        logD(recpath, _func_, "minfileName = ", fileName.c_str());
        logD(recpath, _func_, "mindiskName = ", diskName.c_str());

        // delete it
        StRef<String> dir_name = st_makeString(diskName.c_str());
        Ref<Vfs> vfs = Vfs::createDefaultLocalVfs (dir_name->mem());

        StRef<String> const filenameFull = st_makeString(fileName.c_str(), ".flv");

        logD(recpath, _func_, "filenameFull to remove = ", filenameFull);

        vfs->removeFile (filenameFull->mem());
        vfs->removeSubdirsForFilename (filenameFull->mem());

        std::string channel_name = fileName.substr(0,fileName.find("/"));
        (*m_pStreams)[channel_name].getRefPtr()->GetChannelChecker()->DeleteFromCache(diskName, fileName);
    }

    m_mutex.unlock();

    return next_path;
}

bool
RecpathConfig::IsPathExist(const std::string & path)
{
    logD(recpath, _func_);

    m_mutex.lock();

    ConfigMap::iterator itr = m_configs.find(path);
    bool bRes = (itr != m_configs.end());
    if(bRes)
        logD(recpath, _func_, "path [", path.c_str(), "] exist");
    else
        logD(recpath, _func_, "path [", path.c_str(), "] doesnt exist");

    m_mutex.unlock();

    return bRes;
}

}
