#include <algorithm>
#include <moment-ffmpeg/memory_dispatcher.h>
#include <mntent.h>
#include <sys/statvfs.h>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

#define FREESPACELIMIT 1024 * 100
#define BITRATE 1024 * 8
#define LIVETIMECACHE 5 * 60

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

static bool len_compare(const std::string a, const std::string b)
{
    return (a.length() < b.length());
}

std::string MemoryDispatcher::disknameFromFilename(const std::string & filename)
{
    std::string diskname;

    std::string pathToFile;
    size_t found = filename.rfind("/");
    pathToFile = filename.substr(0, found);

    // find disk where current file is written
    std::vector<std::string> diskNames;
    for (DiskFiles::const_iterator itr = _diskFiles.begin(); itr != _diskFiles.end(); itr++)
    {
        if(pathToFile.find(itr->first) == 0)
            diskNames.push_back(itr->first);
    }
    if(diskNames.size())
        diskname = *(std::max_element(diskNames.begin(), diskNames.end(), len_compare));

    return diskname;
}

Int64 MemoryDispatcher::getFreeSize(const std::string & diskName, time_t curTime)
{
    Int64 resultFreeSize = 0;

    Int64 freeSize = 0;
    Int64 reservedSizeByStreams = 0;
    Int64 actualSizeOfStreams = 0;

#ifndef PLATFORM_WIN32

    struct statvfs info;
    if(statvfs (diskName.c_str(), &info))
    {
        logE_(_func_, "fail to get free space info");
        return -1;
    }
    freeSize = (info.f_bsize * info.f_bavail) / 1024; // in KBytes
#else // broken, need to fix
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
        return -1;

    BOOL fResult = pGetDiskFreeSpaceEx (drivePath,
                                     (PULARGE_INTEGER)&i64FreeBytesToCaller,
                                     (PULARGE_INTEGER)&i64TotalBytes,
                                     (PULARGE_INTEGER)&i64FreeBytes);
    if (!fResult)
        return -1;

    freeSize = i64FreeBytesToCaller / 1024; // in KBytes
#endif

    FilesInfo * pStreams = &_diskFiles[diskName];
    FilesInfo::iterator it = pStreams->begin();

    while(it != pStreams->end())
    {
        logD(memdisp, _func_, "filename = [", it->first.c_str(), "]");

        logD(memdisp, _func_, "timeCreation = [", it->second.fileTimes.timeStart, "]");
        logD(memdisp, _func_, "timeUpdate = [", it->second.fileTimes.timeUpdate, "]");
        if(curTime - it->second.fileTimes.timeUpdate > LIVETIMECACHE)
        {
            // expired record
            logD(memdisp, _func_, "erase expired file = [", it->first.c_str(), "]");
            pStreams->erase(it++);
        }
        else
        {
            reservedSizeByStreams += it->second.fileSizes.reservedSize;
            actualSizeOfStreams   += it->second.fileSizes.actualSize;
            ++it;
        }
    }

    logD(memdisp, _func_, "reservedSizeByStreams = [", reservedSizeByStreams, "]");
    logD(memdisp, _func_, "actualSizeOfStreams = [", actualSizeOfStreams, "]");
    logD(memdisp, _func_, "freeSize = [", freeSize, "]");

    resultFreeSize = freeSize + actualSizeOfStreams - reservedSizeByStreams;
    if(resultFreeSize < 0)
        resultFreeSize = 0;

    logD(memdisp, _func_, "resultFreeSize = [", resultFreeSize, "]");

    return resultFreeSize;
}

Int64 MemoryDispatcher::GetDiskFreeSizeFromDiskname(const std::string & diskName)
{
    g_mutexMemoryDispatcher.lock();

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t curTime = tv.tv_sec;

    // find disk where current file is written
    std::string disknameRes = "";
    std::vector<std::string> diskNames;
    for (DiskFiles::const_iterator itr = _diskFiles.begin(); itr != _diskFiles.end(); itr++)
    {
        if(diskName.find(itr->first) == 0)
            diskNames.push_back(itr->first);
    }
    if(diskNames.size())
        disknameRes = *(std::max_element(diskNames.begin(), diskNames.end(), len_compare));

    Int64 freeSize = getFreeSize(disknameRes, curTime);

    g_mutexMemoryDispatcher.unlock();

    return freeSize;
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

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t curTime = tv.tv_sec;

    logD(memdisp, _func_, "curTime = [", curTime, "]");

    Int64 nThreshold = FREESPACELIMIT;
    Int64 nBitrate = BITRATE;
    time_t liveTimeInCache = LIVETIMECACHE;
    logD(memdisp, _func_, "liveTimeInCache = [", liveTimeInCache, "]");

    Int64 requiredSize = (nBitrate / 8) * nDuration; // in KBytes

    logD(memdisp, _func_, "requiredSize = [", requiredSize, "]");

    std::string diskName = disknameFromFilename(fileName);

    Int64 freeSize = getFreeSize(diskName, curTime);

    if(freeSize < 0)
    {
        g_mutexMemoryDispatcher.unlock();
        return 0;
    }

    logD(memdisp, _func_, "freeSize = [", freeSize, "]");

    Int64 intres = freeSize - requiredSize;

    logD(memdisp, _func_, "nThreshold = [", nThreshold, "]");
    logD(memdisp, _func_, "intres = [", intres, "]");

    bool bRes = intres > nThreshold;

    if(bRes)
    {
        logD(memdisp, _func_, "add record");

        FileSizes fileSizes;
        fileSizes.reservedSize = requiredSize;
        fileSizes.actualSize = 0;

        FileTimes fileTimes;
        fileTimes.timeStart = curTime;
        fileTimes.timeUpdate = curTime;

        FileInfo fileInfo;
        fileInfo.fileSizes = fileSizes;
        fileInfo.fileTimes = fileTimes;

        FilesInfo * pStreams = &_diskFiles[diskName];
        pStreams->insert(std::make_pair(fileName, fileInfo));
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
        FilesInfo::iterator it = pFilesInfo->find(fileName);
        for(FilesInfo::iterator itr1 = pFilesInfo->begin(); itr1 != pFilesInfo->end(); itr1++)
        {
            logD(memdisp, _func_, "fileName in pFilesInfo[", itr1->first.c_str(), "]");
        }
        if(it != pFilesInfo->end())
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            time_t curTime = tv.tv_sec;

            it->second.fileSizes.actualSize = size / 1024; // from Bytes to KB
            it->second.fileTimes.timeUpdate = curTime; // update time
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
