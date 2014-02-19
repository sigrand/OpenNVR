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

struct FileSizes
{
    FileSizes():reservedSize(0),actualSize(0){};
    unsigned long reservedSize;
    unsigned long actualSize;
};

struct FileTimes
{
    FileTimes():timeStart(0),timeUpdate(0){};
    time_t timeStart;
    time_t timeUpdate;
};

struct FileInfo
{
    FileSizes fileSizes;
    FileTimes fileTimes;
};

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
	
    typedef std::map <std::string, FileInfo > FilesInfo; // [FileName, FileInfo]
    typedef std::map <std::string, FilesInfo> DiskFiles; // [DiskName, FilesInfo]

    DiskFiles _diskFiles;
};

}

#endif /* MOMENT__FFMPEG_MEMORY_DISPATCHER__H__ */
