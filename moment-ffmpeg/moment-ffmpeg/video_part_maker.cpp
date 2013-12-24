#include <moment-ffmpeg/inc.h>
#include <moment-ffmpeg/naming_scheme.h>
#include <moment-ffmpeg/video_part_maker.h>
#include <moment-ffmpeg/memory_dispatcher.h>


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {


VideoPartMaker::VideoPartMaker()
{
    nStartTime = 0;
    nCurFileStartTime = 0;
    nCurFileShift = 0;
    nEndTime = 0;
    bIsInit = false;
}

VideoPartMaker::~VideoPartMaker()
{
    m_nvrData.Deinit();
}

bool
VideoPartMaker::tryOpenNextFile ()
{
    StRef<String> const filename = m_fileIter.getNext ();
    if (!filename) {
        logD_(_func_, "filename is null");
        return false;
    }

    StRef<String> cur_filename = st_makeString(record_dir, "/", filename, ".flv");

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
        return false;
    }

    return true;
}

bool VideoPartMaker::Init (Vfs         * const mt_nonnull vfs,
                            ConstMemory   const stream_name,
                            ConstMemory   const record_dir,
                            ConstMemory   const res_file_path,
                            Time          const start_unixtime_sec,
                            Time          const end_unixtime_sec)
{
    logD_(_func_);
    this->nStartTime = start_unixtime_sec;
    this->nEndTime = end_unixtime_sec;
    this->record_dir = st_makeString(record_dir);

    if(nStartTime >= nEndTime)
    {
        logE_(_func_, "fail to init: nStartTime >= nEndTime");
        return false;
    }

    m_fileIter.init (vfs, stream_name, nStartTime);

    if (!m_fileReader.IsInit())
    {
        if (!tryOpenNextFile ())
        {
            logE_(_func_, "fail to init");
            return false;
        }
    }

    m_filepath = st_makeString(res_file_path);
    StRef<String> channel_name = st_makeString (stream_name);

    if(!MemoryDispatcher::Instance().GetPermission(m_filepath->cstr(), end_unixtime_sec - start_unixtime_sec))
    {
        logE_(_func_, "no space for file");
        return false;
    }

    bIsInit = m_nvrData.Init(m_fileReader.GetFormatContext(), channel_name->cstr(), m_filepath->cstr(), NULL, end_unixtime_sec - start_unixtime_sec);

    return bIsInit;
}

bool VideoPartMaker::IsInit()
{
    return bIsInit;
}

bool
VideoPartMaker::Process ()
{
    if(!bIsInit)
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

    int ptsExtra = 0;
    int dtsExtra = 0;
    int ptsExtraPrv = 0;
    int dtsExtraPrv = 0;
    unsigned long fileSize = 0;
    // read packets from file
    while (1)
    {
        FileReader::Frame frame;

        if(m_fileReader.ReadFrame(frame))
        {
            AVPacket packet = frame.GetPacket();
            Time nCurAbsPos = nCurFileStartTime + nCurFileShift + (packet.pts / (double)tb_den);
            if(nCurAbsPos > nEndTime)
            {
                logD_(_func_, "done");
                m_fileReader.FreeFrame(frame);
                break;
            }
            packet.pts += ptsExtra;
            packet.dts += dtsExtra;

//            logD_(_func_, "====== before write");
            m_nvrData.WritePacket(m_fileReader.GetFormatContext(), packet);
//            logD_(_func_, "====== after write");

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
                logD_(_func_, "all files are over");
                break;
            }

            ptsExtra = ptsExtraPrv;
            dtsExtra = dtsExtraPrv;

            logD_(_func_, "next file");
        }
    }

    MemoryDispatcher::Instance().Notify(m_filepath->cstr(), true, 0);

    return true;
}

}
