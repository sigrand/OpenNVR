#include <algorithm>
#include <moment-ffmpeg/memory_dispatcher.h>
#include <mntent.h>
#include <sys/statvfs.h>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static LogGroup libMary_logGroup_memdisp ("mod_ffmpeg.memory_dispatcher", LogLevel::E);

StateMutex MemoryDispatcher::g_mutexMemoryDispatcher;

extern "C"
{
    unsigned long GetPermission(const char * filename, int64_t nDuration)
    {
        logD(memdisp, _func_, "filename = ", filename, ", nDuration = ", nDuration);
        return MemoryDispatcher::Instance().GetPermission(filename, nDuration);
    }

    int Notify(const char * filename, int bDone, unsigned long size)
    {
        logD(memdisp, _func_, "filename = ", filename, ", size = ", size);
        return MemoryDispatcher::Instance().Notify(filename, bDone != 0, size) ? 1: 0;
    }
}

bool MemoryDispatcher::Init()
{
    // get all disks names
    logD(memdisp, _func_);

#ifdef __linux__

    g_mutexMemoryDispatcher.lock();

    if(_isInit)
    {
        g_mutexMemoryDispatcher.unlock();
        return true;
    }

    struct mntent *mnt;
    char const *table = "/etc/mtab";
    FILE *fp;

    fp = setmntent (table, "r");
    if (fp == NULL)
    {
        logE_(_func_, "fail to get disk info from ", table);
        g_mutexMemoryDispatcher.unlock();
        return false;
    }

    while ((mnt = getmntent (fp)))
    {
        struct statvfs info;
        if(statvfs (mnt->mnt_dir, &info))
        {
            logE_(_func_, "fail statvfs for ", mnt->mnt_dir);
            g_mutexMemoryDispatcher.unlock();
            return false;
        }
        if(info.f_frsize > 0 && info.f_blocks > 0)
        {
            std::string diskName = mnt->mnt_dir;
            FilesInfo filesInfo;
            _diskFiles[diskName] = filesInfo;
        }
    }

    if (endmntent (fp) == 0)
    {
        logE_(_func_, "fail to close mntent ", table);
        g_mutexMemoryDispatcher.unlock();
        return false;
    }

    _isInit = true;

    g_mutexMemoryDispatcher.unlock();

#else

    _isInit = false;

#endif

    return _isInit;
}

MemoryDispatcher& MemoryDispatcher::Instance()
{
    static MemoryDispatcher theSingleInstance;
    return theSingleInstance;
}

static bool len_compare(std::string a, std::string b)
{
    return (a.length() < b.length());
}

unsigned long MemoryDispatcher::GetPermission(const std::string & fileName, const Uint64 nDuration)
{
    logD(memdisp, _func_);

    g_mutexMemoryDispatcher.lock();

    if(!_isInit)
    {
        logE_(_func_, "MemoryDispatcher is not inited");
        g_mutexMemoryDispatcher.unlock();
        return 0;
    }

    unsigned long nThreshold = 1024 * 100; // 100MB
    unsigned long nBitrate = 1024 * 8; // 8Mb/s
    time_t liveTimeInCache = 5 * 60; // 5 minutes
    logD(memdisp, _func_, "liveTimeInCache = [", liveTimeInCache, "]");

    unsigned long requiredSize = (nBitrate / 8) * nDuration; // in KBytes
    unsigned long freeSize = -1;

#ifndef PLATFORM_WIN32
    // find disk where current file is written
    std::vector<std::string> diskNames;
    std::string diskName;
    for (DiskFiles::const_iterator itr = _diskFiles.begin(); itr != _diskFiles.end(); itr++)
    {
        if(fileName.find(itr->first) == 0)
            diskNames.push_back(itr->first);
    }
    if(diskNames.size())
        diskName = *(std::max_element(diskNames.begin(), diskNames.end(), len_compare));

    struct statvfs info;
    if(statvfs (diskName.c_str(), &info))
    {
        logE_(_func_, "fail to get free space info");
        g_mutexMemoryDispatcher.unlock();
        return 0;
    }
    freeSize = (info.f_bsize * info.f_bavail) / 1024; // in KBytes
#else
    typedef BOOL (WINAPI *P_GDFSE)(LPCTSTR, PULARGE_INTEGER,
                                      PULARGE_INTEGER, PULARGE_INTEGER);
    P_GDFSE pGetDiskFreeSpaceEx = NULL;
    unsigned __int64 i64FreeBytesToCaller,
                   i64TotalBytes,
                   i64FreeBytes;

    const char *pszDrive = pathToFile.c_str();
    char * drivePath = NULL;
    char szDrive[4];
    if (pszDrive[1] == ':')
    {
        szDrive[0] = pszDrive[0];
        szDrive[1] = ':';
        szDrive[2] = '\\';
        szDrive[3] = '\0';
        drivePath = szDrive;
    }

    pGetDiskFreeSpaceEx = (P_GDFSE)GetProcAddress (
                                  GetModuleHandle ("kernel32.dll"),
                                                   "GetDiskFreeSpaceExA");
    if (!pGetDiskFreeSpaceEx)
        return 0;

    BOOL fResult = pGetDiskFreeSpaceEx (drivePath,
                                     (PULARGE_INTEGER)&i64FreeBytesToCaller,
                                     (PULARGE_INTEGER)&i64TotalBytes,
                                     (PULARGE_INTEGER)&i64FreeBytes);
    if (!fResult)
        return 0;

    freeSize = i64FreeBytesToCaller / 1024; // in KBytes
#endif

    logD(memdisp, _func_, "freeSize = [", freeSize, "]");

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t curTime = tv.tv_sec;

    logD(memdisp, _func_, "curTime = [", curTime, "]");

    unsigned long reservedSizeByStreams = 0;
    unsigned long actualSizeOfStreams = 0;

    FilesInfo * pStreams = &_diskFiles[diskName];
    FilesInfo::iterator it = pStreams->begin();
    while(it != pStreams->end())
    {
        logD(memdisp, _func_, "filename = [", it->first.c_str(), "]");
        logD(memdisp, _func_, "timeCreation = [", it->second.second.first, "]");
        logD(memdisp, _func_, "timeUpdate = [", it->second.second.second, "]");
        if(curTime - it->second.second.second > liveTimeInCache)
        {
            // expired record
            logD(memdisp, _func_, "erase expired file = [", it->first.c_str(), "]");
            pStreams->erase(it++);
        }
        else
        {
            reservedSizeByStreams += it->second.first.first;
            actualSizeOfStreams   += it->second.first.second;
            ++it;
        }
    }
    logD(memdisp, _func_, "reservedSizeByStreams = [", reservedSizeByStreams, "]");
    logD(memdisp, _func_, "reservedSizeByStreams = [", actualSizeOfStreams, "]");

    bool bRes = (freeSize + actualSizeOfStreams - reservedSizeByStreams - requiredSize) > nThreshold;

    if(bRes)
    {
        logD(memdisp, _func_, "add record");
        pStreams->insert(std::make_pair(fileName, std::make_pair(std::make_pair(requiredSize, 0), std::make_pair(curTime, curTime))));
    }
    else
    {
        logD(memdisp, _func_, "no add record");
        requiredSize = 0;
    }

    g_mutexMemoryDispatcher.unlock();

    return requiredSize * 1024; // result in bytes;
}

bool MemoryDispatcher::Notify(const std::string & fileName, bool bDone, unsigned long size)
{
    logD(memdisp, _func_, "fileName [", fileName.c_str(), "]");

    g_mutexMemoryDispatcher.lock();

    if(!_isInit)
    {
        logE_(_func_, "MemoryDispatcher is not inited");
        g_mutexMemoryDispatcher.unlock();
        return false;
    }

    FilesInfo * pFilesInfo = NULL;
    // find all possible disks for current path
    // for example for path "/disk1/records/1.flv" possible path can be
    // "/" and "/disk1"
    // we need path with the greatest length
    std::vector<std::string> vec;
    for(DiskFiles::iterator itr = _diskFiles.begin(); itr != _diskFiles.end(); ++itr)
    {
        logD(memdisp, _func_, "diskName is ", itr->first.c_str());
        size_t found = fileName.find(itr->first);
        logD(memdisp, _func_, "found is ", found);
        if(found == 0)
        {
            logD(memdisp, _func_, "itr->first = ", itr->first.c_str());
            vec.push_back(itr->first);
            itr++;
        }
    }
    std::string strDisk = "";
    for(int i = 0; i < vec.size(); i++)
    {
        if(strDisk.size() < vec[i].size())
            strDisk = vec[i];
    }
    logD(memdisp, _func_, "strDisk is ", strDisk.c_str());
    // we found the disk where file is written, so get collections of all files for this disk
    pFilesInfo = &_diskFiles.find(strDisk)->second;

    bool bRes = true;

    if(bDone)
    {
        logD(memdisp, _func_, "delete");
        pFilesInfo->erase(fileName);
    }
    else
    {
        FilesInfo::iterator it;
        it = pFilesInfo->find(fileName);
        for(FilesInfo::iterator itr1 = pFilesInfo->begin(); itr1 != pFilesInfo->end(); itr1++)
        {
            logD(memdisp, _func_, "fileName in pFilesInfo[", itr1->first.c_str(), "]");
        }
        if(it != pFilesInfo->end())
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            time_t curTime = tv.tv_sec;

            it->second.first.second = size / 1024; // from Bytes to KB
            it->second.second.second = curTime; // update time
        }
        else
        {
            logE_(_func_, "stream [", fileName.c_str(), "] not found!");
        }
    }

    g_mutexMemoryDispatcher.unlock();

    return bRes;
}

}
