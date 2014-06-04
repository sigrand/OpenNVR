#include <moment-ffmpeg/inc.h>
#include <moment-ffmpeg/ffmpeg_common.h>
#include <moment-ffmpeg/naming_scheme.h>
#include <moment-ffmpeg/video_part_maker.h>
#include <moment-ffmpeg/ffmpeg_support.h>
#include <moment-ffmpeg/ffmpeg_pipeline_protocol.h>


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static LogGroup libMary_logGroup_vpm ("mod_ffmpeg.video_part_maker", LogLevel::D);

Uint64 VideoPartMaker::g_Id = 0;
std::map<Uint64, VideoPartMaker *> VideoPartMaker::g_VpmMap;
Mutex VideoPartMaker::g_MapMutex;

VideoPartMaker::VideoPartMaker()
{
    nStartTime = 0;
    nCurFileStartTime = 0;
    nCurFileShift = 0;
    nEndTime = 0;
    m_bIsInit = false;
    m_pOutFormatCtx = NULL;
    m_Id = g_Id++;
    m_out = NULL;

    g_MapMutex.lock();
    g_VpmMap[m_Id] = this;
    g_MapMutex.unlock();
}

VideoPartMaker::~VideoPartMaker()
{
    m_out->flush();

    InternalDeinit();

    m_out = NULL;

    g_MapMutex.lock();
    g_VpmMap.erase(m_Id);
    g_MapMutex.unlock();
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

bool VideoPartMaker::InternalInit(AVFormatContext * format_ctx)
{
    logD(vpm, _func_);

    bool bRes = true;

    static const std::string protocolName = CPipelineProtocol::GetProtocolName();
    std::stringstream bufferForName;
    // make file name e.g.: protocolname:/222.flv
    bufferForName << protocolName << ":/" << m_Id << ".flv";
    const std::string & fullName = bufferForName.str();

    assert(m_pOutFormatCtx == NULL);
    if(m_pOutFormatCtx)
    {
        logE_(_func_, "m_pOutFormatCtx != NULL, very bad situation, check it");
        return false;
    }

    int ffRes = avformat_alloc_output_context2(&m_pOutFormatCtx, NULL, "flv", fullName.c_str());
    logD(vpm, _func_, "szFilePath = ", fullName.c_str(), ", m_pOutFormatCtx->filename = ", m_pOutFormatCtx->filename);
    if(ffRes < 0 || !m_pOutFormatCtx)
    {
        logE_(_func_, "avformat_alloc_output_context2 fails = ", ffRes);
        return false;
    }
    logD(vpm, _func_, "format name = ", m_pOutFormatCtx->oformat->name, ", long name = ", m_pOutFormatCtx->oformat->long_name);

    // initialize streams
    for (int i = 0; i < format_ctx->nb_streams; ++i)
    {
        AVStream * pStream = NULL;

        if (!(pStream = avformat_new_stream(m_pOutFormatCtx, NULL)))
        {
            logE_(_func_, "avformat_new_stream failed!");
            ffRes = -1;
            break;
        }

        AVCodecContext * pInpCodec = format_ctx->streams[i]->codec;
        AVCodecContext * pOutCodec = pStream->codec;

        if(avcodec_copy_context(pOutCodec, pInpCodec) == 0)
        {
            if (!m_pOutFormatCtx->oformat->codec_tag ||
                av_codec_get_id (m_pOutFormatCtx->oformat->codec_tag, pInpCodec->codec_tag) == pOutCodec->codec_id ||
                av_codec_get_tag(m_pOutFormatCtx->oformat->codec_tag, pInpCodec->codec_id) <= 0)
            {
                pOutCodec->codec_tag = pInpCodec->codec_tag;
            }
            else
            {
                pOutCodec->codec_tag = 0;
            }

            pStream->sample_aspect_ratio = format_ctx->streams[i]->sample_aspect_ratio;
        }
        else
        {
            logE_(_func_, "avcodec_copy_context failed!");
            ffRes = -1;
            break;
        }
    }

    if(ffRes < 0)
    {
        logE_(_func_, "init streams fails = ", ffRes);
        return false;
    }

    av_dump_format(m_pOutFormatCtx, 0, fullName.c_str(), 1);

    if (!(m_pOutFormatCtx->oformat->flags & AVFMT_NOFILE))
    {
        int rres;
        // we need open file here
        if ((rres = avio_open(&m_pOutFormatCtx->pb, fullName.c_str(), AVIO_FLAG_WRITE)) < 0)
        {
            logE_(_func_, "fail to avio_open ", fullName.c_str());
            ffRes = -1;
        }
    }
    else
    {
        logE_(_func_, "m_pOutFormatCtx has AVFMT_NOFILE flag");
        return false;
    }

    // write header
    if(ffRes == 0)
    {
        ffRes = avformat_write_header(m_pOutFormatCtx, NULL);
    }

    return ffRes == 0;
}

void VideoPartMaker::InternalDeinit()
{
    logD(vpm, _func_, "Deinit");

    if(m_pOutFormatCtx)
    {
        ffmpegStreamData::CloseCodecs(m_pOutFormatCtx);

        avio_close(m_pOutFormatCtx->pb);

        avformat_free_context(m_pOutFormatCtx);

        m_pOutFormatCtx = NULL;
    }

    m_bIsInit = false;

    logD(vpm, _func_, "Deinit succeed");
}

bool VideoPartMaker::Init (ChannelChecker::ChannelFileDiskTimes * channelFileDiskTimes,
                           std::string & channel_name,
                            Time          const start_unixtime_sec,
                            Time          const end_unixtime_sec,
                           HTTPServerResponse * resp)
{
    logD(vpm, _func_);
    this->nStartTime = start_unixtime_sec;
    this->nEndTime = end_unixtime_sec;
    m_pChannelFileDiskTimes = channelFileDiskTimes;

    resp->setContentType("video/x-flv");
    resp->setChunkedTransferEncoding(true);

    m_out = (std::ostream*)&resp->send();

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
            bFileIsFound = true;

            struct timeval tv;
            gettimeofday(&tv, NULL);

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

    m_bIsInit = InternalInit(m_fileReader.GetFormatContext());

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

    bool bRes = true;
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

            //m_nvrData.WritePacket(m_fileReader.GetFormatContext(), packet);
            if(av_write_frame(m_pOutFormatCtx, &packet) < 0)
            {
                logE_(_func_, "av_interleaved_write_frame failed!");
                bRes = false;
                break;
            }

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

    return bRes;
}

bool VideoPartMaker::SendBuffer(Uint64 id, const unsigned char *buf, int size)
{
    bool bRes = true;

    g_MapMutex.lock();
    std::ostream* out = NULL;
    if(g_VpmMap.find(id) != g_VpmMap.end())
    {
        VideoPartMaker * pvpm = g_VpmMap[id];
        out = pvpm->m_out;
    }
    g_MapMutex.unlock();

    if(out == NULL)
    {
        return false;
    }

    (*out).write((const char *)buf, size);

    return bRes;
}

bool VideoPartMaker::GetVpmId(const char * pszFileName, Uint64 & id)
{
    bool bRes = true;

    // we parse the file name like : protocolname:/123.flv
    std::string fileName = pszFileName;
    size_t backslashIdx = fileName.find('/');
    size_t dotIdx = fileName.find('.');

    if( backslashIdx == std::string::npos || dotIdx == std::string::npos || backslashIdx >= dotIdx)
    {
        logE_( _func_, "bad file name, pszFileName = ", pszFileName);
        return false;
    }

    std::string strId = fileName.substr(backslashIdx+1, dotIdx - backslashIdx - 1);

    if(!strToUint64_safe(strId.c_str(), &id))
    {
        logE_(_func_, "strToUint64_safe failed, id = ", strId.c_str());
        return false;
    }

    return bRes;
}

}
