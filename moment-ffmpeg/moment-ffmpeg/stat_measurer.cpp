#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <iterator>
#include <limits>

#include <moment-ffmpeg/inc.h>
#include <moment-ffmpeg/stat_measurer.h>

#ifndef PLATFORM_WIN32
#include <sys/times.h>
#endif

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

bool
getTotalCPU(Time * totalCPU)
{
    std::ifstream procFile;
    procFile.open("/proc/stat", std::ios::in);
    std::string lineRaw;
    std::string delim = "cpu ";
    std::string line;
    if (procFile.is_open())
    {
        while ( std::getline (procFile, lineRaw) )
        {
            size_t s = lineRaw.find(delim);
            if(std::string::npos != s)
            {
                line = lineRaw.substr(s + delim.size());
                break;
            }
        }
        procFile.close();
    }
    else
    {
        return false;
    }

    while(line[0] == ' ')
    {
        line = line.substr(1);
    }

    std::vector<std::string> tokens = split(line, ' ');

    size_t n = tokens.size();
    long * nums = new long [n];
    for(int i=0;i<n;++i)
    {
        nums[i] = atoi(tokens[i].c_str());
    }
    unsigned long long res = 0;
    for(int i=0;i<n;++i)
    {
        res += nums[i];
    }

    delete [] nums;

    *totalCPU = res;

    return true;
}

bool
getCurProcCPU(Time * utime)
{
    clock_t ct0;
    struct tms tms0;

    if ((ct0 = times (&tms0)) == -1)
        return false;

    *utime = tms0.tms_utime;    

    return true;
}

void
StatMeasurer::flushAll()
{
    m_minInOut = std::numeric_limits<int64_t>::max();
    m_maxInOut = 0;
	m_allInOut = 0;
	m_packetAmountInOut = 0;
	
    m_minInNvr = std::numeric_limits<int64_t>::max();
    m_maxInNvr = 0;
	m_allInNvr = 0;
	m_packetAmountInNvr = 0;

    m_minRAM = std::numeric_limits<int64_t>::max();
    m_maxRAM = 0;
    m_allRAM = 0;
    m_RAMCount = 0;

    m_user_util_min = std::numeric_limits<int64_t>::max();
    m_user_util_max = 0;
    m_user_util_all = 0;
    m_user_util_count = 0;
}

void
StatMeasurer::AddTimeInOut(Time t)
{
	m_allInOut += t;

	if(m_minInOut > t)
	{
		m_minInOut = t;
	}
	if(m_maxInOut < t)
	{
		m_maxInOut = t;
	}

	m_packetAmountInOut++;
}

void
StatMeasurer::AddTimeInNvr(Time t)
{
	m_allInNvr += t;

	if(m_minInNvr > t)
	{
		m_minInNvr = t;
	}
	if(m_maxInNvr < t)
	{
		m_maxInNvr = t;
	}

	m_packetAmountInNvr++;
}

bool
StatMeasurer::CheckRAM()
{
    size_t result = -1;
#ifndef PLATFORM_WIN32
    std::ifstream procFile;
    procFile.open("/proc/self/status", std::ios::in);
    std::string line;
    std::string match = "VmRSS:";
    if (procFile.is_open())
    {
        while ( std::getline (procFile, line) )
        {
            size_t s = line.find(match);
            if(std::string::npos != s)
            {
                line = line.substr(s + match.size());
                break;
            }
        }
        procFile.close();
    }
    else
    {
        logE_(_func_, "fail to open /proc/self/status");
        return false;
    }

    // now we got some string like <VmRSS:    811080 kB>, so retrive the num
    size_t lastSpace = line.find_last_of(' ');
    line = line.substr(0, lastSpace);
    lastSpace = line.find_last_of(' ');
    line = line.substr(lastSpace + 1);
    result = atoi(line.c_str());
#else
    int error = 0;
    result = ::getUsedRAM_(error);
    if(result == -1) {
        logE_(_func_, "getUsedRAM_ failed with GetLastError= ", error);
        return false;
    }
#endif

    m_allRAM += result;

    if(m_minRAM > result)
    {
        m_minRAM = result;
    }
    if(m_maxRAM < result)
    {
        m_maxRAM = result;
    }

    m_RAMCount++;

    return true;
}

bool
StatMeasurer::CheckCPU()
{
    double user_util = 0.0;
#ifndef PLATFORM_WIN32
    Time user_time_now = 0;
    Time all_proc_time_now = 0;

    getTotalCPU(&all_proc_time_now);
    getCurProcCPU(&user_time_now);

    if(all_proc_time_now != m_all_proc_time_prev)
        user_util = 100.0 * (user_time_now - m_user_time_prev) / (double)(all_proc_time_now - m_all_proc_time_prev);

    m_user_time_prev = user_time_now;
    m_all_proc_time_prev = all_proc_time_now;
#else /* PLATFORM_WIN32 */
    BOOL fSuccess = FALSE;
    m_pPerfDataHead = (LPBYTE) ::getPerformanceData_(L"230", INIT_OBJECT_BUFFER_SIZE);
    if (NULL == m_pPerfDataHead)
    {
        logE_(_func_, "GetPerformanceData in loop failed.");
        return false;
    }

    fSuccess = ::getCounterValues_(m_pPerfDataHead, 230, 6, &m_pCurrSample);
    if (FALSE == fSuccess)
    {
        logE_(_func_, "GetCounterValues failed.");
        return false;
    }

    ULONGLONG numerator = m_pCurrSample.Data - m_pPrevSample.Data;
    LONGLONG denominator = m_pCurrSample.Time - m_pPrevSample.Time;
    user_util = (double)(100 * numerator) / denominator;


    memcpy(&m_pPrevSample, &m_pCurrSample, sizeof(RAW_DATA));
#endif /* PLATFORM_WIN32 */

    m_user_util_all += user_util;

    if(m_user_util_min > user_util)
    {
        m_user_util_min = user_util;
    }
    if(m_user_util_max < user_util)
    {
        m_user_util_max = user_util;
    }

    m_user_util_count++;

    return true;
}


StatMeasure StatMeasurer::GetStatMeasure()
{
    StatMeasure stm;

    // packet passing from in to out
    stm.minInOut = m_minInOut;
    stm.maxInOut = m_maxInOut;

    if(m_packetAmountInOut != 0)
        stm.avgInOut = m_allInOut / m_packetAmountInOut;
    else
        stm.avgInOut = 0;

    stm.packetAmountInOut = m_packetAmountInOut;

    // packet passing from in to nvr
    stm.minInNvr = m_minInNvr;
    stm.maxInNvr = m_maxInNvr;

    if(m_packetAmountInNvr != 0)
        stm.avgInNvr = m_allInNvr / m_packetAmountInNvr;
    else
        stm.avgInNvr = 0;

    stm.packetAmountInNvr = m_packetAmountInNvr;

    // RAM measurement
    stm.minRAM = m_minRAM;
    stm.maxRAM = m_maxRAM;

    if(m_RAMCount != 0)
        stm.avgRAM = m_allRAM / m_RAMCount;
    else
        stm.avgRAM = 0;

    // CPU measurement
    // User CPU utilization (in percent)
    stm.user_util_min = m_user_util_min;
    stm.user_util_max = m_user_util_max;

    if(m_user_util_count != 0)
        stm.user_util_avg = m_user_util_all / m_user_util_count;
    else
        stm.user_util_avg = 0;

    flushAll();

    return stm;
}


void
StatMeasurer::refreshTimerTickCPURAM (void * const _self)
{
    StatMeasurer * const self = static_cast <StatMeasurer*> (_self);

    self->CheckRAM();
    self->CheckCPU();
}

mt_const void
StatMeasurer::Init (Timers * const mt_nonnull timers, int time_seconds)
{
    flushAll();

    this->timers = timers;

    if(time_seconds)
    {
        this->timer_keyCPURAM = this->timers->addTimer (CbDesc<Timers::TimerCallback> (refreshTimerTickCPURAM, this, this),
                  time_seconds,
                  true /* periodical */,
                  true /* auto_delete */);
    }

#ifndef PLATFORM_WIN32
    getTotalCPU(&m_all_proc_time_prev);
    getCurProcCPU(&m_user_time_prev);
#else /* PLATFORM_WIN32 */
    BOOL fSuccess = FALSE;
    m_pPerfDataHead = (LPBYTE) ::getPerformanceData_(L"230", INIT_OBJECT_BUFFER_SIZE);
    if (NULL == m_pPerfDataHead)
    {
        logE_(_func_, "GetPerformanceData failed.\n");
        return;
    }

    // Then, sample the "% Processor Time" counter for instance "0" of the moment object.
    fSuccess = ::getCounterValues_(m_pPerfDataHead, 230, 6, &m_pPrevSample);
    if (FALSE == fSuccess)
    {
        logE_(_func_, "GetCounterValues failed.\n");
    }

#endif /* PLATFORM_WIN32 */
}

StatMeasurer::StatMeasurer(): timers(this /* coderef_container */), timer_keyCPURAM (NULL)
{
    m_minInOut = std::numeric_limits<int64_t>::max();
    m_maxInOut = 0;
    m_allInOut = 0;
    m_packetAmountInOut = 0;

    m_minInNvr = std::numeric_limits<int64_t>::max();
    m_maxInNvr = 0;
    m_allInNvr = 0;
    m_packetAmountInNvr = 0;

    m_minRAM = std::numeric_limits<int64_t>::max();
    m_maxRAM = 0;
    m_allRAM = 0;
    m_RAMCount = 0;

    m_user_util_min = std::numeric_limits<int64_t>::max();
    m_user_util_max = 0;
    m_user_util_all = 0;
    m_user_util_count = 0;

    m_user_time_prev = 0;
    m_all_proc_time_prev = 0;

#ifdef PLATFORM_WIN32
    m_pPerfDataHead = NULL;
    memset(&m_pPrevSample, 0 ,sizeof(RAW_DATA));
    memset(&m_pCurrSample, 0 ,sizeof(RAW_DATA));
#endif
}

StatMeasurer::~StatMeasurer()
{
    if (this->timer_keyCPURAM) {
        this->timers->deleteTimer (this->timer_keyCPURAM);
        this->timer_keyCPURAM = NULL;
    }
}

}
