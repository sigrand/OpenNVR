#ifndef MOMENT__FFMPEG_TIME_CHECKER__H__
#define MOMENT__FFMPEG_TIME_CHECKER__H__

#include <libmary/types.h>
#include <moment/libmoment.h>
#include <sys/time.h>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

class TimeChecker
{
    Time m_time;

public:

	TimeChecker();

	void Start();

    void Stop(Time * res);
};

}

#endif /* MOMENT__FFMPEG_TIME_CHECKER__H__ */
