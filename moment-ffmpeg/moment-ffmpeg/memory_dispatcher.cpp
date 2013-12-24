#include <moment-ffmpeg/memory_dispatcher.h>
#include <sys/statvfs.h>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

StateMutex MemoryDispatcher::g_mutexMemoryDispatcher;

extern "C"
{
    unsigned long GetPermission(const char * filename, int64_t nDuration)
    {
        logD_(_func_, "filename = ", filename, ", nDuration = ", nDuration);
        return MemoryDispatcher::Instance().GetPermission(filename, nDuration);
    }

    int Notify(const char * filename, int bDone, unsigned long size)
    {
        logD_(_func_, "filename = ", filename, ", size = ", size);
        return MemoryDispatcher::Instance().Notify(filename, bDone != 0, size) ? 1: 0;
    }
}


unsigned long MemoryDispatcher::GetPermission(const std::string & fileName, const Uint64 nDuration)
{
    logD_(_func_);

    g_mutexMemoryDispatcher.lock();

    unsigned long nThreshold = 1024 * 100; // 100MB
    unsigned long nBitrate = 1024 * 8; // 8Mb/s
    std::string pathToFile;
    unsigned found = fileName.rfind("/");
    pathToFile = fileName.substr(0, found);

    time_t liveTimeInCache = 5 * 60; // 5 minutes
    logD_(_func_, "liveTimeInCache = [", liveTimeInCache, "]");

    unsigned long requiredSize = (nBitrate / 8) * nDuration; // in KBytes
    struct statvfs info;
    if(statvfs (pathToFile.c_str(), &info))
    {
        logE_(_func_, "fail to get free space info");
        g_mutexMemoryDispatcher.unlock();
        return 0;
    }

    unsigned long freeSize = (info.f_bsize * info.f_bavail) / 1024; // in KBytes

    logD_(_func_, "freeSize = [", freeSize, "]");

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t curTime = tv.tv_sec;

    logD_(_func_, "curTime = [", curTime, "]");

    unsigned long reservedSizeByStreams = 0;
    unsigned long actualSizeOfStreams = 0;

    MemManagerStreams::iterator it = _streams.begin();
    while(it != _streams.end())
    {
        logD_(_func_, "filename = [", it->first.c_str(), "]");
        logD_(_func_, "timeCreation = [", it->second.second, "]");
        if(curTime - it->second.second > liveTimeInCache)
        {
            // expired record
            logD_(_func_, "erase expired file = [", it->first.c_str(), "]");
            _streams.erase(it++);
        }
        else
        {
            reservedSizeByStreams += it->second.first.first;
            actualSizeOfStreams   += it->second.first.second;
            ++it;
        }
    }
    logD_(_func_, "reservedSizeByStreams = [", reservedSizeByStreams, "]");
    logD_(_func_, "reservedSizeByStreams = [", actualSizeOfStreams, "]");

    bool bRes = (freeSize + actualSizeOfStreams - reservedSizeByStreams - requiredSize) > nThreshold;

    if(bRes)
    {
        logD_(_func_, "add record");
        _streams[fileName] = std::make_pair(std::make_pair(requiredSize, 0), curTime);
    }
    else
    {
        logD_(_func_, "no add record");
        requiredSize = 0;
    }

    g_mutexMemoryDispatcher.unlock();

    return requiredSize * 1024; // result in bytes;
}

bool MemoryDispatcher::Notify(const std::string & fileName, bool bDone, unsigned long size)
{
    logD_(_func_, "fileName [", fileName.c_str(), "]");

    g_mutexMemoryDispatcher.lock();

    bool bRes = true;

    if(bDone)
    {
        logD_(_func_, "delete");
        _streams.erase(fileName);
    }
    else
    {
        MemManagerStreams::iterator it;
        it = _streams.find(fileName);
        if(it != _streams.end())
        {
            it->second.first.second = size / 1024; // from Bytes to KB
        }
        else
        {
            logE_(_func_, "stream not found!");
        }
    }

    g_mutexMemoryDispatcher.unlock();

    return bRes;
}

}
