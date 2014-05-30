#include <algorithm>
#include <fstream>
#include <map>
#include <mntent.h>
#include <sys/statvfs.h>
#include <json/json.h>
#include <moment/path_manager.h>

#ifdef __linux__
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

using namespace M;

namespace Moment {

// rest of free space in Kb (200 Mb here)
#define RESTFREESPACE (1024*200)

// bitrate in Kb (8 Mb here)
#define BITRATE (1024*8)

static LogGroup libMary_logGroup_pathmgr ("path_manager", LogLevel::E);

StateMutex PathManager::g_mutexPathManager;

#ifdef __linux__
uint64_t get_dir_space(const char *dirname)
{
  logE(pathmgr, _func_, "input dirname: ", dirname);
  DIR *dir;
  struct dirent *ent;
  struct stat st;
  char buf[PATH_MAX];
  uint64_t totalsize = 0LL;

  if(!(dir = opendir(dirname)))
  {
    logE(pathmgr, _func_, "fail to open folder: ", dirname);
    return totalsize;
  }

  while((ent = readdir(dir)))
  {
    if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
      continue;

    sprintf(buf, "%s%s", dirname, ent->d_name);

    if(lstat(buf, &st) == -1)
    {
      logE(pathmgr, _func_, "Couldn't stat: ", buf);
      continue;
    }

    if(S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode))
    {
      uint64_t dirsize;

      strcat(buf, "/");
      dirsize = get_dir_space(buf);
      totalsize += dirsize;
    }
    else if(S_ISREG(st.st_mode)) // We only want to count regular files
    {
      totalsize += st.st_size;
    }
  }

  logE(pathmgr, _func_, "dirspace totalsize: ", totalsize);

  closedir(dir);
  return totalsize;
}

uint64_t get_file_space(const char *filepath)
{
  struct stat st;
  uint64_t totalsize = 0LL;

  if(lstat(filepath, &st) == -1)
  {
    logE(pathmgr, _func_, "Couldn't stat: ", filepath);
    return totalsize;
  }

  if(S_ISREG(st.st_mode)) // We only want to count regular files
  {
    totalsize = st.st_size;
  }

  return totalsize;
}
#endif

extern "C"
{
    unsigned long GetPermission(const char * filename, int64_t nDuration)
    {
        logD(pathmgr, _func_, "filename = ", filename, ", nDuration = ", nDuration);
        return PathManager::Instance().GetPermission(filename, nDuration);
    }

    int Notify(const char * filename, int bDone, unsigned long size)
    {
        logD(pathmgr, _func_, "filename = ", filename, ", size = ", size);
        return PathManager::Instance().Notify(filename, bDone != 0, size) ? 1: 0;
    }
}

Int64 GetDiskFreeSize(const std::string & diskName)
{

    struct statvfs info;
    if(statvfs (diskName.c_str(), &info))
    {
        logE_(_func_, "fail to get free space info");
        return -1;
    }
    Int64 freeSize = (info.f_bsize * info.f_bavail) / 1024; // in KBytes
    return freeSize;
}

/////////////////////////////////////////////////////////////////////////////////////////////

Int64 FullSrcSizes::GetTotalOccupSize()
{
    Int64 totalOccupSize = 0;
    for(SrcSizes::iterator itr = srcSizes.begin(); itr != srcSizes.end(); itr++)
    {
        totalOccupSize += itr->second.occupSize + itr->second.reservedFileSize;
    }
    return totalOccupSize;
}

bool FullSrcSizes::FindSrc(const std::string & src)
{
    return srcSizes.find(src) != srcSizes.end();
}

Int64 FullSrcSizes::GetWritingSrcCount()
{
    Int64 writingSrcCount = 0;
    for(SrcSizes::iterator itr = srcSizes.begin(); itr != srcSizes.end(); itr++)
    {
        writingSrcCount += itr->second.writing ? 1 : 0;
    }
    return writingSrcCount;
}

Int64 Quota::GetTotalSize()
{
    Int64 totalSize = 0;
    for(DiskSrcSizes::iterator itr = diskSrcSizes.begin(); itr != diskSrcSizes.end(); itr++)
    {
        totalSize += itr->second.totalSize;
    }
    return totalSize;
}

Int64 Quota::GetTotalOccupSize()
{
    Int64 totalOccupSize = 0;
    for(DiskSrcSizes::iterator itr = diskSrcSizes.begin(); itr != diskSrcSizes.end(); itr++)
    {
        totalOccupSize += itr->second.GetTotalOccupSize();
    }
    return totalOccupSize;
}

Int64 Quota::GetFreeSize()
{
    Int64 freeSize = 0;
    for(DiskSrcSizes::iterator itr = diskSrcSizes.begin(); itr != diskSrcSizes.end(); itr++)
    {
        freeSize += itr->second.totalSize - itr->second.GetTotalOccupSize();
    }
    return freeSize;
}

bool Quota::FindSrc(const std::string & src)
{
    bool bRes = false;
    for(DiskSrcSizes::iterator itr = diskSrcSizes.begin(); itr != diskSrcSizes.end(); itr++)
    {
        if(itr->second.FindSrc(src))
        {
            bRes = true;
            break;
        }
    }
    return bRes;
}

bool Quota::FindDisk(const std::string & diskname)
{
    return diskSrcSizes.find(diskname) == diskSrcSizes.end();
}

/////////////////////////////////////////////////////////////////////////////////////////////

PathManager& PathManager::Instance()
{
    static PathManager theSingleInstance;
    return theSingleInstance;
}

bool PathManager::Init(const std::string & path_to_config, const std::string & path_to_quota,
                       RemoveAllFilesCallback    removeAllFilesCallback,
                       RemoveFilesCallback       removeFilesCallback,
                       RemoveOldestFilesCallback removeOldestFilesCallback,
                       void * pFFmodule)
{
    logD(pathmgr, _func_);

    g_mutexPathManager.lock();

    //------------Reading config
    {
        Json::Value root;   // will contains the root value after parsing.
        Json::Reader reader;

        std::ifstream config_file(path_to_config, std::ifstream::binary);
        if(!config_file.good())
        {
            g_mutexPathManager.unlock();
            logE_(_func_, "fail to load config");
            return false;
        }

        bool parsingSuccessful = reader.parse( config_file, root, false );
        if(!parsingSuccessful)
        {
            g_mutexPathManager.unlock();
            logE_(_func_, "fail to parse config");
            return false;
        }

        Json::Value configs = root["configs"];
        if(configs.empty())
        {
            g_mutexPathManager.unlock();
            logE_(_func_, "fail to find \"configs\" section");
            return false;
        }

        for( Json::ValueIterator itr = configs.begin() ; itr != configs.end() ; itr++ )
        {
            Json::Value value = (*itr);

            Json::Value path = value["path"];

            if(path.empty())
            {
                logE_(_func_, "fail to parse params for section No ", itr.index());
            }
            else
            {
                std::string sPath = path.asString();
                // if path ended with '/', then drop it
                {
                    if(*sPath.rbegin() == '/')
                    {
                        sPath = sPath.substr(0,sPath.size()-1);
                    }
                }

                m_isInit = true; // at least one path exists
                std::vector<std::string>::iterator itr = std::find(m_disknames.begin(), m_disknames.end(), sPath);
                if(itr == m_disknames.end())
                {
                    m_disknames.push_back(sPath);
                }
            }
        }

        m_path_to_config = path_to_config;
        m_configJson = root.toStyledString();
    }

    m_removeAllFilesCallback = removeAllFilesCallback;
    m_removeFilesCallback = removeFilesCallback;
    m_removeOldestFileCallback = removeOldestFilesCallback;
    m_pFFmodule = pFFmodule;

    //------------Reading quota config
    m_path_to_quota = path_to_quota;
    {
        Json::Value root;   // will contains the root value after parsing.
        Json::Reader reader;
        bool bRes = true;

        std::ifstream config_file(path_to_quota, std::ifstream::binary);
        if(!config_file.good())
        {
            logE_(_func_, "fail to load quota config");
            bRes = false;
        }

        if(bRes)
        {
            bool parsingSuccessful = reader.parse( config_file, root, false );
            if(!parsingSuccessful)
            {
                logE_(_func_, "fail to parse quota config");
                bRes = false;
            }
        }

        Json::Value configs;
        if(bRes)
        {
            configs = root["quotas"];
            if(configs.empty())
            {
                logE_(_func_, "fail to find \"quotas\" section");
                bRes = false;
            }
        }

        if(bRes)
        {
            for( Json::ValueIterator itr = configs.begin() ; itr != configs.end() ; itr++ )
            {
                Json::Value value = (*itr);

                Json::Value jid = value["id"];
                Json::Value jquota = value["quota"];

                if(jid.empty() || jquota.empty())
                {
                    logE_(_func_, "fail to parse params for section No ", itr.index());
                }
                else
                {
                    Quota quota;
                    m_quotas[jid.asInt64()] = quota;

                    for(Json::ValueIterator itr0 = jquota.begin() ; itr0 != jquota.end() ; itr0++)
                    {
                        Json::Value value1 = (*itr0);
                        Json::Value jtotalsize = value1["totalSize"];
                        Json::Value jdiskname = value1["diskname"];
                        Json::Value jsources = value1["sources"];

                        if(jtotalsize.empty() || jdiskname.empty())
                        {
                            logE_(_func_, "fail to parse params for section No ", itr.index());
                        }
                        else
                        {
                            FullSrcSizes fss;
                            fss.totalSize = jtotalsize.asInt64();
                            for( Json::ValueIterator itr1 = jsources.begin() ; itr1 != jsources.end() ; itr1++ )
                            {
                                Json::Value value1 = (*itr1);
                                Json::Value srcName = value1["source"];
                                Json::Value writing = value1["writing"];
                                SpaceSizes ss;
                                ss.writing = writing.asBool();
                                fss.srcSizes[srcName.asString()] = ss;
                            }
                            m_quotas[jid.asInt64()].diskSrcSizes[jdiskname.asString()] = fss;
                        }
                    }
                }
            }
        }
    }

    g_mutexPathManager.unlock();

    return true;
}

bool PathManager::IsInit()
{
    return m_isInit;
}

std::string PathManager::GetConfigJson()
{
    return m_configJson;
}

bool PathManager::CreateOldSrc(const std::string & src)
{
    logD(pathmgr, _func_, "src = [", src.c_str(), "]");

    bool bRes = true;

    g_mutexPathManager.lock();

    if(!m_isInit)
    {
        g_mutexPathManager.unlock();
        logE_(_func_, "isnt inited");
        return false;
    }

    // is src present in quotas
    bool bExistInQuota = false;
    IdQuota::iterator itr = m_quotas.begin();
    for(itr; itr != m_quotas.end(); itr++)
    {
        if(itr->second.FindSrc(src))
        {
            bExistInQuota = true;
            break;
        }
    }

    if(!bExistInQuota)
    {
        g_mutexPathManager.unlock();
        logE_(_func_, "such src doesnt exists in quotas (very strange)");
        return false;
    }

    // calc occupSize for existing src
    for(DiskSrcSizes::iterator itr1 = itr->second.diskSrcSizes.begin(); itr1 != itr->second.diskSrcSizes.end(); itr1++)
    {
        std::string diskname = itr1->first;
        for(SrcSizes::iterator itr2 = itr1->second.srcSizes.begin(); itr2 != itr1->second.srcSizes.end(); itr2++)
        {
            if(itr2->first.compare(src) == 0)
            {
                std::string src = itr2->first;
                std::string fullPath = diskname + std::string("/") + src + std::string("/");
                Int64 occupSize = get_dir_space(fullPath.c_str());
                itr2->second.occupSize = occupSize / 1024; // to Kb
                // TODO: if occupSize > totalSize, do smth
            }
        }
    }

    g_mutexPathManager.unlock();

    return bRes;
}

QuotaResult PathManager::CreateNewSrc(const std::string & src, Int64 id)
{
    logD(pathmgr, _func_, "src = [", src.c_str(), "]");

    QuotaResult bRes = QuotaResult::QuotaSuccess;

    g_mutexPathManager.lock();

    if(!m_isInit)
    {
        g_mutexPathManager.unlock();
        logE_(_func_, "isnt inited");
        return QuotaResult::QuotaErrOther;
    }

    if(m_quotas.find(id) == m_quotas.end())
    {
        g_mutexPathManager.unlock();
        logE_(_func_, "there's no such id ", id);
        return QuotaResult::QuotaErrNotFound;
    }

    IdQuota::iterator itr;

    // is src present in quotas
    bool bExistInQuota = false;
    itr = m_quotas.begin();
    for(itr; itr != m_quotas.end(); itr++)
    {
        if(itr->second.FindSrc(src))
        {
            bExistInQuota = true;
            break;
        }
    }

    if(bExistInQuota)
    {
        g_mutexPathManager.unlock();
        logE_(_func_, "src already exist in quotas");
        return QuotaResult::QuotaErrAlreadyExist;
    }

    // TODO: use dimension and time length for more precise distribution

    std::string diskname;
    std::multimap<Int64, std::string> loadDisk; // [writingSrcCount, diskname], sorted automatically
    itr = m_quotas.find(id);

    for(DiskSrcSizes::iterator itr1 = itr->second.diskSrcSizes.begin(); itr1 != itr->second.diskSrcSizes.end(); itr1++)
    {
        loadDisk.insert(std::pair<Int64, std::string>(itr1->second.GetWritingSrcCount(), itr1->first));
    }

    diskname = loadDisk.begin()->second;

    // add src
    DiskSrcSizes::iterator itrDss = itr->second.diskSrcSizes.find(diskname);
    SpaceSizes ss;
    ss.writing = true;
    itrDss->second.srcSizes[src] = ss;

    writeQuotaFile();

    g_mutexPathManager.unlock();

    return bRes;
}

QuotaResult PathManager::DeleteSrc(const std::string & src)
{
    QuotaResult res = QuotaResult::QuotaSuccess;

    g_mutexPathManager.lock();

    bool bDeleted = false;
    for(IdQuota::iterator itr = m_quotas.begin(); itr != m_quotas.end(); itr++)
    {
        for(DiskSrcSizes::iterator itr1 = itr->second.diskSrcSizes.begin();
            itr1 != itr->second.diskSrcSizes.end(); itr1++)
        {
            if(itr1->second.srcSizes.find(src) != itr1->second.srcSizes.end())
            {
                itr1->second.srcSizes.erase(src);
                m_removeAllFilesCallback(itr1->first, src);
                bDeleted = true;
            }
        }
        if(bDeleted)
            break;
    }

    if(bDeleted)
    {
        writeQuotaFile();
    }
    else
    {
        logE_(_func_, "there is no such src ", src.c_str());
        res = QuotaResult::QuotaErrNotFound;
    }

    g_mutexPathManager.unlock();

    return res;
}

std::string PathManager::GetPath(const std::string & src, const std::string & prevPath)
{
    logD(pathmgr, _func_, "src = [", src.c_str(), "]");

    IdQuota::iterator itr = m_quotas.begin();
    for(itr; itr != m_quotas.end(); itr++)
    {
        if(itr->second.FindSrc(src))
            break;
    }

    if(itr == m_quotas.end())
    {
        logE_(_func_, "src ", src.c_str(), " is not found");
        return std::string();
    }

    // if prevPath is not found in disks, then return first diskname
    // if prevPath is found, then return next diskname. if prevPath is last diskname, then return empty diskname
    std::string resPath;
    DiskSrcSizes::iterator itr1 = itr->second.diskSrcSizes.find(prevPath);
    if(itr1 == itr->second.diskSrcSizes.end())
    {
        resPath = itr->second.diskSrcSizes.begin()->first;
    }
    else
    {
        itr1++;
        if(itr1 != itr->second.diskSrcSizes.end())
        {
            resPath = itr->second.diskSrcSizes.begin()->first;
        }
    }

    logD(pathmgr, _func_, "path return = [", resPath.c_str(), "]");

    return resPath;
}

std::string PathManager::GetPathForRecord(const std::string & src)
{
    std::string diskname;

    g_mutexPathManager.lock();

    bool bQuit = false;
    for(IdQuota::iterator itr = m_quotas.begin(); itr != m_quotas.end(); itr++)
    {
        for(DiskSrcSizes::iterator itr1 = itr->second.diskSrcSizes.begin();
            itr1 != itr->second.diskSrcSizes.end(); itr1++)
        {
            SrcSizes::iterator itr2 = itr1->second.srcSizes.find(src);
            if(itr2 != itr1->second.srcSizes.end())
            {
                if(itr2->second.writing)
                {
                    diskname = itr1->first;
                    bQuit = true;
                    break;
                }
            }
        }
        if(bQuit)
            break;
    }

    if(!diskname.size())
    {
        logE_(_func_, "VERY strange error!");
    }

    g_mutexPathManager.unlock();

    return diskname;
}

unsigned long PathManager::GetPermission(const std::string & fileName, const Uint64 nDuration)
{
    logD(pathmgr, _func_, "fileName = [", fileName.c_str(), "]");

    g_mutexPathManager.lock();

    Uint64 nBitrate = BITRATE;
    Int64 requiredSize = (nBitrate / 8) * nDuration; // in KBytes

    Int64 id = -1;
    std::string src;
    std::string diskname;
    // find which disk and src the filename belongs to.
    for(IdQuota::iterator itrQ = m_quotas.begin(); itrQ != m_quotas.end(); itrQ++)
    {
        for(DiskSrcSizes::iterator itrD = itrQ->second.diskSrcSizes.begin(); itrD != itrQ->second.diskSrcSizes.end(); itrD++)
        {
            for(SrcSizes::iterator itrS = itrD->second.srcSizes.begin(); itrS != itrD->second.srcSizes.end(); itrS++)
            {
                // TODO: optimize!!!
                std::string fillSrcName = itrD->first + std::string("/") + itrS->first + std::string("/");
                size_t n = fileName.find(fillSrcName);
                if(n != std::string::npos)
                {
                    id = itrQ->first;
                    diskname = itrD->first;
                    src = itrS->first;
                    break;
                }
            }
        }
    }

    if(id < 0)
    {
        // strange fail
        logE_(_func_, "no such srcName or diskName");
        g_mutexPathManager.unlock();
        return 0;
    }

    IdQuota::iterator itrQuota = m_quotas.find(id);
    DiskSrcSizes::iterator itrPrevDisk = itrQuota->second.diskSrcSizes.find(diskname);

    bool bSameDisk = false;
    Int64 freeSizeOnDisk = itrPrevDisk->second.totalSize - itrPrevDisk->second.GetTotalOccupSize();
    if(freeSizeOnDisk > requiredSize)
    {
        itrPrevDisk->second.srcSizes[src].reservedFileSize = requiredSize;
        itrPrevDisk->second.srcSizes[src].writing = true;
        bSameDisk = true;
    }
//    // TODO: ? delete DOWNLOADS dir on current disk
//    {

//    }

    if(!bSameDisk)
    {
        // not enough memory on current disk, so find next appropriate disk
        // return 0 and set 'writing' value to next disk

        DiskSrcSizes::iterator itrNextDisk = itrPrevDisk;
        itrNextDisk++;

        bool bAdded = false;
        while(true)
        {
            if(itrNextDisk == itrQuota->second.diskSrcSizes.end())
                itrNextDisk = itrQuota->second.diskSrcSizes.begin();

            if(itrNextDisk == itrPrevDisk)
            {
                // fail to record on any disk
                break;
            }

            Int64 freeSizeOnDisk = itrNextDisk->second.totalSize - itrNextDisk->second.GetTotalOccupSize();
            if(freeSizeOnDisk > requiredSize)
            {
                itrPrevDisk->second.srcSizes[src].writing = false;
                itrNextDisk->second.srcSizes[src].writing = true;
                bAdded = true;
                break;
            }

            itrNextDisk++;
        }

        // if not enough space on all disks, so delete oldest records
        if(!bAdded)
        {
            // find all srcs belong to this quota
            std::vector<std::string> srcList;
            DiskSrcSizes::iterator itr1 = itrQuota->second.diskSrcSizes.begin();
            for(itr1; itr1 != itrQuota->second.diskSrcSizes.end(); itr1++)
            {
                SrcSizes::iterator itr2 = itr1->second.srcSizes.begin();
                for(itr2; itr2 != itr1->second.srcSizes.end(); itr2++)
                {
                    srcList.push_back(itr2->first);
                }
            }

            // delete oldest files from pathOut and check can we record on pathOut
            while(true)
            {
                std::string pathOut;
                std::string srcOut; 
                Int64 deletedSize = m_removeOldestFileCallback(m_pFFmodule, srcList, pathOut, srcOut);
                if(deletedSize < 0)
                {
                    // not enough size on all disks after deleting all files
                    // TODO: think to handle correctly
                    requiredSize = 0;
                    break;
                }
                m_quotas[id].diskSrcSizes[pathOut].srcSizes[srcOut].occupSize -= deletedSize;
                Int64 freeSizeOnDisk = m_quotas[id].diskSrcSizes[pathOut].totalSize - m_quotas[id].diskSrcSizes[pathOut].GetTotalOccupSize();
                if(freeSizeOnDisk > requiredSize)
                {
                    itrPrevDisk->second.srcSizes[src].writing = false;
                    m_quotas[id].diskSrcSizes[pathOut].srcSizes[src].writing = true;
                    if(pathOut.compare(itrPrevDisk->first) == 0)
                    {
                        m_quotas[id].diskSrcSizes[pathOut].srcSizes[src].reservedFileSize = requiredSize;
                        bSameDisk = true;
                    }
                    break;
                }
            }
        }
    }

    if(!bSameDisk)
    {
        // we dont write now. requiredSize = 0 will return error of writing for seg_muxer and it will force to change
        // record path in nvrData in ffmpeg_stream (call PathManager::GetNextPathForRecord)
        // and then seg_muxer will call PathManager::GetPermission and gain non-zero requiredSize
        requiredSize = 0;
    }

    g_mutexPathManager.unlock();

    return requiredSize * 1024; // result in bytes;
}

bool PathManager::Notify(const std::string & fileName, bool bDone, unsigned long size)
{
    bool bRes = true;

    g_mutexPathManager.lock();

    if(bDone)
    {
        Int64 id = -1;
        std::string src;
        std::string path;

        for(IdQuota::iterator itr = m_quotas.begin(); itr != m_quotas.end(); itr++)
        {
            // TODO: more clever algorithm for define disk and src
            for(DiskSrcSizes::iterator itr1 = itr->second.diskSrcSizes.begin(); itr1 != itr->second.diskSrcSizes.end(); itr1++)
            {
                if(fileName.find(itr1->first) != std::string::npos)
                {
                    for(SrcSizes::iterator itr2 = itr1->second.srcSizes.begin(); itr2 != itr1->second.srcSizes.end(); itr2++)
                    {
                        if(fileName.find(itr2->first) != std::string::npos)
                        {
                            id = itr->first;
                            path = itr1->first;
                            src = itr2->first;
                        }
                    }
                }
            }
        }

        if(id < 0)
        {
            g_mutexPathManager.unlock();
            logE_(_func_, "didnt found such file");
            return false;
        }

        size /= 1024; // translate to Kb

        m_quotas[id].diskSrcSizes[path].srcSizes[src].occupSize += size;
        m_quotas[id].diskSrcSizes[path].srcSizes[src].reservedFileSize = 0;
    }

    g_mutexPathManager.unlock();

    return bRes;
}

bool PathManager::writeQuotaFile()
{
    bool bRes = true;

    Json::Value json_root;
    Json::Value json_quotas;
    for(IdQuota::iterator itr = m_quotas.begin(); itr != m_quotas.end(); itr++)
    {
        Json::Value json_quota;
        json_quota["id"] = Json::Value::Int64(itr->first);

        Json::Value json_quota_entries;
        for(DiskSrcSizes::iterator itr1 = itr->second.diskSrcSizes.begin(); itr1 != itr->second.diskSrcSizes.end(); itr1++)
        {
            Json::Value json_quota_entry;
            json_quota_entry["totalSize"] = Json::Int64(itr1->second.totalSize);
            json_quota_entry["diskname"] = itr1->first;
            Json::Value json_sources;
            for(SrcSizes::iterator itr2 = itr1->second.srcSizes.begin(); itr2 != itr1->second.srcSizes.end(); itr2++)
            {
                Json::Value json_source;
                json_source["source"] = itr2->first;
                json_source["writing"] = itr2->second.writing;
                json_sources.append(json_source);
            }
            json_quota_entry["sources"] = json_sources;
            json_quota_entries.append(json_quota_entry);
        }

        json_quota["quota"] = json_quota_entries;

        json_quotas.append(json_quota);
    }

    json_root["quotas"] = json_quotas;

    logD(pathmgr, _func_, "m_path_to_quota = ", m_path_to_quota.c_str());
    std::ofstream qfile(m_path_to_quota);
    if(qfile.is_open())
    {
        logD(pathmgr, _func_, "qouta file is opened");

        Json::StyledWriter json_writer_styled;
        std::string str_json_quotas = json_writer_styled.write(json_root);

        logD(pathmgr, _func_, "str_json_quotas = ", str_json_quotas.c_str());

        qfile << str_json_quotas;

        qfile.close();
    }
    else
    {
        logE_(_func_, "cannot write in quota file [", m_path_to_quota.c_str(), "]");
        bRes = false;
    }

    return bRes;
}

bool PathManager::DumpAll()
{
    return false;
}

bool PathManager::GetDiskForSrc(const std::string & src, std::vector<std::string> & out)
{
    // TODO: think about necessity of locking, now it is unsafe

    for(IdQuota::iterator itrQ = m_quotas.begin(); itrQ != m_quotas.end(); itrQ++)
    {
        for(DiskSrcSizes::iterator itrD = itrQ->second.diskSrcSizes.begin(); itrD != itrQ->second.diskSrcSizes.end(); itrD++)
        {
            if(itrD->second.FindSrc(src))
            {
                out.push_back(itrD->first);
            }
        }
        if(out.size()) // src can exist on one quota only
            break;
    }

    return out.size();
}

bool PathManager::RemoveFiles(const std::string & src, Time timeStart, Time timeEnd)
{
    bool bRes = true;

    g_mutexPathManager.lock();

    Int64 id = -1;
    for(IdQuota::iterator itr = m_quotas.begin(); itr != m_quotas.end(); itr++)
    {
        if(itr->second.FindSrc(src))
        {
            id = itr->first;
        }
    }
    if(id < 0)
    {
        // TODO: some logE msg
        g_mutexPathManager.unlock();
        return false;
    }

    std::map<std::string,Int64> diskSizeDeleted;
    Int64 res = m_removeFilesCallback(m_pFFmodule, src, timeStart, timeEnd, diskSizeDeleted);
    if(res < 0)
    {
        // TODO: some logE msg2
        g_mutexPathManager.unlock();
        return false;
    }

    for(std::map<std::string,Int64>::iterator itr = diskSizeDeleted.begin(); itr != diskSizeDeleted.end(); itr++)
    {
        m_quotas[id].diskSrcSizes[itr->first].srcSizes[src].occupSize -= itr->second;
    }

    g_mutexPathManager.unlock();

    return bRes;
}

bool PathManager::RemoveExpiredFiles(const std::string & filename, const std::string & path, const std::string src)
{
    bool bRes = false;

    // TODO: do remove of file right here
    // TODO: do some message error

    g_mutexPathManager.lock();

    for(IdQuota::iterator itrQ = m_quotas.begin(); itrQ != m_quotas.end(); itrQ++)
    {
        DiskSrcSizes::iterator itrD = itrQ->second.diskSrcSizes.find(path);
        if(itrD != itrQ->second.diskSrcSizes.end())
        {
            SrcSizes::iterator itrS = itrD->second.srcSizes.find(src);
            if(itrS != itrD->second.srcSizes.end())
            {
                itrS->second.occupSize -= get_file_space(filename.c_str());
                bRes = true;
                break;
            }
        }
    }

    g_mutexPathManager.unlock();

    return bRes;
}

QuotaResult PathManager::AddQuota(Int64 id, Int64 size)
{
    logD(pathmgr, _func_);

    QuotaResult res = QuotaResult::QuotaSuccess;

    g_mutexPathManager.lock();

    if(m_quotas.find(id) != m_quotas.end())
    {
        g_mutexPathManager.unlock();
        logE_(_func_, "quota id ", id, " already exists");
        return QuotaResult::QuotaErrAlreadyExist;
    }

    // find freeSizes that actually not free (they were reserved for sources)
    std::map<std::string, Int64> quotaFreeSizes; // [diskname, freeSize]
    for(int i=0;i<m_disknames.size();i++)
    {
        Int64 freeSize = GetDiskFreeSize(m_disknames[i]);
        if(freeSize > 0)
            quotaFreeSizes[m_disknames[i]] = freeSize;
    }
    for(IdQuota::iterator itrQ = m_quotas.begin(); itrQ != m_quotas.end(); itrQ++)
    {
        for(DiskSrcSizes::iterator itrD = itrQ->second.diskSrcSizes.begin(); itrD != itrQ->second.diskSrcSizes.end(); itrD++)
        {
            if(quotaFreeSizes.find(itrD->first) != quotaFreeSizes.end())
            {
                Int64 occupSize = 0; // occupied size of all src of current disk for current quota
                for(SrcSizes::iterator itrS = itrD->second.srcSizes.begin(); itrS != itrD->second.srcSizes.end(); itrS++)
                {
                    occupSize += itrS->second.occupSize;
                }
                Int64 quotaFreeSize = itrD->second.totalSize - occupSize; // free size of current disk, owned by quota

                // calculated value quotaFreeSizes[itr1->first] can be really less than REAL free size that we can use for new quota.
                // it is because for better precision we must use not [itrS->second.occupSize], but [itrS->second.occupSize + itrS->second.writingNowSize],
                // where itrS->second.writingNowSize is size of current recording video file.
                // However it is not bad that we use rounding to less free size.
                quotaFreeSizes[itrD->first] -= quotaFreeSize;
            }
        }
    }
    
    std::map<std::string, Int64>::iterator itr = quotaFreeSizes.begin();
    while(itr != quotaFreeSizes.end())
    {
        if(itr->second <= 0)
            quotaFreeSizes.erase(itr);
        itr++;
    }

    // all disks and corresponded sizes for this quota
    std::map<std::string, Int64> diskTotalSize;
    bool bCreateQuota = false;
    Int64 quotaSize = size;
    for(std::map<std::string, Int64>::iterator itr = quotaFreeSizes.begin(); itr != quotaFreeSizes.end(); itr++)
    {
        if(itr->second > quotaSize)
        {
            diskTotalSize[itr->first] = quotaSize;
            bCreateQuota = true;
            break;
        }
        else
        {
            diskTotalSize[itr->first] = itr->second;
            quotaSize -= itr->second;
        }
    }

    if(bCreateQuota)
    {
        Quota quota;
        for(std::map<std::string, Int64>::iterator itr = diskTotalSize.begin(); itr != diskTotalSize.end(); itr++)
        {
            FullSrcSizes fss;
            fss.totalSize = itr->second;
            quota.diskSrcSizes[itr->first] = fss;
        }
        m_quotas[id] = quota;

        writeQuotaFile();
    }
    else
    {
        logE_(_func_, "not enough free space for new quota ", id);
        res = QuotaResult::QuotaErrSpace;
    }

    g_mutexPathManager.unlock();

    return res;
}

QuotaResult PathManager::UpdateQuota(Int64 id, Int64 size)
{
    logD(pathmgr, _func_);

    QuotaResult res = QuotaResult::QuotaSuccess;

    g_mutexPathManager.lock();

    res = QuotaResult::QuotaErrNotFound; // UpdateQuota is not implemented yet

//    if(m_quotas.find(id) == m_quotas.end())
//    {
//        g_mutexPathManager.unlock();
//        logD(pathmgr, _func_, "quota id=", id, " doesnt exist");
//        return false;
//    }

//    // TODO: new size can be greater than physical freesize, check it
//    m_quotas[id].totalSize = size;

//    writeQuotaFile();

    g_mutexPathManager.unlock();

    return res;
}

QuotaResult PathManager::RemoveQuota(Int64 id)
{
    logD(pathmgr, _func_);

    QuotaResult res = QuotaResult::QuotaSuccess;

    g_mutexPathManager.lock();

    IdQuota::iterator itr = m_quotas.find(id);
    if(itr == m_quotas.end())
    {
        logE_(_func_, "quota is not found");
        g_mutexPathManager.unlock();
        return QuotaResult::QuotaErrNotFound;
    }

    bool bQuotaIsEmpty = true;
    for(DiskSrcSizes::iterator itr1 = itr->second.diskSrcSizes.begin(); itr1 != itr->second.diskSrcSizes.end(); itr1++)
    {
        if(itr1->second.srcSizes.size()) // it's allowed to remove only quota without src
        {
            bQuotaIsEmpty = false;
            break;
        }
    }
    if(bQuotaIsEmpty)
    {
        m_quotas.erase(id);
        writeQuotaFile();
    }
    else
    {
        logE_(_func_, "quota contains src, forbid to remove");
        res = QuotaResult::QuotaErrNotEmpty;
    }

    g_mutexPathManager.unlock();

    return res;
}

bool PathManager::GetDiskOccupation(const std::string & src, std::map<std::string, Int64> & diskOccupSize)
{
    logD(pathmgr, _func_);

    bool bRes = true;

    g_mutexPathManager.lock();

    bool bFound = false;
    for(IdQuota::iterator itrQ = m_quotas.begin(); itrQ != m_quotas.end(); itrQ++)
    {
        for(DiskSrcSizes::iterator itrD = itrQ->second.diskSrcSizes.begin(); itrD != itrQ->second.diskSrcSizes.end(); itrD++)
        {
            for(SrcSizes::iterator itrS = itrD->second.srcSizes.begin(); itrS != itrD->second.srcSizes.end(); itrS++)
            {
                if(itrS->first.compare(src) == 0)
                {
                    diskOccupSize[itrD->first] = itrS->second.occupSize;
                    bFound = true;
                }
            }
        }
        if(bFound)
            break;
    }

    g_mutexPathManager.unlock();

    return bRes;
}

Quota PathManager::GetQuotaInfo(Int64 id)
{
    logD(pathmgr, _func_);

    // TODO: think about some guard
    if(id < 0)
        return Quota();

    g_mutexPathManager.lock();

    Quota sq;
    if(m_quotas.find(id) != m_quotas.end())
    {
        sq = m_quotas[id];
    }

    g_mutexPathManager.unlock();

    return sq;
}

IdQuota PathManager::GetQuotaList()
{
    logD(pathmgr, _func_);

    g_mutexPathManager.lock();

    IdQuota quotaList = m_quotas;

    g_mutexPathManager.unlock();

    return quotaList;
}

}
