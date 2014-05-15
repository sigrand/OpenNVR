#include <moment-ffmpeg/inc.h>

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
static LogGroup libMary_logGroup_mutex ("mod_ffmpeg.mutex", LogLevel::E);

#ifdef __linux__
uint64_t get_file_space(const char *filepath)
{
  struct stat st;
  uint64_t totalsize = 0LL;

  if(lstat(filepath, &st) == -1)
  {
    logE(channelcheck, _func_, "Couldn't stat: ", filepath);
    return totalsize;
  }

  if(S_ISREG(st.st_mode)) // We only want to count regular files
  {
    totalsize = st.st_size;
  }

  return totalsize;
}

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

bool ChannelChecker::writeIdx(const std::string & dir_name, std::vector<std::string> & files_changed)
{
    TimeChecker tc;tc.Start();

    bool bRes = false;

    std::vector<std::string> paths_idx;
    std::string path_prev;
    for(int i = 0; i < files_changed.size(); i++)
    {
        std::string path = files_changed[i].substr(0,files_changed[i].rfind("/"));
        if(path.compare(path_prev) != 0) // equal
        {
            path_prev = path;
            paths_idx.push_back(path);
        }
    }

    for(int i = 0; i < paths_idx.size(); i++)
    {
        StRef<String> const str_idx_file = st_makeString (dir_name.c_str(), "/", paths_idx[i].c_str(), "/", m_channel_name, ".idx");
        std::string idxFileName = str_idx_file->cstr();
        std::ofstream idxFile;
        idxFile.open(idxFileName);
        logD(channelcheck, _func_,"ChannelChecker.writeIdx idxFileName = ", idxFileName.c_str());
        ChannelFileTimes * channelFileTimes = & m_chDiskFileTimes[dir_name];
        ChannelFileTimes::const_iterator it = channelFileTimes->begin();
        bool bIsDone = false;
        logD(channelcheck, _func_,"ChannelChecker.writeIdx files_existence.size() = ", channelFileTimes->size());
        for(it; it != channelFileTimes->end(); ++it)
        {
            std::string path = it->first.substr(0,it->first.rfind("/"));
            if(path.compare(paths_idx[i]) == 0)
            {
                idxFile << it->first;
                idxFile << "|";
                idxFile << it->second.timeStart;
                idxFile << "|";
                idxFile << it->second.timeEnd;
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

    std::string curPath = m_recpathConfig->GetNextPath();

    while(curPath.length() != 0)
    {
        StRef<String> strRecDir = st_makeString(curPath.c_str());

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

                    ChChDiskTimes chChDiskTimes;
                    chChDiskTimes.times.timeStart = initTime;
                    chChDiskTimes.times.timeEnd = endTime;
                    chChDiskTimes.diskName = std::string(curPath);

                    m_chFileDiskTimes[line] = chChDiskTimes;

                    m_chDiskFileTimes[chChDiskTimes.diskName][line] = chChDiskTimes.times;

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

        curPath = m_recpathConfig->GetNextPath(curPath);
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

    logD(mutex, _func_, "QQQQC 1");
    cleanCache();
    logD(mutex, _func_, "QQQQC 2");
    updateCache(true);

    logD(mutex, _func_, "QQQQC MUTEX _locked");
    m_mutex.lock();

    ChannelTimes chFileTimes;
    ChannelFileDiskTimes::iterator it;
    chFileTimes.reserve(m_chFileDiskTimes.size());
    for(it = m_chFileDiskTimes.begin(); it != m_chFileDiskTimes.end(); ++it)
    {
        ChChTimes chChTimes;
        chChTimes.timeStart = (*it).second.times.timeStart;
        chChTimes.timeEnd = (*it).second.times.timeEnd;
        chFileTimes.push_back(chChTimes);
    }

    if(chFileTimes.size() >= 2)
    {
        logD(mutex, _func_, "QQQQC chFileTimes.size() >= 2");
        ChannelTimes::iterator itr = chFileTimes.begin();
        std::advance (itr,1);
logD(mutex, _func_, "QQQQC BEFORE concatination");
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
logD(mutex, _func_, "QQQQC AFTER concatination");
    }
    else
    {
        logD(mutex, _func_, "QQQQC chFileTimes.size() IS NOT >= 2");
    }

    logD(mutex, _func_, "QQQQC MUTEX unlocked");
    m_mutex.unlock();

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"GetChannelTimes exectime = [", t, "]");

    return chFileTimes;
}

std::pair<std::string, ChChDiskTimes>
ChannelChecker::GetOldestFileDiskTimes ()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    if(m_chFileDiskTimes.size())
    {
        logD(channelcheck, _func_,"dirname: [", m_chFileDiskTimes.begin()->first.c_str(), "]");
        return std::pair<std::string, ChChDiskTimes>(m_chFileDiskTimes.begin()->first, m_chFileDiskTimes.begin()->second);
    }
    else
    {
        return std::pair<std::string, ChChDiskTimes>();
    }
}

ChannelChecker::ChannelFileDiskTimes
ChannelChecker::GetChannelFileDiskTimes ()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    TimeChecker tc;tc.Start();

    cleanCache();
    updateCache(true);

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"getChannelFileDiskTimes exectime = [", t, "]");

    return m_chFileDiskTimes;
}

ChannelChecker::DiskSizes
ChannelChecker::GetDiskSizes ()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    logD(mutex, _func_, "QQQQC MUTEX _locked");
    m_mutex.lock();

    DiskSizes ds;

    DiskFileSizes::iterator itr = m_occupSizes.begin();
    for(itr; itr != m_occupSizes.end(); itr++)
    {
        ds[itr->first] = 0;
        std::map<std::string, Uint64>::iterator itr1 = itr->second.begin();
        for(itr1; itr1 != itr->second.end(); itr1++)
        {
            ds[itr->first] += itr1->second;
        }
    }

    logD(mutex, _func_, "QQQQC MUTEX unlocked");
    m_mutex.unlock();

    return ds;
}

ChannelChecker::CheckResult
ChannelChecker::initCache()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    TimeChecker tc;tc.Start();

    logD(mutex, _func_, "QQQQC MUTEX _locked");
    m_mutex.lock();

    initCacheFromIdx();
    completeCache();

    logD(mutex, _func_, "QQQQC MUTEX unlocked");
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

    if(!m_chFileDiskTimes.empty()) // if cache is not empty, then we update only last record duration
    {
        ChannelFileDiskTimes::reverse_iterator itr = m_chFileDiskTimes.rbegin();
        addRecordInCache(itr->first, itr->second.diskName, true);
    }

    ChannelFileDiskTimes::iterator itr = m_chFileDiskTimes.begin();
    for(itr; itr != m_chFileDiskTimes.end(); itr++)
    {
        StRef<String> st_fullname = st_makeString(itr->second.diskName.c_str(), "/", itr->first.c_str(), ".flv");
        m_occupSizes[itr->second.diskName][itr->first] = get_file_space(st_fullname->cstr());
    }

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"ChannelChecker.initCacheFromIdx exectime = [", t, "]");

    return CheckResult_Success;
}

bool
ChannelChecker::DeleteFromCache(const std::string & dirName, const std::string & fileName)
{
    logD(channelcheck, _func_,"filename: [", dirName.c_str(), "/", fileName.c_str(), "]");

    logD(mutex, _func_, "QQQQC MUTEX _locked");
    m_mutex.lock();

    m_chFileDiskTimes.erase(fileName);
    m_chDiskFileTimes[dirName].erase(fileName);

    std::vector<std::string> files_changed;
    files_changed.push_back(fileName);
    writeIdx(dirName, files_changed);

    m_occupSizes[dirName].erase(fileName);

    logD(mutex, _func_, "QQQQC MUTEX unlocked");
    m_mutex.unlock();

    return true;
}

ChannelChecker::CheckResult
ChannelChecker::updateCache(bool bForceUpdate)
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    CheckResult rez = CheckResult_Success;

    logD(mutex, _func_, "QQQQC 2 MUTEX _locked");
    m_mutex.lock();

    TimeChecker tc;tc.Start();

    ChannelFileDiskTimes::reverse_iterator itr = m_chFileDiskTimes.rbegin();
logD(mutex, _func_, "QQQQC update 1");
    if(itr != m_chFileDiskTimes.rend()) // if last recorded file exists
    {
logD(mutex, _func_, "QQQQC update 2");
        std::string lastfile = itr->first;
        std::string lastdir = itr->second.diskName;

        Time timeOfRecord = 0;
        StRef<String> const flv_filename = st_makeString (lastfile.c_str(), ".flv");
        FileNameToUnixTimeStamp().Convert(flv_filename, timeOfRecord);
        timeOfRecord = timeOfRecord / 1000000000LL;

        bool bNewerFile = false; // false - last recorded file is recording file now, true - there is newer file
        std::string curPath = m_recpathConfig->GetNextPath();
logD(mutex, _func_, "QQQQC update 2.1");
        while(curPath.length() != 0)
        {
            NvrFileIterator file_iter;
            StRef<String> strRecDir = st_makeString(curPath.c_str());
            Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (strRecDir->mem());
            file_iter.init (vfs, m_channel_name->mem(), timeOfRecord);
            StRef<String> path = file_iter.getNext();
            // here path is last recorded file (if it is the same disk as for last recorded file)
            // or just some old last file (if it is not the same disk as for last recorded file)
            // or NULL (if current disk(curPath) doesnt contain files)
            // OR "" (if current disk(curPath) didnt contain files and now has one recording file)
            if(path != NULL)
            {
                StRef<String> pathNext = file_iter.getNext();
                // pathNext is new recording file
                // or NULL (if nvr writes on other disk or last recorded file is recording now)
                if(pathNext != NULL && !pathNext->isNullString())
                {
                    // update duration for last recorded file
                    addRecordInCache(lastfile, lastdir, true);

                    // update occup size of last recorded file
                    StRef<String> st_fullname = st_makeString(lastdir.c_str(), "/", lastfile.c_str(), ".flv");
                    Int64 occupSizeFile = get_file_space(st_fullname->cstr());
                    m_occupSizes[lastdir][lastfile] = occupSizeFile;

                    // add new recording file in cache
                    std::string strpathNext = std::string(pathNext->cstr());
                    addRecordInCache(strpathNext, curPath, false);

                    std::vector<std::string> files_changed;
                    files_changed.push_back(lastfile);
                    writeIdx(lastdir, files_changed);
                    if(lastdir.compare(curPath) != 0)
                    {
                        std::vector<std::string> files_changed_new;
                        files_changed_new.push_back(strpathNext);
                        writeIdx(curPath, files_changed_new);
                    }

                    bNewerFile = true;
                    break; // recording file is only one for one source
                }
            }
            curPath = m_recpathConfig->GetNextPath(curPath);
        }
logD(mutex, _func_, "QQQQC update 2.2");
        if(!bNewerFile)
        {
            // update duration for last recorded file
            addRecordInCache(lastfile, lastdir, bForceUpdate);
            std::vector<std::string> files_changed;
            if(bForceUpdate)
                files_changed.push_back(lastfile);
            writeIdx(lastdir, files_changed);
        }
logD(mutex, _func_, "QQQQC update 2.3");
    }
    else // no one records exist
    {
logD(mutex, _func_, "QQQQC update 3");
        std::string curPath = m_recpathConfig->GetNextPath();
        while(curPath.length() != 0)
        {
            NvrFileIterator file_iter;
            StRef<String> strRecDir = st_makeString(curPath.c_str());
            Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (strRecDir->mem());
            file_iter.init (vfs, m_channel_name->mem(), 0);
            StRef<String> path = file_iter.getNext();
            if(path != NULL && !path->isNullString()) // if it is not null, then it is recording file
            {
                std::string strpath = std::string(path->cstr());
                addRecordInCache(strpath, curPath, false);
                std::vector<std::string> files_changed;
                files_changed.push_back(path->cstr());
                writeIdx(curPath, files_changed);
                break; // recording file is only one for one source
            }
            curPath = m_recpathConfig->GetNextPath(curPath);
        }
logD(mutex, _func_, "QQQQC update 3.1");
    }
logD(mutex, _func_, "QQQQC update 4");
    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"ChannelChecker.updateCache exectime = [", t, "]");

    logD(mutex, _func_, "QQQQC 2 MUTEX unlocked");
    m_mutex.unlock();

    return rez;
}

ChannelChecker::CheckResult
ChannelChecker::completeCache()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    TimeChecker tc;tc.Start();

    CheckResult rez = CheckResult_Success;
    std::string curPath = m_recpathConfig->GetNextPath();
    while(curPath.length() != 0)
    {
        Time timeOfRecord = 0;
        ChannelFileTimes * channelFileTimes = & m_chDiskFileTimes[curPath];
        if(!channelFileTimes->empty())
        {
            std::string lastfile = channelFileTimes->rbegin()->first;
            StRef<String> const flv_filename = st_makeString (lastfile.c_str(), ".flv");
            FileNameToUnixTimeStamp().Convert(flv_filename, timeOfRecord);
            timeOfRecord = timeOfRecord / 1000000000LL;
        }

        NvrFileIterator file_iter;
        StRef<String> strRecDir = st_makeString(curPath.c_str());
        Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (strRecDir->mem());
        file_iter.init (vfs, m_channel_name->mem(), timeOfRecord);

        std::vector<std::string> files_changed;

        StRef<String> path = file_iter.getNext();
        while(path != NULL && !path->isNullString())
        {
            std::string strpath = std::string(path->cstr());
            addRecordInCache(strpath, curPath, true);
            ChannelFileDiskTimes::iterator itr = m_chFileDiskTimes.begin();

            StRef<String> st_fullname = st_makeString(curPath.c_str(), "/", strpath.c_str(), ".flv");
            m_occupSizes[curPath][strpath] = get_file_space(st_fullname->cstr());

            files_changed.push_back(strpath);
            path = file_iter.getNext();
        }

        writeIdx(curPath, files_changed);

        curPath = m_recpathConfig->GetNextPath(curPath);
    }

    Time t;tc.Stop(&t);
    logD (channelcheck, _func_,"ChannelChecker.completeCache exectime = [", t, "]");

    return rez;
}

ChannelChecker::CheckResult
ChannelChecker::cleanCache()
{
    logD(channelcheck, _func_,"channel_name: [", m_channel_name, "]");

    logD(mutex, _func_, "QQQQC 1 MUTEX _locked");
    m_mutex.lock();

    if(m_chFileDiskTimes.empty())
    {
        logD(mutex, _func_, "QQQQC 1 MUTEX unlocked");
        m_mutex.unlock();
        return CheckResult_Success;
    }

    TimeChecker tc;tc.Start();

    std::string curPath = m_recpathConfig->GetNextPath();
logD(mutex, _func_, "QQQQC clean 1");
    while(curPath.length() != 0)
    {
        StRef<String> strRecDir = st_makeString(curPath.c_str());
        ConstMemory record_dir = strRecDir->mem();
        Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (record_dir);

        NvrFileIterator file_iter;
        file_iter.init (vfs, m_channel_name->mem(), 0); // get the oldest file

        std::vector<std::string> files_changed;

        StRef<String> path = file_iter.getNext();
logD(mutex, _func_, "QQQQC clean 2");
        if(path != NULL)
        {
            logD(mutex, _func_, "QQQQC clean 3");
            Time timeOfFirstFile = 0;
            {
                StRef<String> const flv_filename = st_makeString (path, ".flv"); // replace by read from idx ??????
                FileNameToUnixTimeStamp().Convert(flv_filename, timeOfFirstFile);
                timeOfFirstFile = timeOfFirstFile / 1000000000LL;
            }

            ChannelFileDiskTimes::iterator it = m_chFileDiskTimes.begin();
logD(mutex, _func_, "QQQQC clean 3.1");
            while(it != m_chFileDiskTimes.end())
            {
                if(it->second.diskName.compare(curPath) == 0)
                {
                    logD(channelcheck, _func_,"file [", it->first.c_str(), ",{", it->second.times.timeStart, ",", it->second.times.timeEnd, "}]");
                    Time timeOfRecord = 0;
                    StRef<String> const flv_filename = st_makeString ((*it).first.c_str(), ".flv");
                    FileNameToUnixTimeStamp().Convert(flv_filename, timeOfRecord);
                    timeOfRecord = timeOfRecord / 1000000000LL;

                    logD(channelcheck, _func_,"timeOfFirstFile: [", timeOfFirstFile, "], timeOfRecord: [", timeOfRecord, "]");
                    if(timeOfRecord < timeOfFirstFile)
                    {
                        logD(channelcheck, _func_,"clean it!");
                        files_changed.push_back(it->first);

                        m_chDiskFileTimes[it->second.diskName].erase(it->first);
                        m_chFileDiskTimes.erase(it);

                        m_occupSizes[it->second.diskName].erase(it->first);

                        it++;
                    }
                    else
                    {
                        logD(channelcheck, _func_,"all expired records have been deleted");
                        break;
                    }
                }
                else
                {
                    it++;
                }
            }
logD(mutex, _func_, "QQQQC clean 3.2");
        }
        else
        {
logD(mutex, _func_, "QQQQC clean 4");
            logD(channelcheck, _func_,"there is no files at all, clean all cache");

            ChannelFileDiskTimes::iterator itr = m_chFileDiskTimes.begin();
            while(itr != m_chFileDiskTimes.end())
            {
                if(itr->second.diskName.compare(curPath) == 0)
                {
                    files_changed.push_back(itr->first);

                    m_chDiskFileTimes[itr->second.diskName].erase(itr->first);
                    m_chFileDiskTimes.erase(itr);
                    m_occupSizes[itr->second.diskName].erase(itr->first);
                }
                itr++;
            }
logD(mutex, _func_, "QQQQC clean 4.1");
        }
logD(mutex, _func_, "QQQQC clean 5");
        writeIdx(curPath, files_changed);

        curPath = m_recpathConfig->GetNextPath(curPath);
logD(mutex, _func_, "QQQQC clean 6");
    }

    Time t;tc.Stop(&t);
    logD(channelcheck, _func_,"ChannelChecker.cleanCache exectime = [", t, "]");

    logD(mutex, _func_, "QQQQC 1 MUTEX unlocked");
    m_mutex.unlock();

    return CheckResult_Success;
}

ChannelChecker::CheckResult
ChannelChecker::addRecordInCache(const std::string & path, const std::string & strRecDir, bool bUpdate)
{
    logD(channelcheck, _func_,"addRecordInCache:", path.c_str());

    TimeChecker tc;tc.Start();

    if(m_chFileDiskTimes.find(path) == m_chFileDiskTimes.end() || bUpdate)
    {
        StRef<String> const flv_filename = st_makeString (path.c_str(), ".flv");
        StRef<String> flv_filenameFull = st_makeString(strRecDir.c_str(), "/", flv_filename);

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

        ChChDiskTimes chChDiskTimes;
        chChDiskTimes.times.timeStart = unixtime_timestamp_start;
        chChDiskTimes.times.timeEnd = unixtime_timestamp_end;
        chChDiskTimes.diskName = strRecDir;
        m_chFileDiskTimes[path] = chChDiskTimes;

        m_chDiskFileTimes[chChDiskTimes.diskName][path] = chChDiskTimes.times;
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
    self->updateCache(false);
}

mt_const void
ChannelChecker::init (Timers * const mt_nonnull timers, RecpathConfig * recpathConfig, StRef<String> & channel_name)
{
    m_recpathConfig = recpathConfig;
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

    logD(mutex, _func_, "QQQQC MUTEX _locked");
    m_mutex.lock();

    logD(channelcheck, _func_, "File summary:");
    for(ChannelFileDiskTimes::iterator itr = m_chFileDiskTimes.begin(); itr != m_chFileDiskTimes.end(); ++itr)
    {
        logD(channelcheck, _func_,"file [", itr->first.c_str(), " on ", itr->second.diskName.c_str(), "] = {",
             itr->second.times.timeStart, ",", itr->second.times.timeEnd, "}");
    }

    logD(mutex, _func_, "QQQQC MUTEX unlocked");
    m_mutex.unlock();
}

ChannelChecker::ChannelChecker(): m_timers(this),m_timer_key(NULL), m_recpathConfig(NULL)
{

}

ChannelChecker::~ChannelChecker ()
{
    if (m_timer_key) {
        m_timers->deleteTimer (this->m_timer_key);
        m_timer_key = NULL;
    }
    m_recpathConfig = NULL;
}

}
