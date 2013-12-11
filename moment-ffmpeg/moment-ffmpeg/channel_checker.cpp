#include <moment-ffmpeg/inc.h>

#include <moment-ffmpeg/channel_checker.h>
#include <moment-ffmpeg/media_reader.h>
#include <moment-ffmpeg/time_checker.h>
#include <moment-ffmpeg/naming_scheme.h>


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static LogGroup libMary_logGroup_channelcheck ("mod_ffmpeg.channelcheck", LogLevel::D);

bool ChannelChecker::writeIdx(ChannelFileSummary & files_existence,
                              StRef<String> const dir_name, StRef<String> const channel_name)
{
    TimeChecker tc;tc.Start();

    bool bRes = false;

    StRef<String> const idx_file = st_makeString (dir_name, "/", channel_name, "/", channel_name, ".idx");
    std::string path = idx_file->cstr();

    std::ofstream idxFile;
    idxFile.open(path);
    if (idxFile.is_open())
    {
        ChannelFileSummary::iterator it = files_existence.begin();
        for(it; it != files_existence.end(); ++it)
        {
            idxFile << it->first;
            idxFile << "|";
            idxFile << it->second.first;
            idxFile << "|";
            idxFile << it->second.second;
            idxFile << std::endl;
        }
        idxFile.close();
        logD_(_func_, "write successful, idxfile: ", path.c_str());
        bRes = true;
    }
    else
    {
        logD_(_func_, "fail to open idxFile: ", path.c_str());
        bRes = false;
    }

    Time t;tc.Stop(&t);
    logD_(_func_, "NvrData.writeIdx exectime = [", t, "]");

    return bRes;
}

bool ChannelChecker::readIdx(ChannelFileSummary & files_existence,
                             StRef<String> const dir_name, StRef<String> const channel_name)
{
    TimeChecker tc;tc.Start();

    bool bRes = false;

    StRef<String> const idx_file = st_makeString (dir_name, "/", channel_name, "/", channel_name, ".idx");
    std::string path = idx_file->cstr();
    std::ifstream idxFile;
    idxFile.open(path);

    std::string delimiter = "|";
    if (idxFile.is_open())
    {
        std::string line;
        while ( std::getline (idxFile, line) )
        {
            StRef<String> strToken;
            int initTime = 0;
            int endTime = 0;
            size_t pos = 0;

            pos = line.rfind(delimiter);
            strToken = st_makeString(line.substr(pos + delimiter.length()).c_str());
            strToInt32_safe(strToken->cstr(), &endTime);
            line.erase(pos);

            pos = line.rfind(delimiter);
            strToken = st_makeString(line.substr(pos + delimiter.length()).c_str());
            strToInt32_safe(strToken->cstr(), &initTime);
            line.erase(pos);

            files_existence[line] = std::make_pair(initTime, endTime);
        }
        idxFile.close();
        logD_(_func_, "read successful, idxfile: ", path.c_str());
        bRes = true;
    }
    else
    {
        logD_(_func_, "fail to open idxFile: ", path.c_str());
        bRes = false;
    }

    Time t;tc.Stop(&t);
    logD_(_func_, "NvrData.writeIdx exectime = [", t, "]");

    return bRes;
}

std::vector<std::pair<int,int>> *
ChannelChecker::getChannelExistence()
{
    logD_(_func_);

    cleanCache();
    completeCache(true);

    return &this->existence;
}

ChannelChecker::ChannelFileSummary *
ChannelChecker::getChannelFilesExistence()
{
    logD_(_func_);

    cleanCache();
    completeCache(true);

    return &this->files_existence;
}

ChannelChecker::CheckResult
ChannelChecker::initCache()
{
    logD_(_func_, "channel_name: [", m_channel_name, "]");

    TimeChecker tc;tc.Start();

    readIdx(files_existence, m_record_dir, m_channel_name);

    if(!files_existence.empty()) // if cache is not empty, then we add only new records
    {
        std::string lastfile = files_existence.rbegin()->first;
        StRef<String> strLastfile = st_makeString(lastfile.c_str());
        addRecordInCache(strLastfile, true);
    }

    recreateExistence();
    concatenateSuccessiveIntervals();

    Time t;tc.Stop(&t);
    logD_(_func_, "ChannelChecker.initCache exectime = [", t, "]");

    return CheckResult_Success;
}

ChannelChecker::CheckResult
ChannelChecker::completeCache(bool bUpdate)
{
    logD_(_func_, "channel_name: [", m_channel_name, "]");

    TimeChecker tc;tc.Start();

    Time timeOfRecord = 0;
    std::string lastfile = "";

    if(!files_existence.empty()) // if cache is not empty, then we add only new records
    {
        lastfile = files_existence.rbegin()->first;
        StRef<String> const flv_filename = st_makeString (lastfile.c_str(), ".flv"); // replace by read from idx ????
        FileNameToUnixTimeStamp().Convert(flv_filename, timeOfRecord);
        timeOfRecord = timeOfRecord / 1000000000LL;
    }

    CheckResult rez;
    NvrFileIterator file_iter;
    file_iter.init (this->vfs, m_channel_name->mem(), timeOfRecord);

    StRef<String> path = file_iter.getNext();
    while(path != NULL && !path->isNullString())
    {
        StRef<String> pathNext = file_iter.getNext();
        if (pathNext != NULL)
        {
            rez = addRecordInCache(path, true);
        }
        else
        {
            rez = addRecordInCache(path, bUpdate);
        }
        path = st_makeString(pathNext);
    }
    writeIdx(files_existence, m_record_dir, m_channel_name);
    concatenateSuccessiveIntervals();

    Time t;tc.Stop(&t);
    logD_(_func_, "ChannelChecker.completeCache exectime = [", t, "]");

    return rez;
}

ChannelChecker::CheckResult
ChannelChecker::cleanCache()
{
    logD_(_func_, "channel_name: [", m_channel_name, "]");

    if(files_existence.empty())
    {
        return CheckResult_Success;
    }

    TimeChecker tc;tc.Start();

    NvrFileIterator file_iter;
    file_iter.init (this->vfs, m_channel_name->mem(), 0); // get the oldest file

    StRef<String> path = file_iter.getNext();
    bool bNeedRecreateExistence = false;
    if(path != NULL)
    {
        Time timeOfFirstFile = 0;
        {
            StRef<String> const flv_filename = st_makeString (path, ".flv"); // replace by read from idx ??????
            FileNameToUnixTimeStamp().Convert(flv_filename, timeOfFirstFile);
            timeOfFirstFile = timeOfFirstFile / 1000000000LL;
        }

        ChannelFileSummary::iterator it = files_existence.begin();
        while(it != files_existence.end())
        {
            logD_(_func_, "file [", it->first.c_str(), ",{", it->second.first, ",", it->second.second, "}]");
            Time timeOfRecord = 0;
            StRef<String> const flv_filename = st_makeString ((*it).first.c_str(), ".flv");
            FileNameToUnixTimeStamp().Convert(flv_filename, timeOfRecord);
            timeOfRecord = timeOfRecord / 1000000000LL;

            logD_(_func_, "timeOfFirstFile: [", timeOfFirstFile, "], timeOfRecord: [", timeOfRecord, "]");
            if(timeOfRecord < timeOfFirstFile)
            {
                logD_(_func_, "clean it!");
                files_existence.erase(it++);

                bNeedRecreateExistence = true;
            }
            else
            {
                logD_(_func_, "all expired records have been deleted");
                break;
            }
        }

        if(bNeedRecreateExistence)
        {
            recreateExistence();
            concatenateSuccessiveIntervals();
        }
    }
    else
    {
        logD_(_func_, "there is no files at all, clean all cache");

        existence.clear();
        std::vector<std::pair<int,int>>().swap(existence); // clear reallocating

        files_existence.clear();
    }

    writeIdx(files_existence, m_record_dir, m_channel_name);

    Time t;tc.Stop(&t);
    logD_(_func_, "ChannelChecker.cleanCache exectime = [", t, "]");

    return CheckResult_Success;
}

ChannelChecker::CheckResult
ChannelChecker::recreateExistence()
{
    existence.clear();
    std::vector<std::pair<int,int>>().swap(existence);

    ChannelFileSummary::iterator it;
    for(it = files_existence.begin(); it != files_existence.end(); ++it)
    {
        existence.push_back(std::make_pair((*it).second.first,(*it).second.second));
    }
}

ChannelChecker::CheckResult
ChannelChecker::addRecordInCache(StRef<String> path, bool bUpdate)
{
    logD_ (_func_, "addRecordInCache:", path->cstr());

    TimeChecker tc;tc.Start();

    if(files_existence.find(std::string(path->cstr())) == files_existence.end() || bUpdate)
    {
        StRef<String> const flv_filename = st_makeString (path, ".flv");
        StRef<String> flv_filenameFull = st_makeString(m_record_dir, "/", flv_filename);

        FileReader fileReader;
        fileReader.Init(flv_filenameFull);
        Time timeOfRecord = 0;

        FileNameToUnixTimeStamp().Convert(flv_filenameFull, timeOfRecord);
        int const unixtime_timestamp_start = timeOfRecord / 1000000000LL;
        int const unixtime_timestamp_end = unixtime_timestamp_start + (int)fileReader.GetDuration();
        logD_(_func_, "(int)fileReader.GetDuration() = [", (int)fileReader.GetDuration(), "]");

        existence.push_back(std::make_pair(unixtime_timestamp_start, unixtime_timestamp_end));
        files_existence[std::string(path->cstr())] = std::make_pair(unixtime_timestamp_start, unixtime_timestamp_end);
    }

    Time t;tc.Stop(&t);
    logD_(_func_, "ChannelChecker.addRecordInCache exectime = [", t, "]");

    return CheckResult_Success;
}

void
ChannelChecker::concatenateSuccessiveIntervals()
{
    if(this->existence.size() < 2)
        return;

    std::vector<std::pair<int,int>>::iterator it = this->existence.begin() + 1;
    while(1)
    {
        if (it == this->existence.end())
            break;

        if ((it->first - (it-1)->second) < 6)
        {
            std::pair<int,int> concatenated = std::make_pair<int,int>((int)(it-1)->first, (int)it->second);
            std::vector<std::pair<int,int>>::iterator prev = it-1;
            std::vector<std::pair<int,int>>::iterator prevPrev = it-2;
            this->existence.erase(it);
            this->existence.erase(prev);
            this->existence.insert(prevPrev+1, concatenated);
            it = this->existence.begin() + 1;
        }
        else ++it;
    }
}

void
ChannelChecker::refreshTimerTick (void * const _self)
{
    ChannelChecker * const self = static_cast <ChannelChecker*> (_self);

    logD_(_func_);

    self->cleanCache();
    self->completeCache(false);
}

mt_const void
ChannelChecker::init (Timers * const mt_nonnull timers, Vfs * const vfs, StRef<String> & record_dir, StRef<String> & channel_name)
{
    this->vfs = vfs;
    this->m_record_dir = record_dir;
    this->m_channel_name = channel_name;

    logD_(_func_, "m_record_dir=", this->m_record_dir);
    logD_(_func_, "m_channel_name=", this->m_channel_name);

    initCache();
    dumpData();

    this->timers = timers;

    this->timer_key = timers->addTimer (CbDesc<Timers::TimerCallback> (refreshTimerTick, this, this),
                      5    /* time_seconds */,
                      true /* periodical */,
                      false /* auto_delete */);
}

void ChannelChecker::dumpData()
{
    logD_(_func_, "m_record_dir = [", m_record_dir, "]");
    logD_(_func_, "m_channel_name = [", m_channel_name, "]");

    logD_(_func_, "existence:");
    for(int i=0;i<existence.size();++i)
    {
        logD_(_func_, "existence[", i, "] = {", existence[i].first, ",", existence[i].second, "}");
    }

    logD_(_func_, "files_existence:");
    ChannelFileSummary::iterator it = files_existence.begin();
    for(it; it != files_existence.end(); ++it)
    {
        logD_(_func_, "file [", it->first.c_str(), "] = {", it->second.first, ",", it->second.second, "}");
    }
}

ChannelChecker::ChannelChecker(): timers(this),timer_key(NULL)
{

}

ChannelChecker::~ChannelChecker ()
{
    if (this->timer_key) {
        this->timers->deleteTimer (this->timer_key);
        this->timer_key = NULL;
    }
}

}
