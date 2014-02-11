#include <moment-ffmpeg/time_checker.h>
#include <sys/time.h>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

TimeChecker::TimeChecker()
{
	m_time = 0;
}

void TimeChecker::Start()
{
	struct timeval tv;
    gettimeofday(&tv, NULL);
	m_time = tv.tv_sec * 1000000 + tv.tv_usec;
}
#ifdef LIBMARY_PERFORMANCE_TESTING
void TimeChecker::SetPacketSize(int packetSize)
{
    m_packetSize = packetSize;
}
#endif

void TimeChecker::Stop(Time * res)
{
	struct timeval tv;
    gettimeofday(&tv, NULL);
    Time time2 = tv.tv_sec * 1000000 + tv.tv_usec;
    *res = time2 - m_time;
}

}
