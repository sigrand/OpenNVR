#ifndef MOMENT__FFMPEG_STAT_MEASURER__H__
#define MOMENT__FFMPEG_STAT_MEASURER__H__

#ifndef PLATFORM_WIN32
#include <sys/time.h>
#else
#include <time.h>
#include "windows_helper_funcs.h"
#endif

#include <moment/libmoment.h>
#include <libmary/types.h>
#ifdef LIBMARY_PERFORMANCE_TESTING
#include <libmary/istat_measurer.h>
#endif

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

#define STATFILE "./stat.txt"

struct StatMeasure
{
    StatMeasure()
    {
        minInOut = std::numeric_limits<int64_t>::max();
        maxInOut = 0;
        avgInOut = 0;
        packetAmountInOut = 0;

        minInNvr = std::numeric_limits<int64_t>::max();
        maxInNvr = 0;
        avgInNvr = 0;
        packetAmountInNvr = 0;

        minRAM = std::numeric_limits<int64_t>::max();
        maxRAM = 0;
        avgRAM = 0;

        user_util_min = std::numeric_limits<int64_t>::max();
        user_util_max = 0;
        user_util_avg = 0;
    }

    // packet passing from in to out
    Time minInOut;
    Time maxInOut;
    Time avgInOut;
    unsigned packetAmountInOut;

    // packet passing from in to nvr
    Time minInNvr;
    Time maxInNvr;
    Time avgInNvr;
    unsigned packetAmountInNvr;

    // RAM measurement
    Time minRAM;
    Time maxRAM;
    Time avgRAM;

    // CPU measurement
    // User CPU utilization (in percent)
    double user_util_min;
    double user_util_max;
    double user_util_avg;
};

class StatMeasurer: public Object
#ifdef LIBMARY_PERFORMANCE_TESTING
, public M::IStatMeasurer
#endif
{
public:

	StatMeasurer();
	~StatMeasurer();

	void AddTimeInNvr(Time t);
    virtual void AddTimeInOut(Time t);

    bool CheckCPU();
    bool CheckRAM();

    StatMeasure GetStatMeasure();

    mt_const void Init(Timers * mt_nonnull timers, int time_seconds = 0);

private:

    mt_const DataDepRef<Timers> timers;
    Timers::TimerKey timer_keyCPURAM;

    // current stats:

	// packet passing from in to out
	Time m_minInOut;
	Time m_maxInOut;
	Time m_allInOut;
	unsigned m_packetAmountInOut;
	
	// packet passing from in to nvr
	Time m_minInNvr;
	Time m_maxInNvr;
	Time m_allInNvr;
	unsigned m_packetAmountInNvr;

    // RAM measurement

    Time m_minRAM;
    Time m_maxRAM;
    Time m_allRAM;
    unsigned m_RAMCount;

    // CPU measurement
    // User CPU utilization (in percent)

    Time m_user_time_prev;
    Time m_all_proc_time_prev;

#ifdef PLATFORM_WIN32
    BYTE* m_pPerfDataHead;
    RAW_DATA m_pPrevSample;
    RAW_DATA m_pCurrSample;
#endif

    double m_user_util_min;
    double m_user_util_max;
    double m_user_util_all;
    unsigned m_user_util_count;




    static void refreshTimerTickCPURAM (void *_self);

    void resetAll();

};

}

#endif /* MOMENT__FFMPEG_STAT_MEASURER__H__ */
