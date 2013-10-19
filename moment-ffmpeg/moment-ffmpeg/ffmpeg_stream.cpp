/*  Moment-Gst - GStreamer support module for Moment Video Server
    Copyright (C) 2011-2013 Dmitry Shatrov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include <moment-ffmpeg/ffmpeg_stream.h>
#include "naming_scheme.h"
#include <libmary/vfs.h>
#include <iostream>
#include <fstream>
#include <string>


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static LogGroup libMary_logGroup_chains   ("mod_ffmpeg.chains",   LogLevel::D);
static LogGroup libMary_logGroup_pipeline ("mod_ffmpeg.pipeline", LogLevel::D);
static LogGroup libMary_logGroup_stream   ("mod_ffmpeg.stream",   LogLevel::D);
static LogGroup libMary_logGroup_bus      ("mod_ffmpeg.bus",      LogLevel::D);
static LogGroup libMary_logGroup_frames   ("mod_ffmpeg.frames",   LogLevel::E); // E is the default
static LogGroup libMary_logGroup_novideo  ("mod_ffmpeg.novideo",  LogLevel::D);
static LogGroup libMary_logGroup_plug     ("mod_ffmpeg.autoplug", LogLevel::D);



#define AV_RB32(x)									\
        (((Uint32)((const Byte*)(x))[0] << 24) |	\
        (((const Byte*)(x))[1] << 16) |				\
        (((const Byte*)(x))[2] <<  8) |				\
        ( (const Byte*)(x))[3])

#define AV_RB24(x)									\
        ((((const Byte*)(x))[0] << 16) |			\
        ( ((const Byte*)(x))[1] <<  8) |			\
        (  (const Byte*)(x))[2])


extern "C"
{
    extern AVOutputFormat ff_segment2_muxer;
    extern AVOutputFormat ff_stream_segment2_muxer;

    int MakeFullPath(const char * channel_name, int const file_duration_sec, char * fullPath, int iBufSize)
    {
        int i, indExtension = -1;
        int filenameSize = strlen(channel_name);
        for(i = filenameSize - 1; i >= 0; --i)
        {
            if(channel_name[i] == '.')
            {
                indExtension = i;
                break;
            }
        }

        if(indExtension == -1)
        {
            logE_ (_func, "<.> is not found, failed");
            return -1;
        }

        ConstMemory fmt(channel_name + indExtension, filenameSize - indExtension);
        ConstMemory rest(channel_name, indExtension);

        indExtension = -1;

        for(i = rest.len(); i >= 0; --i)
        {
            if(rest.mem()[i] == '/')
            {
                indExtension = i;
                break;
            }
        }

        if(indExtension == -1)
        {
            logE_ (_func, "</> is not found, failed");
            return -1;
        }

        ConstMemory record_dir(rest.mem(), indExtension + 1);
        ConstMemory channelName(rest.mem() + indExtension + 1, rest.len() - indExtension - 1);

        struct timeval  tv;
        gettimeofday(&tv, NULL);
        Time next_unixtime_sec;

        StRef<String> const filename = DefaultNamingScheme(file_duration_sec).getPath(channelName, tv, &next_unixtime_sec);
        if (filename == NULL || !filename->len())
        {
            logE_ (_func, "naming_scheme->getPath() failed");
            return -1;
        }

        Ref<Vfs> vfs = Vfs::createDefaultLocalVfs (record_dir);

        if (!vfs->createSubdirsForFilename (filename->mem()))
        {
            logE_ (_func, "vfs->createSubdirsForFilename() failed, filename: ",
                   channelName, ": ", exc->toString());
            return -1;
        }

        StRef<String> sFullPath = st_makeString(record_dir, filename->mem(), fmt);

        if(sFullPath->len() >= iBufSize)
        {
            logE_ (_func, "sFullPath->len(", sFullPath->len(), ") >= iBufSize(", iBufSize,  ")");
            return -1;
        }

        strcpy(fullPath, sFullPath->cstr());

        logD_ (_func, "new fullfilename: ", fullPath, ", "
               "next_unixtime_sec: ", next_unixtime_sec, ", cur unixtime: ", tv.tv_sec);

        return 0;
    }


    void CustomAVLogCallback(void * /*ptr*/, int /*level*/, const char * outFmt, va_list vl)
    {
        if(outFmt)
        {
            // TODO: need to use 'level'
            //char chLogBuffer[2048] = {};
            //vsnprintf(chLogBuffer, sizeof(chLogBuffer), outFmt, vl);
            //logD_(_func_, "ffmpeg LOG: ", chLogBuffer);
        }
    }
}   // extern "C" ]

StateMutex FFmpegStream::ffmpegStreamData::m_mutexFFmpeg;

static void RegisterFFMpeg(void)
{
    static Uint32 uiInitialized = 0;

    if(uiInitialized != 0)
        return;

    uiInitialized = 1;

    av_register_output_format(&ff_segment2_muxer);
    av_register_output_format(&ff_stream_segment2_muxer);
    av_log_set_callback(CustomAVLogCallback);

    // global ffmpeg initialization
    av_register_all();
    avformat_network_init();
}


FFmpegStream::ffmpegStreamData::ffmpegStreamData(void)
{
    // Register all formats and codecs
    RegisterFFMpeg();

    format_ctx = NULL;

    m_bRecordingState = false;
    m_bGotFirstFrame = false;

    video_stream_idx = -1;

    m_nPts = Int64_Min;
    m_nDts = Int64_Min;
}

FFmpegStream::ffmpegStreamData::~ffmpegStreamData()
{
    Deinit();
}

void FFmpegStream::ffmpegStreamData::CloseCodecs(AVFormatContext * pAVFrmtCntxt)
{
    if(!pAVFrmtCntxt)
        return;

    for(unsigned int uiCnt = 0; uiCnt < pAVFrmtCntxt->nb_streams; ++uiCnt)
    {
        // close each AVCodec
        if(pAVFrmtCntxt->streams[uiCnt])
        {
            AVStream * pAVStream = pAVFrmtCntxt->streams[uiCnt];

            if(pAVStream->codec)
                avcodec_close(pAVStream->codec);
        }
    }
}

void FFmpegStream::ffmpegStreamData::Deinit()
{
    // close writer before any actions with reader
    m_nvrData.Deinit();

    if(format_ctx)
    {
        CloseCodecs(format_ctx);

        avformat_close_input(&format_ctx);

        format_ctx = NULL;
    }

    m_bRecordingState = false;
    m_bGotFirstFrame = false;

    video_stream_idx = -1;

    m_nPts = Int64_Min;
    m_nDts = Int64_Min;
}

Result FFmpegStream::ffmpegStreamData::Init(const char * uri, const char * channel_name, const Ref<MConfig::Config> & config, Timers * timers)
{
    if(!uri || !uri[0])
        return Result::Failure;

    Deinit();

    // Open video file
    logD_(_func_, "uri: ", uri);
    Result res = Result::Success;

    if(avformat_open_input(&format_ctx, uri, NULL, NULL) == 0)
    {
        logD_(_func_, "avformat_open_input, success");
        // Retrieve stream information

        m_mutexFFmpeg.lock();
        if(avformat_find_stream_info(format_ctx, NULL) >= 0)
        {
            logD_(_func_,"avformat_find_stream_info, success");

            // Dump information about file onto standard error
            av_dump_format(format_ctx, 0, uri, 0);
        }
        else
        {
            logE_(_func_, "Couldn't find stream information");
            res = Result::Failure;
        }
        m_mutexFFmpeg.unlock();
    }
    else
    {
        logE_(_func_, "Couldn't open uri");
        res = Result::Failure;
    }

    if(res == Result::Success)
    {
        // Find the first video stream
        for(int i = 0; i < format_ctx->nb_streams; i++)
        {
            if(format_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
            {
                video_stream_idx = i;
                break;
            }
        }

        if(video_stream_idx == -1)
        {
            logE_(_func_,"didn't find a video stream");
            res = Result::Failure;
        }
    }

    // reading configs

    ConstMemory record_dir;
    ConstMemory confd_dir;
    Uint64 max_age_minutes = 120;
    Uint64 file_duration_sec = 3600;

    // get cycle time
    {
        ConstMemory const opt_name = "mod_nvr/max_age";
        MConfig::GetResult const res =
                config->getUint64_default (opt_name, &max_age_minutes, max_age_minutes);
        if (!res)
            logE_ (_func, "Invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        else
            logI_ (_func, opt_name, ": ", max_age_minutes);
    }

    // get path for recording
    {
        ConstMemory const opt_name = "mod_nvr/record_dir"; // TODO: change mod_nvr to mod_ffmpeg?
        bool record_dir_is_set = false;
        record_dir = config->getString (opt_name, &record_dir_is_set);
        if (!record_dir_is_set)
        {
            logE_ (_func, opt_name, " config option is not set, disabling writing");
            // we can not write without output path
            return Result::Failure;
        }
        logI_ (_func, opt_name, ": [", record_dir, "]");
    }

    // get duration for each recorded file
    {
        ConstMemory const opt_name = "mod_nvr/file_duration";
        MConfig::GetResult const res =
                config->getUint64_default (opt_name, &file_duration_sec, file_duration_sec);
        if (!res)
            logE_ (_func, "Invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        else
            logI_ (_func, opt_name, ": ", file_duration_sec);
    }

    // get path for recording
    {
        ConstMemory const opt_name = "moment/confd_dir"; // TODO: change mod_nvr to mod_ffmpeg?
        bool confd_dir_is_set = false;
        confd_dir = config->getString (opt_name, &confd_dir_is_set);
        if (!confd_dir_is_set)
        {
            logE_ (_func, opt_name, " config option is not set, disabling writing");
            // we can not write without output path
            return Result::Failure;
        }
        logI_ (_func, opt_name, ": [", confd_dir, "]");
    }

    if(res == Result::Success)
    {
        StRef<String> recdir = st_makeString(record_dir);
        Result nvrResult = m_nvrData.Init(format_ctx, channel_name, recdir->cstr(), file_duration_sec, max_age_minutes);
        logD_(_func_, " m_nvrData.Init = ", (nvrResult == Result::Success) ? "Success" : "Failed");

        Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs (record_dir);
        m_nvr_cleaner = grab (new (std::nothrow) NvrCleaner);
        m_nvr_cleaner->init (timers, vfs, ConstMemory(channel_name, strlen(channel_name)), max_age_minutes * 60, 5);

        bool bDisableRecord = false;
        StRef<String> st_confd_dir = st_makeString(confd_dir);
        StRef<String> channelFullPath = st_makeString(st_confd_dir, "/", channel_name);
        std::string line;
        std::ifstream channelPath (channelFullPath->cstr());
        logD_(_func_, "channelFullPath = ", channelFullPath);
        if (channelPath.is_open())
        {
            logD_(_func_, "channelFullPath opened");
            while ( std::getline (channelPath, line) )
            {
                logD_(_func_, "got line: [", line.c_str(), "]");
                if(line.compare("disable_record") == 0)
                {
                    logD_(_func_, "FOUND disable_record");
                    bDisableRecord = true;
                }
            }
            channelPath.close();
        }

        m_bRecordingState = ! bDisableRecord;
    }
    else
    {
        Deinit();
    }

    return res;
}

bool FFmpegStream::ffmpegStreamData::PushVideoPacket(FFmpegStream * pParent)
{
    if(!format_ctx || video_stream_idx == -1)
        return false;

    AVPacket packet = {};
    bool video_found = false;

    while(av_read_frame(format_ctx, &packet) >= 0)
    {
        if(!m_bGotFirstFrame)
        {
            if (packet.flags & AV_PKT_FLAG_KEY)
            {
                m_bGotFirstFrame = true;
            }
            else
            {
                av_free_packet(&packet);
                memset(&packet, 0, sizeof(packet));
                continue;
            }
        }

        if(m_nPts == Int64_Min || m_nPts > packet.pts)
        {
            if(packet.pts != AV_NOPTS_VALUE)
                m_nPts = packet.pts;
        }

        if(m_nDts == Int64_Min || m_nDts > packet.dts)
        {
            if(packet.dts != AV_NOPTS_VALUE)
                m_nDts = packet.dts;
        }

        if(m_nPts > m_nDts)
            m_nDts = m_nPts;        // to prevent situation when result packet.pts is less than packet.dts

        if(packet.pts != AV_NOPTS_VALUE)
        {
            packet.pts -= m_nPts;
        }

        if(packet.dts != AV_NOPTS_VALUE)
        {
            packet.dts -= m_nDts;
        }

        logD_(_func_, "PACKET,indx=", packet.stream_index,
                            ",pts=", packet.pts,
                            ",dts=", packet.dts,
                            ",size=", packet.size,
                            ",flags=", packet.flags);

        // Is this a packet from the video stream?
        bool bVideo = (packet.stream_index == video_stream_idx);

        if(bVideo)
        {
            // write to moment videostream
            pParent->doVideoData(packet,
                                 format_ctx->streams[video_stream_idx]->codec,
                                 format_ctx->streams[video_stream_idx]->time_base.den);
        }

        // write to file
        if(m_bRecordingState)
        {
            m_nvrData.WritePacket(format_ctx, packet);
        }

        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);

        if(bVideo)
        {
            video_found = true;
            break;
        }

        memset(&packet, 0, sizeof(packet));
    }

    return video_found;
}

int FFmpegStream::ffmpegStreamData::SetChannelState(bool const bState)
{
    m_bRecordingState = bState;
    return 0;
}

int FFmpegStream::ffmpegStreamData::GetChannelState(bool & bState)
{
    bState = m_bRecordingState;
    return 0;
}

FFmpegStream::ffmpegStreamData::nvrData::nvrData(void)
{
    m_file_duration_sec = 0;
    m_max_age_minutes = 0;

    m_out_format_ctx = NULL;

    m_bIsInit = false;
}

FFmpegStream::ffmpegStreamData::nvrData::~nvrData(void)
{
    Deinit();
}

Result FFmpegStream::ffmpegStreamData::nvrData::Init(AVFormatContext * format_ctx, const char * channel_name,
                                                     const char * record_dir, Uint64 file_duration_sec, Uint64 max_age_minutes)
{
    if(!format_ctx || !channel_name || !record_dir)
        return Result::Failure;

    if(format_ctx->nb_streams <= 0)
    {
        logE_ (_func, "input number of streams = 0!");
        return Result::Failure;
    }

    // set internal members
    m_record_dir = st_makeString(record_dir);
    m_channel_name = st_makeString(channel_name);
    m_file_duration_sec = file_duration_sec;
    m_max_age_minutes = max_age_minutes;

    const char OUTPUT_FILE_FORMAT[] = ".flv"; // TODO: may be mp4?
    StRef<String> const path = st_makeString (m_record_dir->cstr(), "/", m_channel_name->cstr(), OUTPUT_FILE_FORMAT);

    logD_ (_func, "The output path: ", path);
    Result res = Result::Success;

    if(avformat_alloc_output_context2(&m_out_format_ctx, NULL, "segment2", path->cstr()) >= 0)
    {
        for (int i = 0; i < format_ctx->nb_streams; ++i)
        {
            AVStream * pStream = NULL;

            if (!(pStream = avformat_new_stream(m_out_format_ctx, NULL)))
            {
                logE_(_func_, "avformat_new_stream failed!");
                res = Result::Failure;
                break;
            }

            AVCodecContext * pInpCodec = format_ctx->streams[i]->codec;
            AVCodecContext * pOutCodec = pStream->codec;

            if(avcodec_copy_context(pOutCodec, pInpCodec) == 0)
            {

                if (!m_out_format_ctx->oformat->codec_tag ||
                    av_codec_get_id (m_out_format_ctx->oformat->codec_tag, pInpCodec->codec_tag) == pOutCodec->codec_id ||
                    av_codec_get_tag(m_out_format_ctx->oformat->codec_tag, pInpCodec->codec_id) <= 0)
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
                res = Result::Failure;
                break;
            }
        }
    }
    else
    {
        logE_(_func_, "avformat_alloc_output_context2 failed!");
        res = Result::Failure;
    }

    if(res == Result::Success)
    {
        av_dump_format(m_out_format_ctx, 0, path->cstr(), 1);

        AVOutputFormat * fmt = m_out_format_ctx->oformat;

        if (!(fmt->flags & AVFMT_NOFILE))
        {
            assert(false);
            logE_(_func_, "fmt->flags doesnt contain AVFMT_NOFILE");
            res = Result::Failure;
        }
    }

    if(res == Result::Success)
    {
        AVDictionary * pOptions = NULL;

        av_dict_set(&pOptions, "segment_time", st_makeString(m_file_duration_sec)->cstr(), 0);

        int ret = avformat_write_header(m_out_format_ctx, &pOptions);

        av_dict_free(&pOptions);

        if (ret < 0)
        {
            logE_(_func_, "Error occurred when opening output file");
            res = Result::Failure;
        }
    }

    if(res == Result::Success)
    {
        m_bIsInit = true;
    }
    else
    {
        Deinit();
    }

    return res;
}

void FFmpegStream::ffmpegStreamData::nvrData::Deinit()
{
    if(m_out_format_ctx)
    {
        av_write_trailer(m_out_format_ctx);

        FFmpegStream::ffmpegStreamData::CloseCodecs(m_out_format_ctx);

        avformat_free_context(m_out_format_ctx);

        m_out_format_ctx = NULL;
    }

    m_record_dir = NULL;
    m_channel_name = NULL;

    m_file_duration_sec = 0;
    m_max_age_minutes = 0;

    m_bIsInit = false;
}

bool FFmpegStream::ffmpegStreamData::nvrData::IsInit() const
{
    return m_bIsInit;
}

Result FFmpegStream::ffmpegStreamData::nvrData::WritePacket(const AVFormatContext * inCtx, /*const*/ AVPacket & packet)
{
    if(!IsInit())
        return Result::Failure;

//    AVPacket opkt;
//    av_init_packet(&opkt);

//    if (packet.pts != AV_NOPTS_VALUE)
//    {
//        opkt.pts = av_rescale_q(packet.pts, inCtx->streams[packet.stream_index]->time_base, m_out_format_ctx->streams[packet.stream_index]->time_base);
//        av_log(NULL, 0, "**********packet.pts != AV_NOPTS_VALUE\n");
//    }
//    else
//    {
//        opkt.pts = AV_NOPTS_VALUE;
//        av_log(NULL, 0, "**********packet.pts == AV_NOPTS_VALUE\n");
//    }

//    if (packet.dts == AV_NOPTS_VALUE)
//    {
//        opkt.dts = AV_NOPTS_VALUE;//av_rescale_q(inCtx->dts, AV_TIME_BASE_Q, m_out_format_ctx->streams[packet.stream_index]->time_base);
//        av_log(NULL, 0, "**********packet.dts == AV_NOPTS_VALUE\n");
//    }
//    else
//    {
//        opkt.dts = av_rescale_q(packet.dts, inCtx->streams[packet.stream_index]->time_base, m_out_format_ctx->streams[packet.stream_index]->time_base);
//        av_log(NULL, 0, "**********packet.dts != AV_NOPTS_VALUE\n");
//    }

//    opkt.duration = av_rescale_q(packet.duration, inCtx->streams[packet.stream_index]->time_base, m_out_format_ctx->streams[packet.stream_index]->time_base);
//    opkt.flags    = packet.flags;
//    opkt.data = packet.data;
//    opkt.size = packet.size;
//    opkt.stream_index = packet.stream_index;

    return (av_interleaved_write_frame(m_out_format_ctx, &/*opkt*/packet) == 0) ? Result::Success : Result::Failure;
}

int FFmpegStream::SetChannelState(bool const bState)
{
    return m_ffmpegStreamData.SetChannelState(bState);
}

int FFmpegStream::GetChannelState(bool & bState)
{
    return m_ffmpegStreamData.GetChannelState(bState);
}

int FFmpegStream::WriteB8ToBuffer(Int32 b, MemoryEx & memory)
{
    if(memory.size() >= memory.len())
        return -1;

    Size curPos = memory.size();
    Byte * pMem = memory.mem();
    pMem[curPos] = (Byte)b;

    memory.setSize(curPos + 1);

    if(memory.size() >= memory.len())
    {
        logE_ (_func, "Fail, size(", memory.size(), ") > memoryOut.len(", memory.len(), ")");
        return Result::Failure;
    }

    return Result::Success;
}

int FFmpegStream::WriteB16ToBuffer(Uint32 val, MemoryEx & memory)
{
    if( WriteB8ToBuffer((Byte)(val >> 8 ), memory) == Result::Success &&
        WriteB8ToBuffer((Byte) val       , memory) == Result::Success)
    {
        return Result::Success;
    }

    return Result::Failure;
}

int FFmpegStream::WriteB32ToBuffer(Uint32 val, MemoryEx & memory)
{
    if( WriteB8ToBuffer(       val >> 24 , memory) == Result::Success &&
        WriteB8ToBuffer((Byte)(val >> 16), memory) == Result::Success &&
        WriteB8ToBuffer((Byte)(val >> 8 ), memory) == Result::Success &&
        WriteB8ToBuffer((Byte) val       , memory) == Result::Success)
    {
        return Result::Success;
    }

    return Result::Failure;
}

int FFmpegStream::WriteDataToBuffer(ConstMemory const memory, MemoryEx & memoryOut)
{
    Size inputSize = memory.len();

    if(memoryOut.size() + inputSize >= memoryOut.len())
    {
        logE_ (_func, "Fail, size(", memoryOut.size(), ") + inputSize(", inputSize, ") > memoryOut.len(", memoryOut.len(), ")");
        return Result::Failure;
    }

    if(inputSize >= memoryOut.len())
    {
        logE_ (_func, "inputSize(", inputSize, ") >= memoryOut.len(", memoryOut.len(), ")");
        return Result::Failure;
    }
    else if(inputSize > 0)
    {
        memcpy(memoryOut.mem() + memoryOut.size(), memory.mem(), inputSize);

        memoryOut.setSize(memoryOut.size() + inputSize);
    }

    return Result::Success;
}



static const Byte * AvcFindStartCodeInternal(const Byte *p, const Byte *end)
{
    const Byte *a = p + 4 - ((intptr_t)p & 3);

    for (end -= 3; p < a && p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4)
    {
        Uint32 x = *(const Uint32*)p;
        //      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
        //      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
        if ((x - 0x01010101) & (~x) & 0x80808080)
        {
            // generic
            if (p[1] == 0)
            {
                if (p[0] == 0 && p[2] == 1)
                    return p;
                if (p[2] == 0 && p[3] == 1)
                    return p+1;
            }

            if (p[3] == 0)
            {
                if (p[2] == 0 && p[4] == 1)
                    return p+2;
                if (p[4] == 0 && p[5] == 1)
                    return p+3;
            }
        }
    }

    for (end += 3; p < end; p++)
    {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return end + 3;
}

static const Byte * AvcFindStartCode(const Byte *p, const Byte *end)
{
    const Byte * out = AvcFindStartCodeInternal(p, end);

    if(p < out && out < end && !out[-1])
        out--;

    return out;
}

int FFmpegStream::AvcParseNalUnits(ConstMemory const mem, MemoryEx * pMemoryOut)
{
    //logD_ (_func);

    const Byte *p = mem.mem();
    const Byte *end = p + mem.len();
    const Byte *nal_start, *nal_end;

    Int32 size = 0;

    nal_start = AvcFindStartCode(p, end);

    for (;;)
    {
        while (nal_start < end && !*(nal_start++));

        if (nal_start == end)
            break;

        nal_end = AvcFindStartCode(nal_start, end);

        if(pMemoryOut)
        {
            WriteB32ToBuffer(nal_end - nal_start, *pMemoryOut);
            WriteDataToBuffer(ConstMemory(nal_start, nal_end - nal_start), *pMemoryOut);
        }

        size += 4 + nal_end - nal_start;
        nal_start = nal_end;
    }

    //logD_ (_func, "Returned size = ", size);

    return size;
}



int FFmpegStream::IsomWriteAvcc(ConstMemory const memory, MemoryEx & memoryOut)
{
    //logD_ (_func, "IsomWriteAvcc = ", memory.len());

    if (memory.len() > 6)
    {
        Byte const * data = memory.mem();

        // check for h264 start code
        if (AV_RB32(data) == 0x00000001 || AV_RB24(data) == 0x000001)
        {
            MemorySafe allocMemory(memory.len() * 2);
            MemoryEx localMemory((Byte *)allocMemory.cstr(), allocMemory.len());

            if(AvcParseNalUnits(memory, &localMemory) < 0)
            {
                logE_ (_func, "AvcParseNalUnits fails");
                return Result::Failure;
            }

            Byte * buf = localMemory.mem();
            Byte * end = buf + localMemory.size();
            Uint32 sps_size = 0, pps_size = 0;
            Byte * sps = 0, *pps = 0;
            // look for sps and pps
            while (end - buf > 4)
            {
                Uint32 size;
                Byte nal_type;

                size = std::min((Uint32)AV_RB32(buf), (Uint32)(end - buf - 4));

                buf += 4;
                nal_type = buf[0] & 0x1f;

                if (nal_type == 7)
                {
                    // SPS
                    sps = buf;
                    sps_size = size;
                }
                else if (nal_type == 8)
                {
                    // PPS
                    pps = buf;
                    pps_size = size;
                }

                buf += size;
            }

            //logD_(_func, "sps = ", sps ? "valid" : "invalid", ", pps = ", pps ? "valid" : "invalid", ", sps_size = ", sps_size, ", pps_size = ", pps_size);

            if (!sps || !pps || sps_size < 4 || sps_size > Uint16_Max || pps_size > Uint16_Max)
            {
                logE_ (_func, "Failed to get sps, pps");
                return Result::Failure;
            }

            WriteB8ToBuffer(1, memoryOut);			// version
            WriteB8ToBuffer(sps[1], memoryOut);	// profile
            WriteB8ToBuffer(sps[2], memoryOut);	// profile compat
            WriteB8ToBuffer(sps[3], memoryOut);	// level
            WriteB8ToBuffer(0xff, memoryOut);		// 6 bits reserved (111111) + 2 bits nal size length - 1 (11)
            WriteB8ToBuffer(0xe1, memoryOut);		// 3 bits reserved (111) + 5 bits number of sps (00001)

            WriteB16ToBuffer(sps_size, memoryOut);
            WriteDataToBuffer(ConstMemory(sps, sps_size), memoryOut);
            WriteB8ToBuffer(1, memoryOut);			// number of pps
            WriteB16ToBuffer(pps_size, memoryOut);
            WriteDataToBuffer(ConstMemory(pps, pps_size), memoryOut);
        }
        else
        {
            logD_ (_func, "AVC header, ONLY WriteDataInternal, ", data[0], ", ", data[1], ", ", data[2], ", ", data[3], ", ");
            WriteDataToBuffer(memory, memoryOut);
        }
    }

    return Result::Success;
}

void
FFmpegStream::reportStatusEvents ()
{
    logD_("task is reported");
    deferred_reg.scheduleTask (&deferred_task, false /* permanent */);
}

bool
FFmpegStream::deferredTask (void * const _self)
{
    FFmpegStream * const self = static_cast <FFmpegStream*> (_self);
    self->doReportStatusEvents ();
    return false /* do not reschedule */;
}


void
FFmpegStream::workqueueThreadFunc (void * const _self)
{
    FFmpegStream * const self = static_cast <FFmpegStream*> (_self);

    updateTime ();

    logD (pipeline, _self_func_);

    self->mutex.lock ();

    self->tlocal = libMary_getThreadLocal();

    for (;;) {
        if (self->stream_closed) {
            self->mutex.unlock ();
            return;
        }

        while (self->workqueue_list.isEmpty()) {
            self->workqueue_cond.wait (self->mutex);
            updateTime ();

            if (self->stream_closed) {
                self->mutex.unlock ();
                return;
            }
        }

        Ref<WorkqueueItem> const workqueue_item = self->workqueue_list.getFirst();
        self->workqueue_list.remove (self->workqueue_list.getFirstElement());

        self->mutex.unlock ();

        switch (workqueue_item->item_type) {
            case WorkqueueItem::ItemType_CreatePipeline:
                self->doCreatePipeline ();
                break;
            case WorkqueueItem::ItemType_ReleasePipeline:
                self->doReleasePipeline ();
                break;
            default:
                unreachable ();
        }

        self->mutex.lock ();
    }
}

void
FFmpegStream::ffmpegThreadFunc (void * const _self)
{
    FFmpegStream * const self = static_cast <FFmpegStream*> (_self);

    logD_(_func_);

    self->beginPushPacket();

    logD_(_func_, "End of ffmpeg_thread");
}

void
FFmpegStream::beginPushPacket()
{
    m_bIsPlaying = true;

    mutex.lock ();
    if (channel_opts->no_video_timeout > 0)
    {
        updateTime ();
        last_frame_time = getTime ();
        no_video_timer = timers->addTimer (CbDesc<Timers::TimerCallback> (noVideoTimerTick,
                                                                            this /* cb_data */,
                                                                            this /* coderef_container */),
                                                                            channel_opts->no_video_timeout,
                                                                            true  /* periodical */,
                                                                            false /* auto_delete */);
    }

    mutex.unlock ();

    while(m_bIsPlaying)
    {
        if(!m_ffmpegStreamData.PushVideoPacket(this))
        {
            logD_(_func_, "EOS");

            m_bIsPlaying = false;

            eos_pending = true;

            reportStatusEvents ();
        }
    }
}

void
FFmpegStream::createSmartPipelineForUri ()
{
    logD_ ("FFmpegStream::createSmartPipelineForUri ()");
    assert (playback_item->spec_kind == PlaybackItem::SpecKind::Uri);

    if (playback_item->stream_spec->mem().len() == 0) {
        logE_ (_this_func, "URI not specified for channel \"", channel_opts->channel_name, "\"");
        return;
    }

    if (stream_closed) {
        mutex.lock ();
        logE_ (_this_func, "stream closed, channel \"", channel_opts->channel_name, "\"");
        goto _failure;
    }

    logD_ (_func, "uri: ", playback_item->stream_spec);

    if(m_ffmpegStreamData.Init( playback_item->stream_spec->cstr(), channel_opts->channel_name->cstr(), this->config, this->timers) != Result::Success)
    {
        mutex.lock ();
        logE_ (_this_func, "stream is not initialized, uri \"", playback_item->stream_spec->cstr(), "\"");
        goto _failure;
    }

    ffmpeg_thread =
            grab (new (std::nothrow) Thread (CbDesc<Thread::ThreadFunc> (ffmpegThreadFunc,
                                                                         this,
                                                                         this)));
    if (!ffmpeg_thread->spawn (true /* joinable */))
        logE_ (_func, "Failed to spawn ffmpeg thread: ", exc->toString());


    goto _return;

mt_mutex (mutex) _failure:
    mt_unlocks (mutex) pipelineCreationFailed ();

_return:
    ;

}

mt_unlocks (mutex) void
FFmpegStream::pipelineCreationFailed ()
{

    logD (pipeline, _this_func_);

    eos_pending = true;
    mutex.unlock ();

    releasePipeline ();

}

void
FFmpegStream::doReleasePipeline ()
{
    // some deinit, if it's needed

    logD_(_func_);

    m_bIsPlaying = false;

//    bool const join = (tlocal != libMary_getThreadLocal());

//    if (join) {
        if(ffmpeg_thread)
        {
            ffmpeg_thread->join();
        }
//    }

    m_ffmpegStreamData.Deinit();

    mutex.lock ();
    {
        stream_closed = true;
    }
    mutex.unlock ();


    reportStatusEvents ();
    logD_(_func_, "released, m_bIsPlaying is false");
}

void
FFmpegStream::releasePipeline ()
{

    logD (pipeline, _this_func_);

//#error Two bugs here:
//#error   1. If the pipeline is being released (got an item in the workqueue),
//#error      then we should wait for the thread to join, but we don't.
//#error   2. Simultaneous joining of the same thread is undefined behavior.

    mutex.lock ();

    while (!workqueue_list.isEmpty()) {
        Ref<WorkqueueItem> &last_item = workqueue_list.getLast();
        switch (last_item->item_type) {
            case WorkqueueItem::ItemType_CreatePipeline:
                workqueue_list.remove (workqueue_list.getLastElement());
                break;
            case WorkqueueItem::ItemType_ReleasePipeline:
                mutex.unlock ();
                return;
            default:
                unreachable ();
        }
    }

    Ref<WorkqueueItem> const new_item = grab (new (std::nothrow) WorkqueueItem);
    new_item->item_type = WorkqueueItem::ItemType_ReleasePipeline;

    workqueue_list.prepend (new_item);
    workqueue_cond.signal ();

    bool const join = (tlocal != libMary_getThreadLocal());

    Ref<Thread> const tmp_workqueue_thread = workqueue_thread;
    workqueue_thread = NULL;

    mutex.unlock ();

    if (join) {
        if (tmp_workqueue_thread)
            tmp_workqueue_thread->join ();
    }

}

mt_mutex (mutex) void
FFmpegStream::reportMetaData ()
{
    logD (stream, _func_);

    if (metadata_reported) {
    return;
    }
    metadata_reported = true;

    if (!playback_item->send_metadata)
    return;

    VideoStream::VideoMessage msg;
    if (!RtmpServer::encodeMetaData (&metadata, page_pool, &msg)) {
    logE_ (_func, "encodeMetaData() failed");
    return;
    }

    logD (stream, _func, "Firing video message");
    // TODO unlock/lock? (locked event == WRONG)
    video_stream->fireVideoMessage (&msg);

    page_pool->msgUnref (msg.page_list.first);
}


void
FFmpegStream::doVideoData (AVPacket & packet, AVCodecContext * pctx, int time_base_den)
{

    updateTime ();

//    mutex.lock ();

    last_frame_time = getTime ();

    if (first_video_frame)
    {
        first_video_frame = false;

        video_codec_id = VideoStream::VideoCodecId::AVC;
        is_h264_stream = true;

        if (playback_item->send_metadata) {
            if (!got_audio || !first_audio_frame) {
              // There's no video or we've got the first video frame already.
                reportMetaData ();
            }
        }
    }

    bool skip_frame = false;
    {
        if (video_skip_counter > 0) {
            --video_skip_counter;
            logD (frames, _func, "Skipping frame, video_skip_counter: ", video_skip_counter);
            skip_frame = true;
        }

        if (!initial_seek_complete) {
            // We have not started playing yet. This is most likely a preroll frame.
            logD (frames, _func, "Skipping an early preroll frame");
            skip_frame = true;
        }
    }

    VideoStream::VideoCodecId const tmp_video_codec_id = video_codec_id;

    if (is_h264_stream) {

        bool report_avc_codec_data = false;
        {
            do {
                MemorySafe allocMemory(pctx->extradata_size * 2);
                MemoryEx m_OutMemory((Byte *)allocMemory.cstr(), allocMemory.len());
                memset(m_OutMemory.mem(), 0, m_OutMemory.len());
                IsomWriteAvcc(ConstMemory(pctx->extradata, pctx->extradata_size), m_OutMemory);


                if (avc_codec_data_buffer) {
                    if (m_OutMemory.size() == avc_codec_data_buffer_size)
                    {
                        if (!memcmp(m_OutMemory.mem(), avc_codec_data_buffer, avc_codec_data_buffer_size))
                        {
                            // Codec data has not changed.
                            //logD_ (_func, "Codec data has not changed");
                              break;
                        }
                    }
                }

                if (avc_codec_data_buffer)
                {
                    delete [] avc_codec_data_buffer;
                    avc_codec_data_buffer_size = 0;
                }
                avc_codec_data_buffer = new u_int8_t [m_OutMemory.size()];
                memcpy(avc_codec_data_buffer, m_OutMemory.mem(), m_OutMemory.size());
                avc_codec_data_buffer_size = m_OutMemory.size();
                report_avc_codec_data = true;
            } while (0);
        }

        if (report_avc_codec_data) {
            // TODO vvv This doesn't sound correct.
            //
            // Timestamps for codec data buffers are seemingly random.
            Uint64 const timestamp_nanosec = 0;

            Size msg_len = 0;

            if (logLevelOn (frames, LogLevel::D)) {
                logLock ();
                log_unlocked__ (_func, "AVC SEQUENCE HEADER");
                hexdump (logs, ConstMemory (avc_codec_data_buffer, avc_codec_data_buffer_size));
                logUnlock ();
            }

            PagePool::PageListHead page_list;

            if (playback_item->enable_prechunking) {
                RtmpConnection::PrechunkContext prechunk_ctx (5 /* initial_offset: FLV AVC header length */);
                RtmpConnection::fillPrechunkedPages (&prechunk_ctx,
                                                     ConstMemory (avc_codec_data_buffer,
                                                                  avc_codec_data_buffer_size),
                                                     page_pool,
                                                     &page_list,
                                                     RtmpConnection::DefaultVideoChunkStreamId,
                                                     timestamp_nanosec / 1000000,
                                                     false /* first_chunk */);
            } else {
            page_pool->getFillPages (&page_list,
                                     ConstMemory (avc_codec_data_buffer,
                                                  avc_codec_data_buffer_size));
            }
            msg_len += avc_codec_data_buffer_size;

            VideoStream::VideoMessage msg;
            msg.timestamp_nanosec = timestamp_nanosec;
            msg.prechunk_size = (playback_item->enable_prechunking ? RtmpConnection::PrechunkSize : 0);
            msg.frame_type = VideoStream::VideoFrameType::AvcSequenceHeader;
            msg.codec_id = tmp_video_codec_id;

            msg.page_pool = page_pool;
            msg.page_list = page_list;
            msg.msg_len = msg_len;
            msg.msg_offset = 0;

            if (logLevelOn (frames, LogLevel::D)) {
                logD_ (_func, "Firing video message (AVC sequence header):");
                logLock ();
                PagePool::dumpPages (logs, &page_list);
                logUnlock ();
            }

            video_stream->fireVideoMessage (&msg);

            page_pool->msgUnref (page_list.first);
        } // if (report_avc_codec_data)
    } // if (is_h264_stream)

    if (skip_frame) {
        logD (frames, _func, "skipping frame");
        return;
    }

    Uint64 const timestamp_nanosec = packet.pts / (double)time_base_den * 1000000000LL;

    VideoStream::VideoMessage msg;
    msg.frame_type = VideoStream::VideoFrameType::InterFrame;
    msg.codec_id = tmp_video_codec_id;

    if (packet.flags & AV_PKT_FLAG_KEY)
    {
        //logD_(_func_, "IT IS KEY FRAME\n");
        msg.frame_type = VideoStream::VideoFrameType::KeyFrame;
    }

    MemorySafe allocMemory(packet.size * 2);
    MemoryEx localMemory((Byte *)allocMemory.cstr(), allocMemory.len());
    size_t sizee = 0;
    if(sizee = AvcParseNalUnits(ConstMemory(packet.data, packet.size), &localMemory) < 0)
    {
        logE_ (_func, "AvcParseNalUnits fails");
        return;
    }

    PagePool::PageListHead page_list;
    if (playback_item->enable_prechunking) {
        Size gen_video_hdr_len = 1;
        if (tmp_video_codec_id == VideoStream::VideoCodecId::AVC)
            gen_video_hdr_len = 5;


        RtmpConnection::PrechunkContext prechunk_ctx (gen_video_hdr_len /* initial_offset */);
        RtmpConnection::fillPrechunkedPages (&prechunk_ctx,
                                             ConstMemory (/*packet.data*/localMemory.mem(),
                                                          /*packet.size*/localMemory.size()),
                                             page_pool,
                                             &page_list,
                                             RtmpConnection::DefaultVideoChunkStreamId,
                                             timestamp_nanosec / 1000000,
                                             false /* first_chunk */);


//    logD_ (_func, "size of packet = ", localMemory.size());

    } else {
    page_pool->getFillPages (&page_list,
                  ConstMemory (packet.data, packet.size));
    }

    Size msg_len = localMemory.size();
    msg.timestamp_nanosec = timestamp_nanosec;

    msg.prechunk_size = (playback_item->enable_prechunking ? RtmpConnection::PrechunkSize : 0);

    msg.page_pool = page_pool;
    msg.page_list = page_list;
    msg.msg_len = msg_len;
    msg.msg_offset = 0;

    video_stream->fireVideoMessage (&msg);

    page_pool->msgUnref (page_list.first);

}

#if 0
gboolean
FFmpegStream::videoDataCb (GstPad    * const /* pad */,
            GstBuffer * const buffer,
            gpointer    const _self)
{
    FFmpegStream * const self = static_cast <FFmpegStream*> (_self);
    self->doVideoData (buffer);
    return TRUE;
}

void
FFmpegStream::handoffVideoDataCb (GstElement * const /* element */,
                   GstBuffer  * const buffer,
                   GstPad     * const /* pad */,
                   gpointer     const _self)
{
    FFmpegStream * const self = static_cast <FFmpegStream*> (_self);
    self->doVideoData (buffer);
}
#endif

void
FFmpegStream::noVideoTimerTick (void * const _self)
{
    FFmpegStream * const self = static_cast <FFmpegStream*> (_self);

    Time const time = getTime();

    self->mutex.lock ();

    logD_(_func_, "time: 0x", fmt_hex, time, ", last_frame_time: 0x", self->last_frame_time);

    if (self->stream_closed) {
        self->mutex.unlock ();
        return;
    }

    if (time > self->last_frame_time &&
    time - self->last_frame_time >= 15 /* TODO Config param for the timeout */)
    {
        logD (novideo, _func, "firing \"no video\" event");

        if (self->no_video_timer) {
            self->timers->deleteTimer (self->no_video_timer);
            self->no_video_timer = NULL;
        }

        self->no_video_pending = true;
        self->mutex.unlock ();

        self->doReportStatusEvents ();
    } else {
        self->got_video_pending = true;
        self->mutex.unlock ();

        self->doReportStatusEvents ();
    }
}

VideoStream::EventHandler FFmpegStream::mix_stream_handler = {
    NULL/*mixStreamAudioMessage*/,
    NULL/*mixStreamVideoMessage*/,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    NULL /* numWatchersChanged */
};

void
FFmpegStream::doCreatePipeline ()
{
    logD (pipeline, _this_func_);

    if (playback_item->spec_kind == PlaybackItem::SpecKind::Chain) {
        logD_ (_func, "place for createPipelineForChainSpec()");
    } else
    if (playback_item->spec_kind == PlaybackItem::SpecKind::Uri) {
        createSmartPipelineForUri ();
    } else {
        assert (playback_item->spec_kind == PlaybackItem::SpecKind::None);
    }
}

void
FFmpegStream::createPipeline ()
{
    logD (pipeline, _this_func_);

    mutex.lock ();

    while (!workqueue_list.isEmpty()) {
        Ref<WorkqueueItem> &last_item = workqueue_list.getLast();
        switch (last_item->item_type) {
            case WorkqueueItem::ItemType_CreatePipeline:
                mutex.unlock ();
                return;
            case WorkqueueItem::ItemType_ReleasePipeline:
                mutex.unlock ();
                return;
            default:
                unreachable ();
        }
    }

    Ref<WorkqueueItem> const new_item = grab (new (std::nothrow) WorkqueueItem);
    new_item->item_type = WorkqueueItem::ItemType_CreatePipeline;

    workqueue_list.prepend (new_item);
    workqueue_cond.signal ();

    mutex.unlock ();
}

void
FFmpegStream::doReportStatusEvents ()
{
    logD (stream, _func_);

    mutex.lock ();
    if (reporting_status_events) {
    mutex.unlock ();
    return;
    }
    reporting_status_events = true;

    while (eos_pending       ||
       error_pending     ||
       no_video_pending  ||
       got_video_pending ||
       seek_pending      ||
       play_pending)
    {
    if (close_notified) {
        logD (stream, _func, "close_notified");

        mutex.unlock ();
        return;
    }

    if (eos_pending) {
        logD (stream, _func, "eos_pending");
        eos_pending = false;
        close_notified = true;

        if (frontend) {
        logD (stream, _func, "firing EOS");
        mt_unlocks_locks (mutex) frontend.call_mutex (frontend->eos, mutex);
        }

        break;
    }

    if (error_pending) {
        logD (stream, _func, "error_pending");
        error_pending = false;
        close_notified = true;

        if (frontend) {
        logD (stream, _func, "firing ERROR");
        mt_unlocks_locks (mutex) frontend.call_mutex (frontend->error, mutex);
        }

        break;
    }

    if (no_video_pending) {
            got_video_pending = false;

        logD (stream, _func, "no_video_pending");
        no_video_pending = false;
        if (stream_closed) {
        mutex.unlock ();
        return;
        }

        if (frontend) {
        logD (stream, _func, "firing NO_VIDEO");
        mt_unlocks_locks (mutex) frontend.call_mutex (frontend->noVideo, mutex);
        }
    }

    if (got_video_pending) {
        logD (stream, _func, "got_video_pending");
        got_video_pending = false;
        if (stream_closed) {
        mutex.unlock ();
        return;
        }

        if (frontend)
        mt_unlocks_locks (mutex) frontend.call_mutex (frontend->gotVideo, mutex);
    }

//    if (seek_pending) {
//        logD (stream, _func, "seek_pending");
//        assert (!play_pending);
//        seek_pending = false;
//        if (stream_closed) {
//        mutex.unlock ();
//        return;
//        }

//        if (initial_seek > 0) {
//        logD (stream, _func, "initial_seek: ", initial_seek);

//        Time const tmp_initial_seek = initial_seek;

//        GstElement * const tmp_playbin = playbin;
//        gst_object_ref (tmp_playbin);
//        mutex.unlock ();

//        bool seek_failed = false;
//        if (!gst_element_seek_simple (tmp_playbin,
//                          GST_FORMAT_TIME,
//                          (GstSeekFlags) (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
//                          (GstClockTime) tmp_initial_seek * 1000000000LL))
//        {
//            seek_failed = true;
//            logE_ (_func, "Seek failed");
//        }

//        gst_object_unref (tmp_playbin);
//        mutex.lock ();
//        if (seek_failed)
//            play_pending = true;
//        } else {
//        play_pending = true;
//        }
//    }

//    if (play_pending) {
//        logD (stream, _func, "play_pending");
//        assert (!seek_pending);
//        play_pending = false;
//        if (stream_closed) {
//        mutex.unlock ();
//        return;
//        }

//        GstElement * const tmp_playbin = playbin;
//        gst_object_ref (tmp_playbin);
//        mutex.unlock ();

//        logD (stream, _func, "Setting pipeline state to PLAYING");
//        if (gst_element_set_state (tmp_playbin, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
//        logE_ (_func, "gst_element_set_state() failed (PLAYING)");

//        gst_object_unref (tmp_playbin);
//        mutex.lock ();
//    }
    }

    logD (stream, _func, "done");
    reporting_status_events = false;
    mutex.unlock ();
}

void
FFmpegStream::getTrafficStats (TrafficStats * const mt_nonnull ret_traffic_stats)
{
    mutex.lock ();
    ret_traffic_stats->rx_bytes = rx_bytes;
    ret_traffic_stats->rx_audio_bytes = rx_audio_bytes;
    ret_traffic_stats->rx_video_bytes = rx_video_bytes;
    mutex.unlock ();
}

void
FFmpegStream::resetTrafficStats ()
{
    rx_bytes = 0;
    rx_audio_bytes = 0;
    rx_video_bytes = 0;
}

mt_const void
FFmpegStream::init (CbDesc<MediaSource::Frontend> const &frontend,
                 Timers            * const timers,
                 DeferredProcessor * const deferred_processor,
                 PagePool          * const page_pool,
                 VideoStream       * const video_stream,
                 VideoStream       * const mix_video_stream,
                 Time                const initial_seek,
                 ChannelOptions    * const channel_opts,
                 PlaybackItem      * const playback_item,
                 const Ref<MConfig::Config> & config)
{
    logD (pipeline, _this_func_);

    this->frontend = frontend;

    this->timers = timers;
    this->page_pool = page_pool;
    this->video_stream = video_stream;
    this->mix_video_stream = mix_video_stream;

    this->channel_opts  = channel_opts;

    this->playback_item = playback_item;

    this->initial_seek = initial_seek;
    if (initial_seek == 0)
        initial_seek_complete = true;

    this->config = config;

    deferred_reg.setDeferredProcessor (deferred_processor);

    {
        workqueue_thread =
                grab (new (std::nothrow) Thread (CbDesc<Thread::ThreadFunc> (workqueueThreadFunc,
                                                                             this,
                                                                             this)));
        if (!workqueue_thread->spawn (true /* joinable */))
            logE_ (_func, "Failed to spawn workqueue thread: ", exc->toString());
    }
}

FFmpegStream::FFmpegStream ()
    : timers    (this /* coderef_container */),
      page_pool (this /* coderef_container */),

      video_stream (NULL),
      mix_video_stream (NULL),

      no_video_timer (NULL),

      initial_seek (0),
      initial_seek_pending  (true),
      initial_seek_complete (false),
      initial_play_pending  (true),

      metadata_reported (false),

      last_frame_time (0),

      audio_codec_id (VideoStream::AudioCodecId::Unknown),
      audio_rate (44100),
      audio_channels (1),

      video_codec_id (VideoStream::VideoCodecId::Unknown),

      got_in_stats (false),
      got_video (false),
      got_audio (false),

      first_audio_frame (true),
      first_video_frame (true),

      audio_skip_counter (0),
      video_skip_counter (0),

      is_adts_aac_stream (false),
      got_adts_aac_codec_data (false),

      is_h264_stream (false),
      avc_codec_data_buffer(NULL),
      avc_codec_data_buffer_size(0),

      prv_audio_timestamp (0),

      changing_state_to_playing (false),
      reporting_status_events (false),

      seek_pending (false),
      play_pending (false),

      no_video_pending (false),
      got_video_pending (false),
      error_pending  (false),
      eos_pending    (false),
      close_notified (false),

      stream_closed (false),

      rx_bytes (0),
      rx_audio_bytes (0),
      rx_video_bytes (0),

      tlocal (NULL),
      m_bIsPlaying(false)
{
    logD (pipeline, _this_func_);

    deferred_task.cb = CbDesc<DeferredProcessor::TaskCallback> (deferredTask, this, this);
}

FFmpegStream::~FFmpegStream ()
{
    logD (pipeline, _this_func_);

    mutex.lock ();
    assert (stream_closed);
    assert (!workqueue_thread);
    mutex.unlock ();

    m_ffmpegStreamData.Deinit();

    if (avc_codec_data_buffer)
    {
        delete [] avc_codec_data_buffer;
        avc_codec_data_buffer_size = 0;
        avc_codec_data_buffer = NULL;
    }

    deferred_reg.release ();
}

}

