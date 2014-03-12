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
#include <moment-ffmpeg/nvr_file_iterator.h>
#include <moment-ffmpeg/media_reader.h>
#include <moment-ffmpeg/memory_dispatcher.h>
#include <moment-ffmpeg/ffmpeg_common.h>
#include "naming_scheme.h"
#include <libmary/vfs.h>
#include <iostream>
#include <fstream>
#include <string>


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static LogGroup libMary_logGroup_pipeline ("mod_ffmpeg.pipeline", LogLevel::E);
static LogGroup libMary_logGroup_stream   ("mod_ffmpeg.stream",   LogLevel::E);
static LogGroup libMary_logGroup_bus      ("mod_ffmpeg.bus",      LogLevel::E);
static LogGroup libMary_logGroup_timer    ("mod_ffmpeg.timer",    LogLevel::E);
static LogGroup libMary_logGroup_ffmpeg   ("mod_ffmpeg.ffmpeg",   LogLevel::E);
static LogGroup libMary_logGroup_frames   ("mod_ffmpeg.frames",   LogLevel::E); // E is the default

#define MAX_AGE 120 // in minutes
#define FILE_DURATION 3600 // in seconds

#define AV_RB32(x)									\
        (((Uint32)((const Byte*)(x))[0] << 24) |	\
        (((const Byte*)(x))[1] << 16) |				\
        (((const Byte*)(x))[2] <<  8) |				\
        ( (const Byte*)(x))[3])

#define AV_RB24(x)									\
        ((((const Byte*)(x))[0] << 16) |			\
        ( ((const Byte*)(x))[1] <<  8) |			\
        (  (const Byte*)(x))[2])

#define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |       \
      ((const uint8_t*)(x))[1])

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

        logD(ffmpeg, _func_, "new fullfilename: ", fullPath, ", "
               "next_unixtime_sec: ", next_unixtime_sec, ", cur unixtime: ", tv.tv_sec);

        return 0;
    }


    void CustomAVLogCallback(void * /*ptr*/, int /*level*/, const char * outFmt, va_list vl)
    {
        if(outFmt)
        {
            // TODO: need to use 'level'
            char chLogBuffer[2048] = {};
            vsnprintf(chLogBuffer, sizeof(chLogBuffer), outFmt, vl);
            //printf(chLogBuffer);
            logD(ffmpeg, _func_, "ffmpeg LOG: ", chLogBuffer);
        }
    }
}   // extern "C" ]

static Time ff_blocking_timeout = 30000000; // in microsec
static int interrupt_cb(void* arg)
{
    TimeChecker * tc = static_cast<TimeChecker*>(arg);
    Time t;
    tc->Stop(&t);

    if (t > ff_blocking_timeout)
    {
        return 1;
    }

    return 0;
}

#ifndef PLATFORM_WIN32
static int ff_lockmgr(void **mutex, enum AVLockOp op)
{
   pthread_mutex_t** pmutex = (pthread_mutex_t**) mutex;
   switch (op) {
   case AV_LOCK_CREATE:
      *pmutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
       pthread_mutex_init(*pmutex, NULL);
       break;
   case AV_LOCK_OBTAIN:
       pthread_mutex_lock(*pmutex);
       break;
   case AV_LOCK_RELEASE:
       pthread_mutex_unlock(*pmutex);
       break;
   case AV_LOCK_DESTROY:
       pthread_mutex_destroy(*pmutex);
       free(*pmutex);
       break;
   }
   return 0;
}
#else
static int ff_lockmgr(void **mutex, enum AVLockOp op)
{
    CRITICAL_SECTION **critSec = (CRITICAL_SECTION **)mutex;
    switch (op) {
    case AV_LOCK_CREATE:
        *critSec = new CRITICAL_SECTION();
        InitializeCriticalSection(*critSec);
        break;
    case AV_LOCK_OBTAIN:
        EnterCriticalSection(*critSec);
        break;
    case AV_LOCK_RELEASE:
        LeaveCriticalSection(*critSec);
        break;
    case AV_LOCK_DESTROY:
        DeleteCriticalSection(*critSec);
        delete *critSec;
        break;
    }
    return 0;
}
#endif



StateMutex ffmpegStreamData::g_mutexFFmpeg;

static void RegisterFFMpeg(void)
{
    logD(stream, _func_, "RegisterFFMpeg");

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
    av_lockmgr_register(ff_lockmgr);

    logD(stream, _func_, "RegisterFFMpeg succeed");
}


ffmpegStreamData::ffmpegStreamData(void)
{
    // Register all formats and codecs
    RegisterFFMpeg();

    format_ctx = NULL;
    m_absf_ctx = NULL;

    m_bRecordingState = false;
    m_bRecordingEnable = false;
    m_bIsRecording = false;
    m_bGotFirstFrame = false;
    m_pRecpathConfig = NULL;

    audio_stream_idx = video_stream_idx = -1;
    m_file_duration_sec = -1;
}

ffmpegStreamData::~ffmpegStreamData()
{
    Deinit();
}

void ffmpegStreamData::CloseCodecs(AVFormatContext * pAVFrmtCntxt)
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

void ffmpegStreamData::Deinit()
{
    logD(stream, _func_, "ffmpegStreamData Deinit");
    // close writer before any actions with reader
    m_nvrData.Deinit();

    if(format_ctx)
    {
        CloseCodecs(format_ctx);

        avformat_close_input(&format_ctx);

        format_ctx = NULL;
    }

    if (m_absf_ctx)
    {
        av_bitstream_filter_close(m_absf_ctx);
        m_absf_ctx = NULL;
    }

    m_bRecordingState = false;
    m_bRecordingEnable = false;
    m_bIsRecording = false;
    m_bGotFirstFrame = false;
    m_pRecpathConfig = NULL;

    audio_stream_idx = video_stream_idx = -1;
    m_file_duration_sec = -1;
}

static int get_bit_rate(AVCodecContext *ctx)
{
    int bit_rate;
    int bits_per_sample;

    switch (ctx->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
    case AVMEDIA_TYPE_DATA:
    case AVMEDIA_TYPE_SUBTITLE:
    case AVMEDIA_TYPE_ATTACHMENT:
        bit_rate = ctx->bit_rate;
        break;
    case AVMEDIA_TYPE_AUDIO:
        bits_per_sample = av_get_bits_per_sample(ctx->codec_id);
        bit_rate = bits_per_sample ? ctx->sample_rate * ctx->channels * bits_per_sample : ctx->bit_rate;
        break;
    default:
        bit_rate = 0;
        break;
    }
    return bit_rate;
}

Result ffmpegStreamData::Init(const char * uri, const char * channel_name, const Ref<MConfig::Config> & config,
                              Timers * timers, RecpathConfig * recpathConfig)
{
    if(!uri || !uri[0])
    {
        logD(stream, _func_, "ffmpegStreamData Init failed");
        return Result::Failure;
    }

    if(std::string(uri).find("rtsp") != 0)
    {
        logE(stream, _func_, "ffmpegStreamData Init failed, uri is not rtsp");
        return Result::Failure;
    }

    Deinit();

    // Open video file
    logD(stream, _func_, "uri: ", uri);
    Result res = Result::Success;

    format_ctx = avformat_alloc_context();

    format_ctx->interrupt_callback.callback = interrupt_cb;
    format_ctx->interrupt_callback.opaque = &m_tcFFTimeout;

    m_tcFFTimeout.Start();
    if(avformat_open_input(&format_ctx, uri, NULL, NULL) == 0)
    {
        logD(stream, _func_, "avformat_open_input, success");
        // Retrieve stream information

        g_mutexFFmpeg.lock();

        m_tcFFTimeout.Start();
        if(avformat_find_stream_info(format_ctx, NULL) >= 0)
        {
            logD(stream, _func_,"avformat_find_stream_info, success");

            // Dump information about file onto standard error
            av_dump_format(format_ctx, 0, uri, 0);
        }
        else
        {
            logE_(_func_, "Couldn't find stream information");
            res = Result::Failure;
        }
        g_mutexFFmpeg.unlock();
    }
    else
    {
        logE_(_func_, "Couldn't open uri [", uri, "]");
        res = Result::Failure;
    }

    if(res == Result::Success)
    {
        m_sourceInfo.sourceName = channel_name;
        m_sourceInfo.uri = uri;

        // Find the first video and audio stream
        for(int i = 0; i < format_ctx->nb_streams; i++)
        {
            stStreamInfo si;
            const char *codec_type = NULL;
            const char *codec_name = NULL;
            const char *profile = NULL;
            const AVCodec *p;
            codec_type = av_get_media_type_string(format_ctx->streams[i]->codec->codec_type);
            codec_name = avcodec_get_name(format_ctx->streams[i]->codec->codec_id);
            if (format_ctx->streams[i]->codec->profile != FF_PROFILE_UNKNOWN)
            {
                if (format_ctx->streams[i]->codec->codec)
                    p = format_ctx->streams[i]->codec->codec;
                else
                    p = 0 ? avcodec_find_encoder(format_ctx->streams[i]->codec->codec_id) :
                                avcodec_find_decoder(format_ctx->streams[i]->codec->codec_id);
                if (p)
                    profile = av_get_profile_name(p, format_ctx->streams[i]->codec->profile);
            }
            si.streamType = codec_type? codec_type: "UNKNOWN";
            si.codecName = codec_name ? codec_name: "UNKNOWN";
            si.profile = profile ? profile: "UNKNOWN";

            if(video_stream_idx == -1 && format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                if(format_ctx->streams[i]->codec->codec_id == AV_CODEC_ID_H264)
                    video_stream_idx = i;
                else
                    logE_(_func_, "video stream is not h264");

                stVideoStream vs;
                vs.streamInfo = si;

                double dFps = 0.;
                if(format_ctx->streams[i]->avg_frame_rate.den && format_ctx->streams[i]->avg_frame_rate.num)
                {
                    dFps = av_q2d(format_ctx->streams[i]->avg_frame_rate);
                    vs.fps = dFps;
                }
                vs.width = format_ctx->streams[i]->codec->width;
                vs.height = format_ctx->streams[i]->codec->height;

                m_sourceInfo.videoStreams.push_back(vs);
            }
            else if(audio_stream_idx == -1 && format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                audio_stream_idx = i;

                stAudioStream as;
                as.streamInfo = si;

                if(format_ctx->streams[i]->codec->sample_rate)
                {
                    as.sampleRate = format_ctx->streams[i]->codec->sample_rate;
                }
                m_sourceInfo.audioStreams.push_back(as);
            }
            else
            {
                stOtherStream os;
                os.streamInfo = si;
                m_sourceInfo.otherStreams.push_back(os);
            }
        }

        if(video_stream_idx == -1)
        {
            logE_(_func_,"didn't find a video stream");
            res = Result::Failure;
        }

        if(format_ctx)
        {
            m_vecPts.resize(format_ctx->nb_streams, Int64_Min);
            m_vecDts.resize(format_ctx->nb_streams, Int64_Min);
        }

        if(m_absf_ctx == NULL)
        {
            m_absf_ctx = av_bitstream_filter_init("aac_adtstoasc");
            if (!m_absf_ctx)
            {
                logE_(_func_, "No available 'aac_adtstoasc' bitstream filter");
            }
        }
    }

    // reading configs

    m_pRecpathConfig = recpathConfig;
    ConstMemory record_dir;
    ConstMemory confd_dir;
    Uint64 max_age_minutes = MAX_AGE;
    m_file_duration_sec = FILE_DURATION;

    // writing on/off
    {
        ConstMemory const opt_name = "mod_nvr/enable";
        StRef<String> str = st_makeString(config->getString (opt_name));
        std::string strRecEnable = std::string(str->cstr());
        if(strRecEnable.compare("y") == 0 || strRecEnable.compare("yes") == 0)
            m_bRecordingEnable = true;
    }

    // get cycle time
    {
        ConstMemory const opt_name = "mod_nvr/max_age";
        MConfig::GetResult const res =
                config->getUint64_default (opt_name, &max_age_minutes, max_age_minutes);
        if (!res)
        {
            logE_ (_func, "Invalid value for config option ", opt_name, ": ", config->getString (opt_name));
            m_bRecordingEnable = false;
        }
        else
            logD (stream, _func_, opt_name, ": ", max_age_minutes);
    }

    // get duration for each recorded file
    {
        ConstMemory const opt_name = "mod_nvr/file_duration";
        MConfig::GetResult const res =
                config->getUint64_default (opt_name, &m_file_duration_sec, m_file_duration_sec);
        if (!res)
        {
            logE_ (_func, "Invalid value for config option ", opt_name, ": ", config->getString (opt_name));
            m_bRecordingEnable = false;
        }
        else
            logD(stream, _func_, opt_name, ": ", m_file_duration_sec);
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
            m_bRecordingEnable = false;
        }
        logD(stream, _func_, opt_name, ": [", confd_dir, "]");
    }

    if(m_pRecpathConfig == 0 || !m_pRecpathConfig->IsInit())
        m_bRecordingEnable = false;

    if(m_bRecordingEnable)
    {
        if(m_pRecpathConfig->IsEmpty())
        {
            m_recordDir = st_makeString("");
        }
        else
        {
            m_recordDir = st_makeString(m_pRecpathConfig->GetNextPath().c_str());
        }

        m_channelName = st_makeString(channel_name);
        m_nvr_cleaner = grab (new (std::nothrow) NvrCleaner);
        m_nvr_cleaner->init (timers, m_pRecpathConfig, ConstMemory(channel_name, strlen(channel_name)), max_age_minutes * 60, 5);

        bool bDisableRecord = false;
        StRef<String> st_confd_dir = st_makeString(confd_dir);
        StRef<String> channelFullPath = st_makeString(st_confd_dir, "/", channel_name);
        std::string line;
        std::ifstream channelPath (channelFullPath->cstr());
        logD(stream, _func_, "channelFullPath = ", channelFullPath);
        if (channelPath.is_open())
        {
            logD(stream, _func_, "channelFullPath opened");
            while ( std::getline (channelPath, line) )
            {
                logD(stream, _func_, "got line: [", line.c_str(), "]");
                if(line.find("disable_record") == 0)
                {
                    std::string strLower = line;
                    std::transform(strLower.begin(), strLower.end(), strLower.begin(), ::tolower);
                    if(strLower.find("true") != std::string::npos)
                    {
                        logD(stream, _func_, "DISABLE RECORDING");
                        bDisableRecord = true;
                    }
                }
            }
            channelPath.close();
        }

        m_bRecordingState = ! bDisableRecord;
    }

    if(res == Result::Failure)
    {
        Deinit();
    }

    return res;
}

bool ffmpegStreamData::PushMediaPacket(FFmpegStream * pParent)
{
    if(!format_ctx || video_stream_idx == -1)
    {
        logD(stream, _func_, "PushMediaPacket failed, video_stream_idx = [", video_stream_idx, "]");
        return false;
    }

    AVPacket packet = {};
    bool media_found = false;

    while(1)
    {
#ifdef LIBMARY_PERFORMANCE_TESTING
		TimeChecker* tcInOut = dynamic_cast<TimeChecker*>(pParent->getTimeChecker());
        tcInOut->Start();
#else
		TimeChecker tcInOut;tcInOut.Start();
#endif
        TimeChecker tcInNvr;tcInNvr.Start();

        TimeChecker tc;tc.Start();
        int res = 0;

        m_tcFFTimeout.Start();
        if((res = av_read_frame(format_ctx, &packet)) < 0)
        {
            logD(frames, _func_, "av_read_frame failed, res = [", res, "]");
            break;
        }

        Time t;tc.Stop(&t);
        logD(frames, _func_, m_channelName, " av_read_frame exectime = [", t, "]");

        if(!m_bGotFirstFrame)
        {
            if (packet.flags & AV_PKT_FLAG_KEY)
            {
                logD(frames, _func_, "got key frame");
                m_bGotFirstFrame = true;
            }
            else
            {
                logD(frames, _func_, "skip non-key frame");
                av_free_packet(&packet);
                memset(&packet, 0, sizeof(packet));
                continue;
            }
        }

        if(m_vecPts[packet.stream_index] == Int64_Min || m_vecPts[packet.stream_index] > packet.pts)
        {
            if(packet.pts != AV_NOPTS_VALUE)
                m_vecPts[packet.stream_index] = packet.pts;
        }

        if(m_vecDts[packet.stream_index] == Int64_Min || m_vecDts[packet.stream_index] > packet.dts)
        {
            if(packet.dts != AV_NOPTS_VALUE)
                m_vecDts[packet.stream_index] = packet.dts;
        }

        if(m_vecPts[packet.stream_index] > m_vecDts[packet.stream_index])
            m_vecDts[packet.stream_index] = m_vecPts[packet.stream_index];        // to prevent situation when result packet.pts is less than packet.dts

        if(packet.stream_index == video_stream_idx)
        {
            logD(frames, _func_, "PACKET IS VIDEO");
        }
        else if(packet.stream_index == audio_stream_idx)
        {
            logD(frames, _func_, "PACKET IS AUDIO");
            if( format_ctx->streams[packet.stream_index]->codec->codec_id == AV_CODEC_ID_AAC &&
                packet.size > 2 &&
                (AV_RB16(packet.data) & 0xfff0) == 0xfff0 )
            {
                if(m_absf_ctx)
                {
                    logD(frames, _func_, "filtering aac packet");
                    AVPacket new_inpkt = packet;
                    int ret = av_bitstream_filter_filter(m_absf_ctx,
                                               format_ctx->streams[audio_stream_idx]->codec,
                                               NULL,
                                               &new_inpkt.data, &new_inpkt.size,
                                               packet.data, packet.size,
                                               packet.flags & AV_PKT_FLAG_KEY);
                    // create a padded packet
                    if (ret == 0 && new_inpkt.data != packet.data && new_inpkt.destruct)
                    {
                        // the new buffer should be a subset of the old so cannot overflow
                        uint8_t *t = (uint8_t *)av_malloc(new_inpkt.size + FF_INPUT_BUFFER_PADDING_SIZE);
                        if (!t)
                        {
                            logE_(_func_, "fail to allocate memory");
                        }
                        else
                        {
                            memcpy(t, new_inpkt.data, new_inpkt.size);
                            memset(t + new_inpkt.size, 0, FF_INPUT_BUFFER_PADDING_SIZE);
                            new_inpkt.data = t;
                            new_inpkt.buf = NULL;
                            ret = 1;
                        }
                    }

                    if (ret > 0)
                    {
                        new_inpkt.buf = av_buffer_create(new_inpkt.data, new_inpkt.size,
                                              av_buffer_default_free, NULL, 0);
                        if (!new_inpkt.buf)
                        {
                            logE_(_func_, "fail to av_buffer_create");
                        }
                        else
                        {
                            av_free_packet(&packet);
                            packet = new_inpkt;
                        }
                    }
                }
                else
                {
                    logE_(_func_, "Malformed stream and invalid bitstream filter");
                }
            }
        }
        else
        {
            logD(frames, _func_, "PACKET IS NEITHER");
        }

        logD(frames, _func_, m_channelName, " ORIGIN PACKET,indx=", packet.stream_index,
                            ",pts=", packet.pts,
                            ",dts=", packet.dts,
                            ",size=", packet.size,
                            ",flags=", packet.flags);
        logD(frames, _func_, m_channelName, " m_vecPts[packet.stream_index] = ", m_vecPts[packet.stream_index],
                ", m_vecPts[packet.stream_index] = ", m_vecDts[packet.stream_index]);

        if(packet.pts != AV_NOPTS_VALUE)
        {
            packet.pts -= m_vecPts[packet.stream_index];
        }

        if(packet.dts != AV_NOPTS_VALUE)
        {
            packet.dts -= m_vecDts[packet.stream_index];
        }

        logD(frames, _func_, m_channelName, " PACKET,indx=", packet.stream_index,
                            ",pts=", packet.pts,
                            ",dts=", packet.dts,
                            ",size=", packet.size,
                            ",flags=", packet.flags);

        // Is this a packet from the video or audio stream?
        if(packet.stream_index == video_stream_idx || packet.stream_index == audio_stream_idx)
        {
            TimeChecker tc;tc.Start();
            // write to moment video or audio stream
            if(packet.stream_index == video_stream_idx)
                pParent->doVideoData(packet, *(format_ctx->streams[packet.stream_index]));
            else
                pParent->doAudioData(packet, *(format_ctx->streams[packet.stream_index]));

            Time t;tc.Stop(&t);
            logD(frames, _func_, "FFmpegStream.",
                  (packet.stream_index == video_stream_idx) ? "doVideoData" : "doAudioData", " exectime = [", t, "]");
#ifdef LIBMARY_PERFORMANCE_TESTING
            tcInOut->SetPacketSize(packet.size);
#else
            Time tInOut;tcInOut.Stop(&tInOut);
            pParent->m_statMeasurer.AddTimeInOut(tInOut);
#endif

            media_found = true;
        }

        // write to file
        bool bRecordingSuccess = false;
        if(m_bRecordingEnable)
        {
            if(m_bRecordingState)
            {
                while(true) // check paths until we got successful writing of packet OR we checked all paths
                {
                    bool bNeedNextPath = false;
                    // prepare to write in file
                    if(!m_nvrData.IsInit())
                    {
                        if(m_recordDir == NULL || m_recordDir->len() == 0)
                        {
                            bNeedNextPath = true;
                            logD(frames,_func_, "m_recordDir is 0");
                        }
                        else
                        {
                            StRef<String> filepath = st_makeString (m_recordDir, "/", m_channelName, ".flv");
                            int nvrResult = m_nvrData.Init(format_ctx, m_channelName->cstr(), filepath->cstr(), "segment2", m_file_duration_sec);
                            if(nvrResult == 1)
                                bNeedNextPath = true;
                            logD(frames,_func_, "m_nvrData.Init = ", (nvrResult == 0) ? "Success" : "Failed");
                        }
                    }

                    // write in file
                    if(m_nvrData.IsInit())
                    {
                        TimeChecker tc;tc.Start();

                        int res = m_nvrData.WritePacket(format_ctx, packet);
                        if(res == ERR_NOSPACE)
                        {
                            bNeedNextPath = true;
                        }
                        else
                        {
                            bRecordingSuccess = true;
                        }

                        logD(frames, _func_, "NvrData.WritePacket res = ", res);

                        Time t;tc.Stop(&t);
                        logD(frames, _func_, "NvrData.WritePacket exectime = [", t, "]");

                        Time tInNvr;tcInNvr.Stop(&tInNvr);
                        pParent->m_statMeasurer.AddTimeInNvr(tInNvr);
                    }
                    if(!m_pRecpathConfig->IsPathExist(m_recordDir->cstr()))
                    {
                        bNeedNextPath = true;
                    }
                    if(bNeedNextPath)
                    {
                        if(!m_pRecpathConfig->IsEmpty())
                            m_recordDir = st_makeString(m_pRecpathConfig->GetNextPath(m_recordDir->cstr()).c_str());
                        else
                            m_recordDir = st_makeString("");
                        logD(frames,_func_, "next m_recordDir = [", m_recordDir, "]");

                        m_nvrData.Deinit();
                    }

                    // if packet is successfully written OR we checked all paths and all of them have not free space
                    if(bRecordingSuccess || m_recordDir == NULL || m_recordDir->len() == 0)
                    {
                        break;
                    }
                }
            }
            else
            {
                if(m_nvrData.IsInit())
                    m_nvrData.Deinit();
            }
        }
        m_bIsRecording = bRecordingSuccess;

        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);

        if(media_found)
        {
            break;
        }

        memset(&packet, 0, sizeof(packet));
    }

    return media_found;
}

int ffmpegStreamData::SetRecordingState(bool const bState)
{
    m_bRecordingState = bState;
    return 0;
}

int ffmpegStreamData::GetRecordingState(bool & bState)
{
    bState = m_bRecordingState;
    return 0;
}

bool ffmpegStreamData::IsRecording()
{
    return m_bIsRecording;
}

stSourceInfo ffmpegStreamData::GetSourceInfo()
{
    return m_sourceInfo;
}

nvrData::nvrData(void)
{
    m_file_duration_sec = 0;

    m_out_format_ctx = NULL;

    m_bIsInit = false;
}

nvrData::~nvrData(void)
{
    Deinit();
}

int nvrData::Init(AVFormatContext * format_ctx, const char * channel_name, const char * filepath,
                        const char * segMuxer, Uint64 file_duration_sec)
{
    if(!format_ctx || !filepath)
    {
        logE_ (_func, "fail nvrData::Init");
        logE_ (_func, "filepath=", filepath);
        return -1;
    }

    if(!MemoryDispatcher::Instance().Init())
    {
        logE_ (_func, "fail MemoryDispatcher::Init");
        return -1;
    }

    if(format_ctx->nb_streams <= 0)
    {
        logE_ (_func, "input number of streams = 0!");
        return -1;
    }

    // set internal members
    m_file_duration_sec = file_duration_sec;

    StRef<String> temp_channel_name = st_makeString(channel_name);
    m_channelName = st_grab (new (std::nothrow) String);
    m_channelName->set(temp_channel_name->mem());

    StRef<String> temp_filepath = st_makeString(filepath);
    m_filepath = st_grab (new (std::nothrow) String);
    m_filepath->set(temp_filepath->mem());

    StRef<String> temp_segMuxer = st_makeString(segMuxer);
    m_segMuxer = st_grab (new (std::nothrow) String);
    m_segMuxer->set(temp_segMuxer->mem());

    logD(stream, _func, "filepath: ", m_filepath);
    logD(stream, _func, "segMuxer: ", !m_segMuxer->isNullString() ? m_segMuxer->cstr() : "NONE");
    logD(stream, _func, "m_file_duration_sec: ", m_file_duration_sec);

    logD(stream, _func, "The output path: ", m_filepath);
    int res = 0;

    if(avformat_alloc_output_context2(&m_out_format_ctx, NULL, !m_segMuxer->isNullString() ? m_segMuxer->cstr() : NULL, m_filepath->cstr()) >= 0)
    {
        for (int i = 0; i < format_ctx->nb_streams; ++i)
        {
            AVStream * pStream = NULL;

            if (!(pStream = avformat_new_stream(m_out_format_ctx, NULL)))
            {
                logE_(_func_, "avformat_new_stream failed!");
                res = -1;
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
                res = -1;
                break;
            }
        }
    }
    else
    {
        logE_(_func_, "avformat_alloc_output_context2 failed!");
        res = -1;
    }

    if(res == 0)
    {
        logD(stream, _func, "check m_out_format_ctx");

        av_dump_format(m_out_format_ctx, 0, m_filepath->cstr(), 1);

        AVOutputFormat * fmt = m_out_format_ctx->oformat;

        if (!(fmt->flags & AVFMT_NOFILE))
        {
            int rres;
            // we need open file here
            if ((rres = avio_open(&m_out_format_ctx->pb, m_filepath->cstr(), AVIO_FLAG_WRITE)) < 0)
            {
                logE_(_func_, "fail to avio_open ", m_filepath);
                res = -1;
            }
        }
    }

    if(res == 0)
    {
        if(this->WriteHeader() != 0) // -22 - error of non existing path
        {
            res = 1;
        }
    }

    if(res == 0)
    {
        m_bIsInit = true;
    }
    else
    {
        logE_ (_func, "init failed");
        Deinit();
    }

    return res;
}

void nvrData::Deinit()
{
    logD(stream, _func_, "Deinit");

    if(m_out_format_ctx)
    {
        ffmpegStreamData::CloseCodecs(m_out_format_ctx);

        int ret = avio_close(m_out_format_ctx->pb);

        avformat_free_context(m_out_format_ctx);

        m_out_format_ctx = NULL;
    }

    m_bIsInit = false;

    logD(stream, _func_, "Deinit succeed");
}

bool nvrData::IsInit() const
{
    return m_bIsInit;
}

int nvrData::WriteHeader()
{
    logD(stream, _func, "set options");

    AVDictionary * pOptions = NULL;

    int ret = 0;
    if(!m_segMuxer->isNullString())
    {
        av_dict_set(&pOptions, "segment_time", st_makeString(m_file_duration_sec)->cstr(), 0);
        ret = avformat_write_header(m_out_format_ctx, &pOptions);
    }
    else
    {
        ret = avformat_write_header(m_out_format_ctx, NULL);
    }

    av_dict_free(&pOptions);

    if (ret < 0)
        logE_(_func_, "Error occurred when opening output file, ret = ", ret);

    return ret;
}

int nvrData::WritePacket(const AVFormatContext * inCtx, /*const*/ AVPacket & packet)
{
    if(!IsInit())
    {
        logE_(_func_, "fail, nvrdata IsInit is false");
        return -1;
    }

    AVPacket opkt;
    av_init_packet(&opkt);

    if (packet.pts != AV_NOPTS_VALUE)
    {
        opkt.pts = av_rescale_q(packet.pts, inCtx->streams[packet.stream_index]->time_base, m_out_format_ctx->streams[packet.stream_index]->time_base);
    }
    else
    {
        opkt.pts = AV_NOPTS_VALUE;
    }

    if (packet.dts != AV_NOPTS_VALUE)
    {
        opkt.dts = av_rescale_q(packet.dts, inCtx->streams[packet.stream_index]->time_base, m_out_format_ctx->streams[packet.stream_index]->time_base);
    }
    else
    {
        opkt.dts = AV_NOPTS_VALUE;
    }

    opkt.duration = av_rescale_q(packet.duration, inCtx->streams[packet.stream_index]->time_base, m_out_format_ctx->streams[packet.stream_index]->time_base);
    opkt.flags    = packet.flags;
    opkt.data = packet.data;
    opkt.size = packet.size;
    opkt.stream_index = packet.stream_index;

    logD(frames, _func_, m_channelName, " opkt,indx=", opkt.stream_index,
                        ",pts=", opkt.pts,
                        ",dts=", opkt.dts,
                        ",size=", opkt.size,
                        ",flags=", opkt.flags);

    if(m_out_format_ctx->streams[packet.stream_index]->pts.den == 0)
    {
        logD(frames, _func_, "stream's den is 0, skipping packet");
        return 0;
    }

    int nres = av_write_frame(m_out_format_ctx, &opkt);

    return nres;
}

int FFmpegStream::SetRecordingState(bool const bState)
{
    return m_ffmpegStreamData.SetRecordingState(bState);
}

int FFmpegStream::GetRecordingState(bool & bState)
{
    return m_ffmpegStreamData.GetRecordingState(bState);
}

bool FFmpegStream::IsRecording()
{
    return m_ffmpegStreamData.IsRecording();
}

bool FFmpegStream::IsRestreaming()
{
    return m_bIsRestreaming;
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

    return size;
}



int FFmpegStream::IsomWriteAvcc(ConstMemory const memory, MemoryEx & memoryOut)
{
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
            //logD(frames, _func, "AVC header, ONLY WriteDataInternal, ", data[0], ", ", data[1], ", ", data[2], ", ", data[3], ", ");
            WriteDataToBuffer(memory, memoryOut);
        }
    }

    return Result::Success;
}

void
FFmpegStream::reportStatusEvents ()
{
    logD(pipeline, _func_, "task is reported");
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

    logD(pipeline, _func_);

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
                logD(pipeline, _func_, "stream is closed");
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

    logD(stream, _func_);

    self->beginPushPacket();

    logD(stream, _func_, "End of ffmpeg_thread");
}

void
FFmpegStream::beginPushPacket()
{
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

    while(m_bIsRestreaming)
    {
        if(!m_ffmpegStreamData.PushMediaPacket(this))
        {
            logD(stream, _func_, "EOS");

            m_bIsRestreaming = false;

            eos_pending = true;

            reportStatusEvents ();
        }
    }
}

void
FFmpegStream::createSmartPipelineForUri ()
{
    logD(pipeline, _func_);
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

    logD(pipeline, _func, "uri: ", playback_item->stream_spec);

    while(m_ffmpegStreamData.Init( playback_item->stream_spec->cstr(),
            channel_opts->channel_name->cstr(), this->config, this->timers, this->m_pRecpathConfig) != Result::Success)
    {
        m_ffmpegStreamData.Deinit();
        logD(pipeline, _func_, "wait for 1 sec and init ffmpegStreamData again");
        sleep(1);
        if(m_bReleaseCalled)
        {
            mutex.lock ();
            goto _failure;
        }
    }

    ffmpeg_thread =
            grab (new (std::nothrow) Thread (CbDesc<Thread::ThreadFunc> (ffmpegThreadFunc,
                                                                         this,
                                                                         this)));
    if (!ffmpeg_thread->spawn (true /* joinable */))
        logE_ (_func, "Failed to spawn ffmpeg thread: ", exc->toString());

    m_bIsRestreaming = true;

    logD(pipeline, _func, "all ok");
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

    logD(pipeline, _func_);

    m_bIsRestreaming = false;

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
    logD(pipeline, _func_, "released, m_bIsRestreaming is false");
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

    m_bReleaseCalled = true;

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
FFmpegStream::doVideoData (AVPacket & packet, const AVStream & stream)
{
    AVCodecContext * pctx = stream.codec;
    int time_base_den = stream.time_base.den;

    updateTime ();

//    mutex.lock ();

    last_frame_time = getTime ();

    if (first_video_frame)
    {
        logD(frames, _func_, "it is first video frame");
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
                logD(frames, _func, "new codec data");
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
                logD(frames,_func, "Firing video message (AVC sequence header):");
                logLock ();
                PagePool::dumpPages (logs, &page_list);
                logUnlock ();
            }

            video_stream->fireVideoMessage (&msg);

            page_pool->msgUnref (page_list.first);
        } // if (report_avc_codec_data)
    } // if (is_h264_stream)
    else
    {
        logE_(_func_, "it is_not h264_stream");
        return;
    }

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
        logD(frames, _func_, "IT IS KEY FRAME\n");
        msg.frame_type = VideoStream::VideoFrameType::KeyFrame;
    }

    MemorySafe allocMemory(packet.size * 2);
    MemoryEx localMemory((Byte *)allocMemory.cstr(), allocMemory.len());
    int sizee = 0;
    if( (sizee = AvcParseNalUnits(ConstMemory(packet.data, packet.size), &localMemory)) < 0 )
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
    logD(frames, _func, "fireVideoMessage succeed, msg.msg_len=", msg.msg_len, ", msg.timestamp_nanosec=", msg.timestamp_nanosec);

    page_pool->msgUnref (page_list.first);

}

void FFmpegStream::doAudioData(AVPacket & packet, const AVStream & stream)
{
    AVCodecContext * pctx = stream.codec;
    int time_base_den = stream.time_base.den;

    updateTime();

    last_frame_time = getTime ();
    logD (frames, _func, "last_frame_time: 0x", fmt_hex, last_frame_time);

    VideoStream::AudioFrameType codec_data_type = VideoStream::AudioFrameType::Unknown;

    if(first_audio_frame)
    {
        if(pctx->codec_id == AV_CODEC_ID_MP3)
        {
            if(pctx->sample_rate == 8000)
                audio_codec_id = VideoStream::AudioCodecId::MP3_8kHz;
            else
                audio_codec_id = VideoStream::AudioCodecId::MP3;
        }
        else if(pctx->codec_id == AV_CODEC_ID_AAC)
        {
            audio_codec_id = VideoStream::AudioCodecId::AAC;

            if(pctx->extradata && pctx->extradata_size > 0)
            {
                codec_data_type = VideoStream::AudioFrameType::AacSequenceHeader;
            }
        }
        else if(pctx->codec_id == AV_CODEC_ID_PCM_U8)
        {
            audio_codec_id = VideoStream::AudioCodecId::LinearPcmPlatformEndian;
        }
        else if(pctx->codec_id == AV_CODEC_ID_ADPCM_SWF)
        {
            audio_codec_id = VideoStream::AudioCodecId::ADPCM;
        }
        else if(pctx->codec_id == AV_CODEC_ID_PCM_S16LE)
        {
            audio_codec_id = VideoStream::AudioCodecId::LinearPcmLittleEndian;
        }
        else if(pctx->codec_id == AV_CODEC_ID_NELLYMOSER)
        {
            if(pctx->channels == 1)
            {
                // mono
                if(pctx->sample_rate == 8000)
                    audio_codec_id = VideoStream::AudioCodecId::Nellymoser_8kHz_mono;
                else if(pctx->sample_rate == 16000)
                    audio_codec_id = VideoStream::AudioCodecId::Nellymoser_16kHz_mono;
            }

            if(audio_codec_id == VideoStream::AudioCodecId::Unknown)
                audio_codec_id = VideoStream::AudioCodecId::Nellymoser;
        }
        else if(pctx->codec_id == AV_CODEC_ID_PCM_ALAW)
        {
            audio_codec_id = VideoStream::AudioCodecId::G711ALaw;
        }
        else if(pctx->codec_id == AV_CODEC_ID_PCM_MULAW)
        {
            audio_codec_id = VideoStream::AudioCodecId::G711MuLaw;
        }
        else if(pctx->codec_id == AV_CODEC_ID_SPEEX)
        {
            audio_codec_id = VideoStream::AudioCodecId::Speex;

            if(pctx->extradata && pctx->extradata_size > 0)
            {
                codec_data_type = VideoStream::AudioFrameType::SpeexHeader;
            }
        }

        if(audio_codec_id != VideoStream::AudioCodecId::Unknown)
        {
            metadata.audio_sample_rate = (Uint32)pctx->sample_rate;
            metadata.got_flags |= RtmpServer::MetaData::AudioSampleRate;

            metadata.audio_sample_size = av_get_bytes_per_sample(pctx->sample_fmt)<<3;
            metadata.got_flags |= RtmpServer::MetaData::AudioSampleSize;

            metadata.num_channels = (Uint32)pctx->channels;
            metadata.got_flags |= RtmpServer::MetaData::NumChannels;

            audio_rate = pctx->sample_rate;
            audio_channels = pctx->channels;

            logD(frames, _func, "Audio Params, codec(", audio_codec_id, ") Frq(", metadata.audio_sample_rate,
                   ") SampleSizeInBits(", metadata.audio_sample_size, ") Channels(", metadata.num_channels, ")");

            if(playback_item->send_metadata)
            {
                if(!got_video || !first_video_frame)
                {
                    // There's no video or we've got the first video frame already.
                    reportMetaData ();
                    metadata_reported_cond.signal ();
                }
                else
                {
                    // Waiting for the first video frame.
                    while (got_video && first_video_frame)
                    {
                        // TODO FIXME This is a perfect way to hang up a thread.
                        metadata_reported_cond.wait (mutex);
                    }
                }
            }
        }

        first_audio_frame = false;
    }

    if(audio_codec_id == VideoStream::AudioCodecId::Unknown)
    {
        // TODO: need to support this codec, now we skip audio packets
        return;
    }

    bool skip_frame = false;
    {
        if(audio_skip_counter > 0)
        {
            --audio_skip_counter;
            logD (frames, _func, "Skipping audio frame, audio_skip_counter: ", audio_skip_counter, " left");
            skip_frame = true;
        }

        if(!initial_seek_complete)
        {
            // We have not started playing yet. This is most likely a preroll frame.
            logD (frames, _func, "Skipping an early preroll frame");
            skip_frame = true;
        }
    }

    if(codec_data_type != VideoStream::AudioFrameType::Unknown)
    {
        // Reporting codec data if needed.
        Size msg_len = 0;

        Uint64 const cd_timestamp_nanosec = 0;

        if(logLevelOn (frames, LogLevel::D))
        {
            logLock ();
            logD_unlocked_ (_func, "CODEC DATA");
            hexdump (logs, ConstMemory(pctx->extradata, pctx->extradata_size));
            logUnlock ();
        }

        PagePool::PageListHead page_list;

        if(playback_item->enable_prechunking)
        {
            Size msg_audio_hdr_len = 1;

            if(codec_data_type == VideoStream::AudioFrameType::AacSequenceHeader)
                msg_audio_hdr_len = 2;

            RtmpConnection::PrechunkContext prechunk_ctx (msg_audio_hdr_len);   // initial_offset
            RtmpConnection::fillPrechunkedPages (&prechunk_ctx,
                                                 ConstMemory (pctx->extradata, pctx->extradata_size),
                                                 page_pool,
                                                 &page_list,
                                                 RtmpConnection::DefaultAudioChunkStreamId,
                                                 cd_timestamp_nanosec,
                                                 false );   // first_chunk
        }
        else
        {
            page_pool->getFillPages (&page_list,
                                     ConstMemory (pctx->extradata, pctx->extradata_size));
        }

        msg_len += pctx->extradata_size;

        VideoStream::AudioMessage msg;
        msg.timestamp_nanosec = cd_timestamp_nanosec;
        msg.prechunk_size = (playback_item->enable_prechunking ? RtmpConnection::PrechunkSize : 0);
        msg.frame_type = codec_data_type;
        msg.codec_id = audio_codec_id;
        msg.rate = audio_rate;
        msg.channels = audio_channels;

        msg.page_pool = page_pool;
        msg.page_list = page_list;
        msg.msg_len = msg_len;
        msg.msg_offset = 0;

        video_stream->fireAudioMessage (&msg);

        page_pool->msgUnref (page_list.first);
    }

    if(skip_frame)
        return;

    {
        Size msg_len = 0;
        Uint64 const timestamp_nanosec = ((packet.pts == AV_NOPTS_VALUE) ? 0.0 : (double)packet.pts) / (double)time_base_den * 1000000000LL;
        PagePool::PageListHead page_list;

        Byte *buffer_data = packet.data;
        Size  buffer_size = packet.size;

        if(playback_item->enable_prechunking)
        {
            Size gen_audio_hdr_len = 1;

            if(audio_codec_id == VideoStream::AudioCodecId::AAC)
                gen_audio_hdr_len = 2;

            RtmpConnection::PrechunkContext prechunk_ctx (gen_audio_hdr_len);   // initial_offset
            RtmpConnection::fillPrechunkedPages (&prechunk_ctx,
                                               ConstMemory (buffer_data, buffer_size),
                                               page_pool,
                                               &page_list,
                                               RtmpConnection::DefaultAudioChunkStreamId,
                                               timestamp_nanosec / 1000000,
                                               false ); // first_chunk
        }
        else
        {
            page_pool->getFillPages (&page_list, ConstMemory (buffer_data, buffer_size));
        }

        msg_len += buffer_size;

        VideoStream::AudioMessage msg;
        msg.timestamp_nanosec = timestamp_nanosec;
        msg.prechunk_size = (playback_item->enable_prechunking ? RtmpConnection::PrechunkSize : 0);
        msg.frame_type = VideoStream::AudioFrameType::RawData;
        msg.codec_id = audio_codec_id;

        msg.page_pool = page_pool;
        msg.page_list = page_list;
        msg.msg_len = msg_len;
        msg.msg_offset = 0;
        msg.rate = audio_rate;
        msg.channels = audio_channels;

        video_stream->fireAudioMessage (&msg);

        page_pool->msgUnref (page_list.first);
    }
}

VideoStream::AudioCodecId FFmpegStream::GetAudioCodecId(const AVCodecContext * pctx)
{
    switch(pctx->codec_id)
    {
        case AV_CODEC_ID_PCM_U8:        return VideoStream::AudioCodecId::LinearPcmPlatformEndian;
        case AV_CODEC_ID_ADPCM_SWF:		return VideoStream::AudioCodecId::ADPCM;
        case AV_CODEC_ID_PCM_S16LE:		return VideoStream::AudioCodecId::LinearPcmLittleEndian;
        case AV_CODEC_ID_NELLYMOSER:
        {
            switch(pctx->sample_rate)
            {
                case 8000:              return VideoStream::AudioCodecId::Nellymoser_8kHz_mono;
                case 16000:             return VideoStream::AudioCodecId::Nellymoser_16kHz_mono;
            }

            return VideoStream::AudioCodecId::Nellymoser;
        }
        case AV_CODEC_ID_PCM_ALAW:      return VideoStream::AudioCodecId::G711ALaw;
        case AV_CODEC_ID_PCM_MULAW:     return VideoStream::AudioCodecId::G711MuLaw;
        case AV_CODEC_ID_AAC:           return VideoStream::AudioCodecId::AAC;
        case AV_CODEC_ID_SPEEX:         return VideoStream::AudioCodecId::Speex;
        case AV_CODEC_ID_MP3:
        {
            if(pctx->sample_rate <= 8000)
                return VideoStream::AudioCodecId::MP3_8kHz;

            return VideoStream::AudioCodecId::MP3;
        }
        default: break;
    }

    return VideoStream::AudioCodecId::Unknown;
}

VideoStream::AudioCodecId FFmpegStream::GetAudioCodecId(AVCodecID codec_id)
{
    switch(codec_id)
    {
        case AV_CODEC_ID_PCM_U8:        return VideoStream::AudioCodecId::LinearPcmPlatformEndian;
        case AV_CODEC_ID_ADPCM_SWF:		return VideoStream::AudioCodecId::ADPCM;
        case AV_CODEC_ID_PCM_S16LE:		return VideoStream::AudioCodecId::LinearPcmLittleEndian;
        case AV_CODEC_ID_NELLYMOSER:    return VideoStream::AudioCodecId::Nellymoser;
        case AV_CODEC_ID_PCM_ALAW:      return VideoStream::AudioCodecId::G711ALaw;
        case AV_CODEC_ID_PCM_MULAW:     return VideoStream::AudioCodecId::G711MuLaw;
        case AV_CODEC_ID_AAC:           return VideoStream::AudioCodecId::AAC;
        case AV_CODEC_ID_SPEEX:         return VideoStream::AudioCodecId::Speex;
        case AV_CODEC_ID_MP3:           return VideoStream::AudioCodecId::MP3;
        default: break; // need to support new type
    }

    return VideoStream::AudioCodecId::Unknown;
}

VideoStream::VideoCodecId FFmpegStream::GetVideoCodecId(AVCodecID codec_id)
{
    switch(codec_id)
    {
        case AV_CODEC_ID_FLV1:      return VideoStream::VideoCodecId::SorensonH263;
        case AV_CODEC_ID_H263:		return VideoStream::VideoCodecId::SorensonH263; // need to add new type
        case AV_CODEC_ID_FLASHSV:   return VideoStream::VideoCodecId::ScreenVideo;
        case AV_CODEC_ID_FLASHSV2:  return VideoStream::VideoCodecId::ScreenVideoV2;
        case AV_CODEC_ID_VP6F:      return VideoStream::VideoCodecId::VP6;
        case AV_CODEC_ID_VP6A:      return VideoStream::VideoCodecId::VP6Alpha;
        case AV_CODEC_ID_H264:      return VideoStream::VideoCodecId::AVC;
        case AV_CODEC_ID_MPEG4: // need to support new type
        default: break; // need to support new type
    }

    return VideoStream::VideoCodecId::Unknown;
}

void
FFmpegStream::noVideoTimerTick (void * const _self)
{
    FFmpegStream * const self = static_cast <FFmpegStream*> (_self);

    Time const time = getTime();

    self->mutex.lock ();

    logD(timer, _func_, "time: 0x", fmt_hex, time, ", last_frame_time: 0x", self->last_frame_time);

    if (self->stream_closed) {
        self->mutex.unlock ();
        return;
    }

    if (time > self->last_frame_time &&
    time - self->last_frame_time >= 15 /* TODO Config param for the timeout */)
    {
        logD (timer, _func, "firing \"no video\" event");

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
        logD(pipeline, _func, "place for createPipelineForChainSpec()");
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
    logD (bus, _func_);

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
            logD (bus, _func, "close_notified");

            mutex.unlock ();
            return;
        }

        if (eos_pending) {
            logD (bus, _func, "eos_pending");
            eos_pending = false;
            close_notified = true;

            if (frontend) {
            logD (bus, _func, "firing EOS");
            mt_unlocks_locks (mutex) frontend.call_mutex (frontend->eos, mutex);
            }

            break;
        }

        if (error_pending) {
            logD (bus, _func, "error_pending");
            error_pending = false;
            close_notified = true;

            if (frontend) {
            logD (bus, _func, "firing ERROR");
            mt_unlocks_locks (mutex) frontend.call_mutex (frontend->error, mutex);
            }

            break;
        }

        if (no_video_pending) {
                got_video_pending = false;

            logD (bus, _func, "no_video_pending");
            no_video_pending = false;
            if (stream_closed) {
            mutex.unlock ();
            return;
            }

            if (frontend) {
            logD (bus, _func, "firing NO_VIDEO");
            mt_unlocks_locks (mutex) frontend.call_mutex (frontend->noVideo, mutex);
            }
        }

        if (got_video_pending) {
            logD (bus, _func, "got_video_pending");
            got_video_pending = false;
            if (stream_closed) {
            mutex.unlock ();
            return;
            }

            if (frontend)
            mt_unlocks_locks (mutex) frontend.call_mutex (frontend->gotVideo, mutex);
        }
    }

    logD (bus, _func, "done");
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
#ifdef LIBMARY_PERFORMANCE_TESTING
IStatMeasurer*
FFmpegStream::getStatMeasurer() {
    return static_cast<IStatMeasurer*>(&m_statMeasurer);
}

ITimeChecker*
FFmpegStream::getTimeChecker() {
    return static_cast<ITimeChecker*>(&m_timeChecker);
}
#endif

StatMeasure
FFmpegStream::GetStatMeasure()
{
    return m_statMeasurer.GetStatMeasure();
}

Ref<ChannelChecker>
FFmpegStream::GetChannelChecker()
{
    return m_channel_checker;
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
                 MConfig::Config   * const config,
                 RecpathConfig     * const recpathConfig,
                 ChannelChecker    * const channel_checker)
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
    this->m_pRecpathConfig = recpathConfig;
    this->m_channel_checker = channel_checker;

    std::string channel_name = this->channel_opts->channel_name->cstr();

    this->m_statMeasurer.Init(this->timers);

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

bool
FFmpegStream::IsClosed()
{
    return stream_closed;
}

stSourceInfo
FFmpegStream::GetSourceInfo()
{
    stSourceInfo si = m_ffmpegStreamData.GetSourceInfo();
    si.title = std::string(channel_opts->channel_title->cstr());
    return si;
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
      m_bIsRestreaming(false),
      m_bReleaseCalled(false),
      m_pRecpathConfig(NULL)
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

