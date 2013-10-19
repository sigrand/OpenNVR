
#include <moment-ffmpeg/inc.h>

#include <moment-ffmpeg/channel_checker.h>
#include <moment-ffmpeg/media_reader.h>
#include <moment-ffmpeg/naming_scheme.h>


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static LogGroup libMary_logGroup_channelcheck ("mod_ffmpeg.channelcheck", LogLevel::D);

std::vector<std::pair<int,int>> *
ChannelChecker::getChannelExistence()
{
    logD_(_func_);
    check();
    return &this->existence;
}

std::vector<ChannelChecker::ChannelFileSummary> *
ChannelChecker::getChannelFilesExistence()
{
    logD_(_func_);
    check();
    return &this->files_existence;
}

ChannelChecker::CheckResult
ChannelChecker::check()
{
    logD_(_func_, "channel_name: [", m_channel_name, "]");
    CheckResult rez;
    NvrFileIterator file_iter;
    file_iter.init (this->vfs, m_channel_name->mem(), 0);

    while(1)
    {
        StRef<String> path = file_iter.getNext();

        if (path != NULL)
        {
            std::vector<std::string>::iterator it = std::find (m_files.begin(), m_files.end(), path->cstr());
            if(it == m_files.end())
            {
                rez = checkIdxFile(path);
                m_files.push_back(path->cstr());
            }
        }
        else
        {
            break;
        }
    }

    concatenateSuccessiveIntervals();
    return rez;
}

ChannelChecker::CheckResult
ChannelChecker::checkIdxFile(StRef<String> path)
{
    logD (channelcheck, _func, "under processing flv file:", path->cstr());

    StRef<String> const flv_filename = st_makeString (path, ".flv");

    StRef<String> flv_filenameFull = st_makeString(m_record_dir, "/", flv_filename);

    FileReader fileReader;
    bool bRes = fileReader.Init(flv_filenameFull);
    logD_ (_func, "fileReader.Init bRes: ", bRes? 1: 0);
    Time timeOfRecord;

    FileNameToUnixTimeStamp().Convert(flv_filenameFull, timeOfRecord);
    int const unixtime_timestamp_start = timeOfRecord / 1000000000LL;
    int const unixtime_timestamp_end = unixtime_timestamp_start + (int)fileReader.GetDuration();

//    logD_ (_func, "unixtime_timestamp_start", unixtime_timestamp_start);
//    logD_ (_func, "(int)fileReader.GetDuration()", (int)fileReader.GetDuration());
//    logD_ (_func, "unixtime_timestamp_end", unixtime_timestamp_end);
    logD_ (_func, "idx ts for start and end [", unixtime_timestamp_start, ":", unixtime_timestamp_end, "]");

    this->existence.push_back(std::make_pair(unixtime_timestamp_start, unixtime_timestamp_end));
    this->files_existence.push_back(ChannelFileSummary(unixtime_timestamp_start,
                                                               unixtime_timestamp_end,
                                                               std::string(path->cstr())));

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

    self->check();
}

mt_const void
ChannelChecker::init (Timers * const mt_nonnull timers, Vfs * const vfs, StRef<String> & record_dir, StRef<String> & channel_name)
{
    this->vfs = vfs;
    this->m_record_dir = record_dir;
    this->m_channel_name = channel_name;

    check(); // initial fulfilling

    timers->addTimer (CbDesc<Timers::TimerCallback> (refreshTimerTick, this, this),
                      5    /* time_seconds */,
                      true /* periodical */,
                      false /* auto_delete */);
}

ChannelChecker::ChannelChecker(){}

ChannelChecker::~ChannelChecker (){}

}
