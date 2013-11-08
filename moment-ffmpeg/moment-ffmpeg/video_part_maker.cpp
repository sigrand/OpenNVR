#include <moment-ffmpeg/inc.h>
#include <moment-ffmpeg/naming_scheme.h>
#include <moment-ffmpeg/video_part_maker.h>


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

    StRef<String> st = st_makeString(res_file_path);
    bIsInit = m_nvrData.Init(m_fileReader.GetFormatContext(), st->cstr(), NULL, 0, 0);

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

    int c = 0;
    int ptsExtra = 0;
    int dtsExtra = 0;
    int ptsExtraPrv = 0;
    int dtsExtraPrv = 0;
    // read packets from file
    while (1)
    {
        FileReader::Frame frame;

        if(m_fileReader.ReadFrame(frame))
        {
            logD_(_func_, "====== frame.src_packet.pts before: ", frame.src_packet.pts);
            Time nCurAbsPos = nCurFileStartTime + nCurFileShift + (frame.src_packet.pts / (double)tb_den);
            logD_(_func_, "====== nCurFileStartTime: ", nCurFileStartTime);
            logD_(_func_, "====== nCurFileShift: ", nCurFileShift);
            logD_(_func_, "====== nCurAbsPos: ", nCurAbsPos);
            if(nCurAbsPos > nEndTime)
            {
                logD_(_func_, "====== ENOUGH");
                m_fileReader.FreeFrame(frame);
                break;
            }
            logD_(_func_, "====== ptsExtra: ", ptsExtra);
            frame.src_packet.pts += ptsExtra;
            frame.src_packet.dts += dtsExtra;
            logD_(_func_, "====== frame.src_packet.pts after: ", frame.src_packet.pts);
            logD_(_func_, "====== nStartTime: ", (double)nStartTime);
            logD_(_func_, "====== nEndTime: ", (double)nEndTime);

//            logD_(_func_, "====== before write");
            m_nvrData.WritePacket(m_fileReader.GetFormatContext(), frame.src_packet);
//            logD_(_func_, "====== after write");

            ptsExtraPrv = frame.src_packet.pts;
            dtsExtraPrv = frame.src_packet.dts;

            m_fileReader.FreeFrame(frame);
        }
        else
        {
            if (!tryOpenNextFile ())
            {
                logD_(_func_, "====== ALL FILES ARE OVER");
                break;
            }

            logD_(_func_, "====== ptsExtraPrv: ", ptsExtraPrv);
            logD_(_func_, "====== dtsExtraPrv: ", dtsExtraPrv);
            ptsExtra = ptsExtraPrv;
            dtsExtra = dtsExtraPrv;

            logD_(_func_, "====== NEXT FILE");
        }
        c++;
        logD_(_func_, "====== counter = ", c);
    }

    return true;
}

}
