#ifndef MOMENT__FFMPEG_TIME_CHECKER__H__
#define MOMENT__FFMPEG_TIME_CHECKER__H__

#include <libmary/types.h>
#include <moment/libmoment.h>

#ifdef LIBMARY_PERFORMANCE_TESTING
#include <libmary/istat_measurer.h>
#endif


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

class TimeChecker
#ifdef LIBMARY_PERFORMANCE_TESTING
	: public M::ITimeChecker
#endif
{
    Time m_time;

public:

	TimeChecker();
	void Start();
    virtual void Stop(Time * res);
#ifdef LIBMARY_PERFORMANCE_TESTING
    void SetPacketSize(int packetSize);
    virtual int GetPacketSize() { return m_packetSize; }
private:
    int m_packetSize;
#endif
};

}

#endif /* MOMENT__FFMPEG_TIME_CHECKER__H__ */
