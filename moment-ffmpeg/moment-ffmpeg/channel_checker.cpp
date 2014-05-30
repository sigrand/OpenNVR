#include <moment-ffmpeg/inc.h>

#include <moment/path_manager.h>
#include <moment-ffmpeg/channel_checker.h>
#include <moment-ffmpeg/media_reader.h>
#include <moment-ffmpeg/time_checker.h>
#include <moment-ffmpeg/naming_scheme.h>

#ifdef __linux__
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

#define CONCAT_INTERVAL 6
#define TIMER_TICK 5 // in seconds

static LogGroup libMary_logGroup_channelcheck ("mod_ffmpeg.channelcheck", LogLevel::E);

#ifdef __linux__
uint64_t get_dir_space(char *dirname)
{
  DIR *dir;
  struct dirent *ent;
  struct stat st;
  char buf[PATH_MAX];
  uint64_t totalsize = 0LL;

  if(!(dir = opendir(dirname)))
  {
    logE(channelcheck, _func_, "fail to open folder: ", dirname);
    return totalsize;
  }

  while((ent = readdir(dir)))
  {
    if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
      continue;

    sprintf(buf, "%s%s", dirname, ent->d_name);

    if(lstat(buf, &st) == -1)
    {
      logE(channelcheck, _func_, "Couldn't stat: ", buf);
      continue;
    }

    if(S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode))
    {
      uint64_t dirsize;

      strcat(buf, "/");
      dirsize = get_dir_space(buf);
      totalsize += dirsize;
    }
    else if(S_ISREG(st.st_mode)) // We only want to count regular files
    {
      totalsize += st.st_size;
    }
  }

  closedir(dir);
  return totalsize;
}
#endif

double FastGetDuration(const char * file_name_path)
{
    char durationStrLit[] = {0x64, 0x75, 0x72, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x00};
    const long buffSize = 256;

    double m_dDuration = 0.0;

    FILE * fp;
    fp = fopen(file_name_path, "rb");
    if(fp)
    {
        char buffer[buffSize]={0};
        int result = fread (buffer,1,buffSize,fp);

        char * pchBuffIterr;
        char * pchShift = NULL;
        char * pchBufEnd;
        char doubleBuf[8] = {};
        char * pDoubleBuf = (char *)&doubleBuf[0];

        if(buffer[0] == 'F' && buffer[1] == 'L' && buffer[2] == 'V')
        {

            pchBuffIterr = buffer;
            pchBufEnd = buffer + buffSize;

            while(	(pchShift == NULL) &&
                    (pchBuffIterr + 17) < pchBufEnd)
            {
                if(!memcmp(pchBuffIterr, durationStrLit, 9))
                    pchShift = pchBuffIterr + 9;

                pchBuffIterr++;
            }

            if(pchShift != NULL)
            {
                pDoubleBuf[0] = pchShift[7];
                pDoubleBuf[1] = pchShift[6];
                pDoubleBuf[2] = pchShift[5];
                pDoubleBuf[3] = pchShift[4];
                pDoubleBuf[4] = pchShift[3];
                pDoubleBuf[5] = pchShift[2];
                pDoubleBuf[6] = pchShift[1];
                pDoubleBuf[7] = pchShift[0];

                memcpy((void *)&m_dDuration, (void *)&doubleBuf, 8);
            }
        }

        fclose(fp);
    }

    // returns 0.0 in case of fail
    return m_dDuration;
}

bool ChannelChecker::writeIdx(std::map<std::string, std::string> & files_changed)
{
    TimeChecker tc;tc.Start();

    bool bRes = false;

    std::map<std::string, std::string> paths_idx;
    std::string path_prev;
    for(std::map<std::string, std::string>::iterator itr = files_changed.begin(); itr != files_changed.end(); itr++)
    {
        std::string path = itr->first.substr(0,itr->first.rfind("/"));
        if(path.compare(path_prev) != 0) // equal
        {
            path_prev = path;
            paths_idx[path] = itr->second;
        }
    }

    for(std::map<std::string, std::string>::iterator itrIdx = paths_idx.begin(); itrIdx != paths_idx.end(); itrIdx++)
    {
        StRef<String> const str_idx_file = st_makeString (itrIdx->second.c_str(), "/", itrIdx->first.c_str(), "/", m_channel_name, ".idx");
        std::string idxFileName = str_idx_file->cstr();
        std::ofstream idxFile;
        idxFile.open(idxFileName);
        logD(channelcheck, _func_,"ChannelChecker.writeIdx idxFileName = ", idxFileName.c_str());
        ChannelFileDiskTimes::const_iterator it = m_fileDiskTimes.begin();
        bool bIsDone = false;
        logD(channelcheck, _func_,"ChannelChecker.writeIdx m_fileDiskTimes.size() = ", m_fileDiskTimes.size());
        for(it; it != m_fileDiskTimes.end(); ++it)
        {
            std::string path = it->first.substr(0,it->first.rfind("/"));
            if(path.compare(itrIdx->first) == 0 && itrIdx->second.compare(it->second.diskName) == 0)
            {
                idxFile << it->first;
                idxFile << "|";
                idxFile << it->second.times.timeStart;
                idxFile << "|";
                idxFile << it->second.times.timeEnd;
                idxFile << std::endl;

                bIsDone = true;
                bRes = true;
            }
            else if (bIsDone) // there are no records left in files_existence for this idx
            {
                break;
            }
        }
        idxFile.close();
    }

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"ChannelChecker.writeIdx exectime = [", t, "]");

    return bRes;
}

bool ChannelChecker::readIdx()
{
    TimeChecker tc;tc.Start();

    bool bRes = false;

    std::vector<std::string> disks;
    PathManager::Instance().GetDiskForSrc(std::string(this->m_channel_name->cstr()), disks);

    for(int i = 0; i < disks.size(); i++)
    {
        StRef<String> strRecDir = st_makeString(disks[i].c_str());

        IdxFileIterator idx_iter;
        Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (strRecDir->mem());
        idx_iter.init (vfs, m_channel_name->mem(), 0);

        StRef<String> str_path = idx_iter.getNext();
        while(str_path != NULL)
        {
            StRef<String> full_str_path = st_makeString(strRecDir, "/", str_path);
            std::string path = full_str_path->cstr();
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

                    ChChTimes chChTimes;
                    chChTimes.timeStart = initTime;
                    chChTimes.timeEnd = endTime;

                    // fullfil m_chFileDiskTimes
                    {
                        ChChDiskTimes diskTimes;
                        diskTimes.diskName = disks[i];
                        diskTimes.times = chChTimes;

                        m_fileDiskTimes[line] = diskTimes;
                    }

                    // fullfil m_diskFileTimes
                    {
                        if(m_diskFileTimes.find(disks[i]) == m_diskFileTimes.end())
                        {
                            ChannelFileTimes cft;
                            m_diskFileTimes[disks[i]] = cft;
                        }

                        m_diskFileTimes[disks[i]][line] = chChTimes;
                    }
                }
                idxFile.close();
                logD(channelcheck, _func_, "read successful, idxfile: ", path.c_str());
                bRes = true;
            }
            else
            {
                logD(channelcheck, _func_, "fail to open idxFile: ", path.c_str());
                bRes = false;
            }

            str_path = idx_iter.getNext();
        }
    }

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_, "ChannelChecker.readIdx exectime = [", t, "]");

    return bRes;
}

ChannelChecker::ChannelTimes
ChannelChecker::GetChannelTimes()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    TimeChecker tc;tc.Start();

    cleanCache();
    completeCache(true);

    m_mutex.lock();

    ChannelTimes chFileTimes;

    chFileTimes.reserve(m_fileDiskTimes.size());
    for(ChannelFileDiskTimes::iterator it = m_fileDiskTimes.begin(); it != m_fileDiskTimes.end(); ++it)
    {
        chFileTimes.push_back(it->second.times);
    }

    if(chFileTimes.size() >= 2)
    {
        ChannelTimes::iterator itr = chFileTimes.begin();
        std::advance (itr,1);

        while(1)
        {
            if (itr == chFileTimes.end())
                break;

            std::vector<ChChTimes>::iterator it1 = itr;
            std::advance (it1,-1);
            if ((itr->timeStart - (it1)->timeEnd) < CONCAT_INTERVAL)
            {
                ChChTimes chChTimes;
                chChTimes.timeStart = (it1)->timeStart;
                chChTimes.timeEnd = (itr)->timeEnd;

                std::vector<ChChTimes>::iterator prev = itr;
                std::advance (prev,-1);
                std::vector<ChChTimes>::iterator prevPrev = itr;
                std::advance (prevPrev,-2);

                chFileTimes.erase(itr);
                chFileTimes.erase(prev);
                std::vector<ChChTimes>::iterator prevPrev1 = prevPrev;
                std::advance (prevPrev1,1);
                chFileTimes.insert(prevPrev1, chChTimes);
                itr = chFileTimes.begin();
                std::advance (itr,1);
            }
            else ++itr;
        }
    }

    m_mutex.unlock();

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"GetChannelTimes exectime = [", t, "]");

    return chFileTimes;
}

ChannelChecker::ChannelFileTimes
ChannelChecker::GetChannelFileTimes ()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    TimeChecker tc;tc.Start();

    cleanCache();
    completeCache(true);

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"getChannelFileDiskTimes exectime = [", t, "]");

    m_mutex.lock();

    ChannelFileTimes fileTimes;
    for(ChannelFileDiskTimes::iterator it = m_fileDiskTimes.begin(); it != m_fileDiskTimes.end(); ++it)
    {
        fileTimes.insert(std::pair<std::string, ChChTimes>(it->first, it->second.times));
    }

    m_mutex.unlock();

    return fileTimes;
}

ChannelChecker::ChannelDiskFileTimes
ChannelChecker::GetChannelDiskFileTimes ()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    cleanCache();
    completeCache(true);

    return m_diskFileTimes;
}

ChannelChecker::ChannelFileDiskTimes
ChannelChecker::GetChannelFileDiskTimes ()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    cleanCache();
    completeCache(true);

    return m_fileDiskTimes;
}

bool
ChannelChecker::GetOldestFile (std::string & filename, std::string & disk)
{
    bool bRes = true;

    m_mutex.lock();

    if(m_fileDiskTimes.size())
    {
        filename = m_fileDiskTimes.begin()->first;
        disk = m_fileDiskTimes.begin()->second.diskName;
    }
    else
    {
        bRes = false;
    }

    m_mutex.unlock();

    return bRes;
}

ChannelChecker::CheckResult
ChannelChecker::initCache()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    TimeChecker tc;tc.Start();

    m_mutex.lock();

    initCacheFromIdx();
    completeCache(true);

    m_mutex.unlock();

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"ChannelChecker.initCache exectime = [", t, "]");

    return CheckResult_Success;
}

ChannelChecker::CheckResult
ChannelChecker::initCacheFromIdx()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    TimeChecker tc;tc.Start();

    readIdx();

    if(!m_fileDiskTimes.empty()) // if cache is not empty, then we update only last record duration
    {
        ChannelFileDiskTimes::reverse_iterator itr = m_fileDiskTimes.rbegin();
        addRecordInCache(itr->first, itr->second.diskName, true);
    }

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"ChannelChecker.initCacheFromIdx exectime = [", t, "]");

    return CheckResult_Success;
}

bool
ChannelChecker::DeleteFromCache(const std::string & fileName)
{
    logD(channelcheck, _func_,"filename: [", fileName.c_str(), "]");

    m_mutex.lock();

    m_fileDiskTimes.erase(fileName);

    std::string disk_to_erase;
    for(ChannelDiskFileTimes::iterator itr = m_diskFileTimes.begin();
        itr != m_diskFileTimes.end(); itr++)
    {
        if(itr->second.find(fileName) != itr->second.end())
        {
            disk_to_erase = itr->first;
            itr->second.erase(fileName);
            break;
        }
    }

    if(disk_to_erase.size() != 0)
    {
        std::map<std::string, std::string> files_changed;
        files_changed[fileName] = disk_to_erase;
        writeIdx(files_changed);
    }

    m_mutex.unlock();

    return true;
}

ChannelChecker::CheckResult
ChannelChecker::completeCache(bool bForceUpdate)
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    TimeChecker tc;tc.Start();

    CheckResult rez = CheckResult_Success;

    std::vector<std::string> disks;
    PathManager::Instance().GetDiskForSrc(m_channel_name->cstr(), disks);

    for(int i = 0; i < disks.size(); i++)
    {
        Time timeOfRecord = 0;

        if(!m_diskFileTimes[disks[i]].empty())
        {
            std::string lastfile = m_diskFileTimes[disks[i]].rbegin()->first;
            StRef<String> const flv_filename = st_makeString (lastfile.c_str(), ".flv");
            FileNameToUnixTimeStamp().Convert(flv_filename, timeOfRecord);
            timeOfRecord = timeOfRecord / 1000000000LL;
        }

        NvrFileIterator file_iter;
        StRef<String> strRecDir = st_makeString(disks[i].c_str());
        Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (strRecDir->mem());
        file_iter.init (vfs, m_channel_name->mem(), timeOfRecord);

        std::map<std::string, std::string> files_changed;

        StRef<String> path = file_iter.getNext();
        while(path != NULL && !path->isNullString())
        {
            std::string strpath = std::string(path->cstr());

            StRef<String> pathNext = file_iter.getNext();
            if(pathNext != NULL && !pathNext->isNullString())
            {
                addRecordInCache(strpath, disks[i], true);
                files_changed[strpath] = disks[i];
            }
            else
            {
                addRecordInCache(strpath, disks[i], bForceUpdate);
                files_changed[strpath] = disks[i];
            }

            path = st_makeString(pathNext);
        }

        writeIdx(files_changed);
    }

    Time t;tc.Stop(&t);
    logD (channelcheck, _func_,"ChannelChecker.completeCache exectime = [", t, "]");

    return rez;
}

ChannelChecker::CheckResult
ChannelChecker::cleanCache()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    m_mutex.lock();

    if(m_diskFileTimes.empty())
    {
        m_mutex.unlock();
        return CheckResult_Success;
    }

    TimeChecker tc;tc.Start();

    for(ChannelDiskFileTimes::iterator itrD = m_diskFileTimes.begin(); itrD != m_diskFileTimes.end(); itrD++)
    {
        StRef<String> strRecDir = st_makeString(itrD->first.c_str());
        ConstMemory record_dir = strRecDir->mem();
        Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (record_dir);

        NvrFileIterator file_iter;
        file_iter.init (vfs, m_channel_name->mem(), 0); // get the oldest file

        std::map<std::string, std::string> files_changed;

        StRef<String> path = file_iter.getNext();
        if(path != NULL)
        {
            Time timeOfFirstFile = 0;
            {
                StRef<String> const flv_filename = st_makeString (path, ".flv"); // replace by read from idx ??????
                FileNameToUnixTimeStamp().Convert(flv_filename, timeOfFirstFile);
                timeOfFirstFile = timeOfFirstFile / 1000000000LL;
            }

            ChannelFileTimes::iterator itrF = itrD->second.begin();
            while(itrF != itrD->second.end())
            {
                logD(channelcheck, _func_,"file [", itrF->first.c_str(), ",{", itrF->second.timeStart, ",", itrF->second.timeEnd, "}]");
                Time timeOfRecord = 0;
                StRef<String> const flv_filename = st_makeString (itrF->first.c_str(), ".flv");
                FileNameToUnixTimeStamp().Convert(flv_filename, timeOfRecord);
                timeOfRecord = timeOfRecord / 1000000000LL;

                logD(channelcheck, _func_,"timeOfFirstFile: [", timeOfFirstFile, "], timeOfRecord: [", timeOfRecord, "]");
                if(timeOfRecord < timeOfFirstFile)
                {
                    logD(channelcheck, _func_,"clean it!");
                    files_changed[itrF->first] = itrD->first;
                    itrD->second.erase(itrF);
                    itrF++;
                }
                else
                {
                    logD(channelcheck, _func_,"all expired records have been deleted");
                    break;
                }
            }
        }
        else
        {
            logD(channelcheck, _func_,"there is no files at all, clean all cache");

            ChannelFileTimes::iterator itrF = itrD->second.begin();
            while(itrF != itrD->second.end())
            {
                files_changed[itrF->first] = itrD->first;
                itrF++;
            }
            itrD->second.clear();
        }

        writeIdx(files_changed);
    }


    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"ChannelChecker.cleanCache exectime = [", t, "]");

    m_mutex.unlock();

    return CheckResult_Success;
}

ChannelChecker::CheckResult
ChannelChecker::addRecordInCache(const std::string & filename, const std::string & disk, bool bUpdate)
{
    logD(channelcheck, _func_,"addRecordInCache:", filename.c_str());

    TimeChecker tc;tc.Start();

    if(m_fileDiskTimes.find(filename) == m_fileDiskTimes.end() || bUpdate)
    {
        StRef<String> const flv_filename = st_makeString (filename.c_str(), ".flv");
        StRef<String> flv_filenameFull = st_makeString(disk.c_str(), "/", flv_filename);

        Time timeOfRecord = 0;
        FileNameToUnixTimeStamp().Convert(flv_filenameFull, timeOfRecord);
        int const unixtime_timestamp_start = timeOfRecord / 1000000000LL;

        int duration = FastGetDuration(flv_filenameFull->cstr());
        if(duration < 0.1)
        {
            if(bUpdate)
            {
                logD(channelcheck, _func_, "fast duration is failed");
                // trying to get duration by ffmpeg
                FileReader fileReader;
                fileReader.Init(flv_filenameFull);
                duration = (int)fileReader.GetDuration();
            }
            else
            {
                duration = 0;
            }
        }
        int const unixtime_timestamp_end = unixtime_timestamp_start + duration;
        logD(channelcheck, _func_,"flv duration = [", duration, "]");

        ChChTimes chChTimes;
        chChTimes.timeStart = unixtime_timestamp_start;
        chChTimes.timeEnd = unixtime_timestamp_end;

        ChChDiskTimes chchDiskTimes;
        chchDiskTimes.diskName = disk;
        chchDiskTimes.times = chChTimes;

        m_fileDiskTimes[filename] = chchDiskTimes;
        if(m_diskFileTimes.find(disk) == m_diskFileTimes.end())
        {
            ChannelFileTimes chFileTimes;
            m_diskFileTimes[disk] = chFileTimes;
        }
        m_diskFileTimes[disk].insert(std::pair<std::string, ChChTimes>(filename, chChTimes));
    }

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"ChannelChecker.addRecordInCache exectime = [", t, "]");

    return CheckResult_Success;
}

void
ChannelChecker::refreshTimerTick (void * const _self)
{
    ChannelChecker * const self = static_cast <ChannelChecker*> (_self);

    logD (channelcheck, _func_);

    self->cleanCache();
    self->completeCache(false);
}

mt_const void
ChannelChecker::init (Timers * const mt_nonnull timers, const std::string & diskName, StRef<String> & channel_name)
{
    m_channel_name = channel_name;

    logD(channelcheck, _func_,"m_channel_name=", m_channel_name);

    initCache();
    dumpData();

    m_timers = timers;

    m_timer_key = m_timers->addTimer (CbDesc<Timers::TimerCallback> (refreshTimerTick, this, this),
                      TIMER_TICK,
                      true /* periodical */,
                      false /* auto_delete */);
}

void ChannelChecker::dumpData()
{
    logD(channelcheck, _func_,"m_channel_name = [", m_channel_name, "]");

    m_mutex.lock();

    logD(channelcheck, _func_, "File summary:");
    for(ChannelFileDiskTimes::iterator itr = m_fileDiskTimes.begin(); itr != m_fileDiskTimes.end(); ++itr)
    {
        logD(channelcheck, _func_,"file [", itr->second.diskName.c_str(), "/", itr->first.c_str(), "] = {",
             itr->second.times.timeStart, ",", itr->second.times.timeEnd, "}");
    }

    m_mutex.unlock();
}

ChannelChecker::ChannelChecker(): m_timers(this),m_timer_key(NULL)
{

}

ChannelChecker::~ChannelChecker ()
{
    if (m_timer_key) {
        m_timers->deleteTimer (this->m_timer_key);
        m_timer_key = NULL;
    }
}

}
