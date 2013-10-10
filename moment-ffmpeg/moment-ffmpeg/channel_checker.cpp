
#include <moment-ffmpeg/inc.h>

#include <moment-ffmpeg/channel_checker.h>
#include <moment-ffmpeg/media_reader.h>
#include <moment-ffmpeg/naming_scheme.h>


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static LogGroup libMary_logGroup_channelcheck ("mod_ffmpeg.channelcheck", LogLevel::D);

std::vector<std::pair<int,int>> *
ChannelChecker::getChannelExistence(ConstMemory const channel_name)
{
    logD_(_func_);
    CheckResult iRez = CheckResult_Success;
    this->existence.clear();
    iRez = check(channel_name, CheckMode_Timings);
    return (iRez == CheckResult_Success) ? &this->existence : NULL;
}

std::vector<ChannelChecker::ChannelFileSummary> *
ChannelChecker::getChannelFilesExistence(ConstMemory const channel_name)
{
    CheckResult iRez = CheckResult_Success;
    this->files_existence.clear();
    iRez = check(channel_name, CheckMode_FileSummary);

    return (iRez == CheckResult_Success) ? &this->files_existence : NULL;
}

ChannelChecker::CheckResult
ChannelChecker::check(ConstMemory const channel_name, CheckMode mode)
{
    logD_(_func_, "channel_name: [", channel_name, "]");
    CheckResult rez;
    NvrFileIterator file_iter;
    file_iter.init (this->vfs, channel_name, 0);
    while(1)
    {
        StRef<String> path = file_iter.getNext();
        if (path != NULL)
            rez = checkIdxFile(path, mode);
        else
            break;
    }

    concatenateSuccessiveIntervals();
    return rez;
}

ChannelChecker::CheckResult
ChannelChecker::checkIdxFile(StRef<String> path, CheckMode mode)
{
    logD (channelcheck, _func, "under processing flv file:", path->cstr());

    StRef<String> const flv_filename = st_makeString (path, ".flv");
//    Ref<Vfs::VfsFile> const flv_file = this->vfs->openFile (flv_filename->mem(),
//                                                      0 /* open_flags */,
//                                                      FileAccessMode::ReadOnly);
//    if (!flv_file)
//    {
//        logE_ (_func, "this->vfs->openFile() failed for filename ",
//               flv_filename, ": ", exc->toString());
//        return CheckResult_Failure;
//    }

//    Size bytes_read = 0;
//    Byte bufferStart [8];
//    Byte bufferEnd [8];
//    FileOffset offsetForEnd = -16;
//    IoResult res;
//    File * const file = flv_file->getFile();


//    res = file->read (Memory::forObject (bufferStart), &bytes_read);
//    if (res == IoResult::Error) {
//        logE_ (_func, "idx file read error: ", exc->toString());
//        return CheckResult_Failure;
//    }

//    if (!file->seek (offsetForEnd, SeekOrigin::End))
//    {
//        logE_ (_func, "seek failed", exc->toString());
//        return CheckResult_Failure;
//    }

//    res = file->read (Memory::forObject (bufferEnd), &bytes_read);
//    if (res == IoResult::Error) {
//        logE_ (_func, "idx file read error: ", exc->toString());
//        return CheckResult_Failure;
//    }

//    int const unixtime_timestamp_start = readTime (bufferStart);
//    int const unixtime_timestamp_end = readTime (bufferEnd);

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

    switch(mode)
    {
        case CheckMode_Timings:
            this->existence.push_back(std::make_pair(unixtime_timestamp_start, unixtime_timestamp_end));
        break;
        case CheckMode_FileSummary:
                    this->files_existence.push_back(ChannelFileSummary(unixtime_timestamp_start,
                                                                       unixtime_timestamp_end,
                                                                       std::string(path->cstr())));
        break;
        default:
            logE_ (_func, "WrongMode");
            return CheckResult_Failure;
    }

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

int
ChannelChecker::readTime (Byte * buf)
{
    Uint64 lRez =   ((Uint64) buf [0] << 56) |
                    ((Uint64) buf [1] << 48) |
                    ((Uint64) buf [2] << 40) |
                    ((Uint64) buf [3] << 32) |
                    ((Uint64) buf [4] << 24) |
                    ((Uint64) buf [5] << 16) |
                    ((Uint64) buf [6] <<  8) |
                    ((Uint64) buf [7] <<  0);
    int iRes = (double) lRez / 1000000000.0f;
    return iRes;
}

mt_const void
ChannelChecker::init (Vfs * const vfs, StRef<String> & record_dir)
{
    this->vfs = vfs;
    this->m_record_dir = record_dir;
}

ChannelChecker::ChannelChecker(){}

ChannelChecker::~ChannelChecker (){}

}
