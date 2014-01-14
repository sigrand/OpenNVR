#ifndef MOMENT__FFMPEG_MEMORY_DISPATCHER__H__
#define MOMENT__FFMPEG_MEMORY_DISPATCHER__H__

#include <moment/libmoment.h>
#include <utility>
#include <map>
#include <sys/time.h>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

class MemoryDispatcher
{
public:

    static MemoryDispatcher& Instance();

    unsigned long GetPermission(const std::string & fileName, const Uint64 nDuration);

    bool Notify(const std::string & fileName, bool bDone, unsigned long size = 0);

private:

    MemoryDispatcher(){};
    MemoryDispatcher(const MemoryDispatcher& root);
    MemoryDispatcher& operator=(const MemoryDispatcher&);

    static StateMutex g_mutexMemoryDispatcher;
    typedef std::map<std::string, std::pair<std::pair <unsigned long, unsigned long>, std::pair <time_t, time_t> > > MemManagerStreams;
    MemManagerStreams _streams; // [streamName, [[reservedSize, actualSize], [timeBegin, timeUpdate]]]
};

}

#endif /* MOMENT__FFMPEG_MEMORY_DISPATCHER__H__ */
