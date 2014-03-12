#include <moment-ffmpeg/inc.h>
#include <moment-ffmpeg/ffmpeg_common.h>
#include <moment-ffmpeg/naming_scheme.h>
#include <moment-ffmpeg/video_part_maker.h>
#include <moment-ffmpeg/memory_dispatcher.h>


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static LogGroup libMary_logGroup_vpm ("mod_ffmpeg.video_part_maker", LogLevel::E);

VideoPartMaker::VideoPartMaker()
{
    nStartTime = 0;
    nCurFileStartTime = 0;
    nCurFileShift = 0;
    nEndTime = 0;
    m_bIsInit = false;
}

VideoPartMaker::~VideoPartMaker()
{
    m_nvrData.Deinit();
}

bool
VideoPartMaker::tryOpenNextFile ()
{
    if (m_itr == m_pChannelFileDiskTimes->end())
    {
        logD(vpm, _func_, "end of files");
        return false;
    }

    StRef<String> cur_filename = st_makeString(m_itr->second.diskName.c_str(), "/", m_itr->first.c_str(), ".flv");

    m_itr++;

    if(!m_fileReader.Init(cur_filename))
    {
        logE_(_func_,"m_fileReader.Init failed");
        return false;
    }

    Time unix_time_stamp = 0;

    if(FileNameToUnixTimeStamp().Convert(cur_filename, unix_time_stamp) == Result::Success)
    {
        unix_time_stamp /= 1000000000LL;
        nCurFileStartTime = unix_time_stamp;

        if(unix_time_stamp < nStartTime)
        {
            double dNumSeconds = (nStartTime - unix_time_stamp);
            nCurFileShift = dNumSeconds;

            if(!m_fileReader.Seek(dNumSeconds))
            {
                return false;
            }
        }
        else
        {
            nCurFileShift = 0;
        }
    }
    else
    {
        logE_(_func_, "fail to convert to time ", cur_filename);
        return false;
    }

    logD(vpm, _func_, "file is found ", cur_filename);
    return true;
}

bool VideoPartMaker::Init (ChannelChecker::ChannelFileDiskTimes * channelFileDiskTimes,
                           std::string & channel_name,
                            Time          const start_unixtime_sec,
                            Time          const end_unixtime_sec,
                            std::string & filePathOut)
{
    logD(vpm, _func_);
    this->nStartTime = start_unixtime_sec;
    this->nEndTime = end_unixtime_sec;
    m_pChannelFileDiskTimes = channelFileDiskTimes;

    if(nStartTime >= nEndTime)
    {
        logE_(_func_, "fail to init: nStartTime >= nEndTime");
        return false;
    }

    bool bFileIsFound = false;
    for(m_itr = m_pChannelFileDiskTimes->begin(); m_itr != m_pChannelFileDiskTimes->end(); m_itr++)
    {
        if(m_itr->second.times.timeStart <= nStartTime && m_itr->second.times.timeEnd > nStartTime)
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);

            StRef<String> strRecDir = st_makeString(m_itr->second.diskName.c_str(), "/", channel_name.c_str());
            Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (strRecDir->mem());
            vfs->createDirectory(DOWNLOAD_DIR);
            m_filepath = st_makeString(strRecDir, "/", DOWNLOAD_DIR, "/", tv.tv_sec, ".mp4");
            filePathOut = m_filepath->cstr();

            bFileIsFound = true;

            break; // file is found
        }
    }
    if(!bFileIsFound)
    {
        logD(vpm, _func_, "there is no files with such timestamps in storage");
        return false;
    }

    if (!m_fileReader.IsInit())
    {
        if (!tryOpenNextFile ())
        {
            logE_(_func_, "fail to init");
            return false;
        }
    }

    if(!MemoryDispatcher::Instance().GetPermission(m_filepath->cstr(), end_unixtime_sec - start_unixtime_sec))
    {
        logE_(_func_, "no space for file");
        return false;
    }

    int res = m_nvrData.Init(m_fileReader.GetFormatContext(), NULL, m_filepath->cstr(), NULL, end_unixtime_sec - start_unixtime_sec);
    m_bIsInit = (res == 0);

    return m_bIsInit;
}

bool VideoPartMaker::IsInit()
{
    return m_bIsInit;
}

bool
VideoPartMaker::Process ()
{
    if(!m_bIsInit)
    {
        logE_(_func_, "is not inited");
        return false;
    }

    AVFormatContext * fmtctx = m_fileReader.GetFormatContext();
    int video_stream_idx = -1;
    for(int i = 0; i < fmtctx->nb_streams; i++)
    {
        if(fmtctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
        {
            video_stream_idx = i;
            break;
        }
    }
    int tb_den = fmtctx->streams[video_stream_idx]->time_base.den;

    int ptsExtra = 0;   // to normalize packets in [0;file_duration_time]
    int dtsExtra = 0;
    int ptsExtraPrv = 0;
    int dtsExtraPrv = 0;
    bool isFirstPacket = true;
    int ptsShift = 0;
    int dtsShift = 0;
    unsigned long fileSize = 0;
    // read packets from file
    while (1)
    {
        FileReader::Frame frame;

        if(m_fileReader.ReadFrame(frame))
        {
            AVPacket packet = frame.GetPacket();
            // if first packet has some non-zero pts value, then we need substract it from packet pts to make pts zero
            if(isFirstPacket)
            {
                if(packet.pts != 0)
                {
                    ptsShift = packet.pts;
                    dtsShift = packet.dts;
                }
                isFirstPacket = false;
            }
            packet.pts -= ptsShift;
            packet.dts -= dtsShift;
            Time nCurAbsPos = nCurFileStartTime + nCurFileShift + (packet.pts / (double)tb_den);
            if(nCurAbsPos > nEndTime)
            {
                logD(vpm, _func_, "done");
                m_fileReader.FreeFrame(frame);
                break;
            }
            packet.pts += ptsExtra;
            packet.dts += dtsExtra;

            m_nvrData.WritePacket(m_fileReader.GetFormatContext(), packet);

            fileSize += packet.size;

            MemoryDispatcher::Instance().Notify(m_filepath->cstr(), false, fileSize);

            ptsExtraPrv = packet.pts;
            dtsExtraPrv = packet.dts;

            m_fileReader.FreeFrame(frame);
        }
        else
        {
            if (!tryOpenNextFile ())
            {
                logD(vpm, _func_, "all files are over");
                break;
            }

            ptsExtra = ptsExtraPrv;
            dtsExtra = dtsExtraPrv;

            if(ptsShift != 0)
            {
                ptsShift = 0;
                dtsShift = 0;
            }
            isFirstPacket = true;

            logD(vpm, _func_, "next file");
        }
    }

    MemoryDispatcher::Instance().Notify(m_filepath->cstr(), true, 0);

    return true;
}

}
