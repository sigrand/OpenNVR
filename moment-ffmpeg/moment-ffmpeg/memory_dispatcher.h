#ifndef MOMENT__FFMPEG_MEMORY_DISPATCHER__H__
#define MOMENT__FFMPEG_MEMORY_DISPATCHER__H__

#include <utility>
#include <map>

#include <sys/time.h>
#ifndef PLATFORM_WIN32
#include <sys/statvfs.h>
#else
#include <windows.h>
#endif

#include <moment/libmoment.h>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

class MemoryDispatcher
{
public:

    static MemoryDispatcher& Instance();

    unsigned long GetPermission(const std::string & fileName, const Uint64 nDuration);

    bool Notify(const std::string & fileName, bool bDone, unsigned long size = 0);

    bool Init();

private:

    MemoryDispatcher():_isInit(false){};
    MemoryDispatcher(const MemoryDispatcher& root);
    MemoryDispatcher& operator=(const MemoryDispatcher&);

    bool _isInit;

    static StateMutex g_mutexMemoryDispatcher;
    typedef std::pair <unsigned long, unsigned long> FileSizes;                 // [reservedSize, actualSize]
    typedef std::pair <time_t, time_t> FileTimes;                               // [timeStart, timeUpdate]
    typedef std::map <std::string, std::pair <FileSizes, FileTimes> > FilesInfo; // [FileName, [FileSizes, FileTimes]]
    typedef std::map <std::string, FilesInfo> DiskFiles;                         // [DiskName, FilesInfo]

    DiskFiles _diskFiles;
};

}

#endif /* MOMENT__FFMPEG_MEMORY_DISPATCHER__H__ */
