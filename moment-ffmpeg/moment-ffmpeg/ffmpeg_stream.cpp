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

        logD_ (_func, "new fullfilename: ", fullPath, ", "
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
            logD_(_func_, "ffmpeg LOG: ", chLogBuffer);
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


StateMutex ffmpegStreamData::g_mutexFFmpeg;

static void RegisterFFMpeg(void)
{
    logD_(_func_, "RegisterFFMpeg");

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

    logD_(_func_, "RegisterFFMpeg succeed");
}


ffmpegStreamData::ffmpegStreamData(void)
{
    // Register all formats and codecs
    RegisterFFMpeg();

    format_ctx = NULL;
    m_absf_ctx = NULL;

    m_bRecordingState = false;
    m_bRecordingEnable = false;
    m_bGotFirstFrame = false;

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
    logD_(_func_, "ffmpegStreamData Deinit");
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
    m_bGotFirstFrame = false;

    audio_stream_idx = video_stream_idx = -1;
    m_file_duration_sec = -1;
}

Result ffmpegStreamData::Init(const char * uri, const char * channel_name, const Ref<MConfig::Config> & config, Timers * timers)
{
    if(!uri || !uri[0])
    {
        logD_(_func_, "ffmpegStreamData Init failed");
        return Result::Failure;
    }

    Deinit();

    // Open video file
    logD_(_func_, "uri: ", uri);
    Result res = Result::Success;

    format_ctx = avformat_alloc_context();

    format_ctx->interrupt_callback.callback = interrupt_cb;
    format_ctx->interrupt_callback.opaque = &m_tcFFTimeout;

    m_tcFFTimeout.Start();
    if(avformat_open_input(&format_ctx, uri, NULL, NULL) == 0)
    {
        logD_(_func_, "avformat_open_input, success");
        // Retrieve stream information

        g_mutexFFmpeg.lock();

        m_tcFFTimeout.Start();
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
        g_mutexFFmpeg.unlock();
    }
    else
    {
        logE_(_func_, "Couldn't open uri");
        res = Result::Failure;
    }

    if(res == Result::Success)
    {
        // Find the first video and audio stream
        for(int i = 0; i < format_ctx->nb_streams; i++)
        {
            if(video_stream_idx == -1 && format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                if(format_ctx->streams[i]->codec->codec_id == AV_CODEC_ID_H264)
                    video_stream_idx = i;
                else
                    logE_(_func_, "video stream is not h264");
            }
            else if(audio_stream_idx == -1 && format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                audio_stream_idx = i;
            }
        }

        if(video_stream_idx == -1)
        {
            logE_(_func_,"didn't find a video stream");
            res = Result::Failure;
        }
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

    // reading configs

    ConstMemory record_dir;
    ConstMemory confd_dir;
    Uint64 max_age_minutes = 120;
    m_file_duration_sec = 3600;

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
            res = Result::Failure;
        }
        logI_ (_func, opt_name, ": [", record_dir, "]");
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
            logI_ (_func, opt_name, ": ", m_file_duration_sec);
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
        logI_ (_func, opt_name, ": [", confd_dir, "]");
    }

    if(m_bRecordingEnable)
    {
        m_recordDir = st_makeString(record_dir);
        m_channelName = st_makeString(channel_name);
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
        logD_(_func_, "PushMediaPacket failed, video_stream_idx = [", video_stream_idx, "]");
        return false;
    }

    AVPacket packet = {};
    bool media_found = false;

    while(1)
    {
        TimeChecker tcInOut;tcInOut.Start();
        TimeChecker tcInNvr;tcInNvr.Start();

        TimeChecker tc;tc.Start();
        int res = 0;

        m_tcFFTimeout.Start();
        if((res = av_read_frame(format_ctx, &packet)) < 0)
        {
            logD_(_func_, "av_read_frame failed, res = [", res, "]");
            break;
        }

        Time t;tc.Stop(&t);
        logD_(_func_, "av_read_frame exectime = [", t, "]");

        if(!m_bGotFirstFrame)
        {
            if (packet.flags & AV_PKT_FLAG_KEY)
            {
                logD_(_func_, "got key frame");
                m_bGotFirstFrame = true;
            }
            else
            {
                logD_(_func_, "skip non-key frame");
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
            logD_(_func_, "PACKET IS VIDEO");
        }
        else if(packet.stream_index == audio_stream_idx)
        {
            logD_(_func_, "PACKET IS AUDIO");
        }
        else
        {
            logD_(_func_, "PACKET IS NEITHER");
        }

        logD_(_func_, "ORIGIN PACKET,indx=", packet.stream_index,
                            ",pts=", packet.pts,
                            ",dts=", packet.dts,
                            ",size=", packet.size,
                            ",flags=", packet.flags);
        logD_(_func_, "m_vecPts[packet.stream_index] = ", m_vecPts[packet.stream_index],
                ", m_vecPts[packet.stream_index] = ", m_vecDts[packet.stream_index]);

        if(packet.pts != AV_NOPTS_VALUE)
        {
            packet.pts -= m_vecPts[packet.stream_index];
        }

        if(packet.dts != AV_NOPTS_VALUE)
        {
            packet.dts -= m_vecDts[packet.stream_index];
        }

        logD_(_func_, "PACKET,indx=", packet.stream_index,
                            ",pts=", packet.pts,
                            ",dts=", packet.dts,
                            ",size=", packet.size,
                            ",flags=", packet.flags);


        int nSkipAudioIndex = -1; // default no skip audio
        // if aac is malformed then process it by bitstream aac filter
        if(packet.stream_index == audio_stream_idx &&
                format_ctx->streams[packet.stream_index]->codec->codec_id == AV_CODEC_ID_AAC &&
                packet.size > 2 && (AV_RB16(packet.data) & 0xfff0) == 0xfff0)
        {
            if(m_absf_ctx)
            {
                logD_(_func_, "filtering aac packet");
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
                        nSkipAudioIndex = audio_stream_idx;
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
                        nSkipAudioIndex = audio_stream_idx;
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
                nSkipAudioIndex = audio_stream_idx;
            }
        }

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
            logD_(_func_, "FFmpegStream.",
                  (packet.stream_index == video_stream_idx) ? "doVideoData" : "doAudioData", " exectime = [", t, "]");

            Time tInOut;tcInOut.Stop(&tInOut);
            pParent->m_statMeasurer.AddTimeInOut(tInOut);

            media_found = true;
        }

        // write to file
        if(m_bRecordingState && m_bRecordingEnable)
        {
            // prepare to write in file
            if(!m_nvrData.IsInit())
            {
                StRef<String> filepath = st_makeString (m_recordDir, "/", m_channelName, ".flv");
                Result nvrResult = m_nvrData.Init(format_ctx, m_channelName->cstr(), filepath->cstr(), "segment2", m_file_duration_sec, nSkipAudioIndex);
                logD_(_func_, "m_nvrData.Init = ", (nvrResult == Result::Success) ? "Success" : "Failed");
            }

            // write in file
            if(m_nvrData.IsInit())
            {
                TimeChecker tc;tc.Start();

                int res = 0;
                if(m_nvrData.IsInit())
                {
                    res = m_nvrData.WritePacket(format_ctx, packet);
                }
                else
                {
                    m_nvrData.Reinit(format_ctx);
                }

                Time t;tc.Stop(&t);
                logD_(_func_, "NvrData.WritePacket exectime = [", t, "]");

                Time tInNvr;tcInNvr.Stop(&tInNvr);
                pParent->m_statMeasurer.AddTimeInNvr(tInNvr);
            }
        }

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

int ffmpegStreamData::SetChannelState(bool const bState)
{
    m_bRecordingState = bState;
    return 0;
}

int ffmpegStreamData::GetChannelState(bool & bState)
{
    bState = m_bRecordingState;
    return 0;
}

nvrData::nvrData(void)
{
    m_file_duration_sec = 0;
    m_skipAudioIndex = -1;

    m_out_format_ctx = NULL;

    m_bIsInit = false;
    m_bWriteTrailer = false;
}

nvrData::~nvrData(void)
{
    Deinit();
}

Result nvrData::Init(AVFormatContext * format_ctx, const char * channel_name, const char * filepath,
                        const char * segMuxer, Uint64 file_duration_sec, int skipAudioIndex)
{
    if(!format_ctx || !filepath)
    {
        logE_ (_func, "fail nvrData::Init");
        logE_ (_func, "filepath=", filepath);
        return Result::Failure;
    }

    if(format_ctx->nb_streams <= 0)
    {
        logE_ (_func, "input number of streams = 0!");
        return Result::Failure;
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

    logD_ (_func, "filepath: ", m_filepath);
    logD_ (_func, "segMuxer: ", !m_segMuxer->isNullString() ? m_segMuxer->cstr() : "NONE");
    logD_ (_func, "m_file_duration_sec: ", m_file_duration_sec);

    logD_ (_func, "The output path: ", m_filepath);
    Result res = Result::Success;

    if(avformat_alloc_output_context2(&m_out_format_ctx, NULL, !m_segMuxer->isNullString() ? m_segMuxer->cstr() : NULL, m_filepath->cstr()) >= 0)
    {
        if(skipAudioIndex != -1)
            m_skipAudioIndex = skipAudioIndex;

        for (int i = 0; i < format_ctx->nb_streams; ++i)
        {
            if (m_skipAudioIndex != -1 && i == m_skipAudioIndex)
            {
                continue;
            }

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
        logD_ (_func, "check m_out_format_ctx");

        av_dump_format(m_out_format_ctx, 0, m_filepath->cstr(), 1);

        AVOutputFormat * fmt = m_out_format_ctx->oformat;

        if (!(fmt->flags & AVFMT_NOFILE))
        {
            int rres;
            // we need open file here
            if ((rres = avio_open(&m_out_format_ctx->pb, m_filepath->cstr(), AVIO_FLAG_WRITE)) < 0)
            {
                logE_(_func_, "fail to avio_open ", m_filepath);
                res = Result::Failure;
            }
        }
    }

    if(res == Result::Success)
    {
        logD_ (_func, "init succeed");
        m_bIsInit = true;
        this->WriteHeader();
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
    logD_(_func_, "Deinit");

    if(m_out_format_ctx)
    {
        if(m_bWriteTrailer)
        {
            av_write_trailer(m_out_format_ctx);
        }

        ffmpegStreamData::CloseCodecs(m_out_format_ctx);

        int ret = avio_close(m_out_format_ctx->pb);

        avformat_free_context(m_out_format_ctx);

        m_out_format_ctx = NULL;
    }

    m_bIsInit = false;
    m_bWriteTrailer = false;
    m_skipAudioIndex = -1;

    logD_(_func_, "Deinit succeed");
}

bool nvrData::IsInit() const
{
    return m_bIsInit;
}

Result nvrData::Reinit(AVFormatContext * format_ctx)
{
    Result res = Result::Success;

    Deinit();

    res = Init(format_ctx, m_channelName->cstr(), m_filepath->cstr(), m_segMuxer->cstr(), m_file_duration_sec, m_skipAudioIndex);

    return res;
}

int nvrData::WriteHeader()
{
    if(!m_bIsInit)
    {
        return -1;
    }

    logD_ (_func, "set options");

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
        logE_(_func_, "Error occurred when opening output file");
    else
        m_bWriteTrailer = true;

    return ret;
}

int nvrData::WritePacket(const AVFormatContext * inCtx, /*const*/ AVPacket & packet)
{
    if(!IsInit())
    {
        logE_(_func_, "fail, nvrdata IsInit is false");
        return -1;
    }

    if(m_skipAudioIndex == packet.stream_index)
        return 0;

//    AVCodecContext * pInpCodec = inCtx->streams[packet.stream_index]->codec;

//    logD_(_func_, "pInpCodec time_base.num: ", pInpCodec->time_base.num);
//    logD_(_func_, "pInpCodec time_base.den: ", pInpCodec->time_base.den);
//    logD_(_func_, "inCtx->streams[i]->time_base.num: ", inCtx->streams[packet.stream_index]->time_base.num);
//    logD_(_func_, "inCtx->streams[i]->time_base.den: ", inCtx->streams[packet.stream_index]->time_base.den);

//    AVCodecContext * pOutCodec = m_out_format_ctx->streams[packet.stream_index]->codec;

//    logD_(_func_, "pOutCodec time_base.num: ", pOutCodec->time_base.num);
//    logD_(_func_, "pOutCodec time_base.den: ", pOutCodec->time_base.den);
//    logD_(_func_, "m_out_format_ctx->streams[i]->time_base.num: ", m_out_format_ctx->streams[packet.stream_index]->time_base.num);
//    logD_(_func_, "m_out_format_ctx->streams[i]->time_base.den: ", m_out_format_ctx->streams[packet.stream_index]->time_base.den);

    AVPacket opkt;
    av_init_packet(&opkt);

    if (packet.pts != AV_NOPTS_VALUE)
    {
        opkt.pts = av_rescale_q(packet.pts, inCtx->streams[packet.stream_index]->time_base, m_out_format_ctx->streams[packet.stream_index]->time_base);
        //av_log(NULL, 0, "**********packet.pts != AV_NOPTS_VALUE\n");
    }
    else
    {
        opkt.pts = AV_NOPTS_VALUE;
        //av_log(NULL, 0, "**********packet.pts == AV_NOPTS_VALUE\n");
    }

    if (packet.dts != AV_NOPTS_VALUE)
    {
        opkt.dts = av_rescale_q(packet.dts, inCtx->streams[packet.stream_index]->time_base, m_out_format_ctx->streams[packet.stream_index]->time_base);
        //av_log(NULL, 0, "**********packet.dts != AV_NOPTS_VALUE\n");
    }
    else
    {
        opkt.dts = AV_NOPTS_VALUE;
        //av_log(NULL, 0, "**********packet.dts == AV_NOPTS_VALUE\n");
    }

    opkt.duration = av_rescale_q(packet.duration, inCtx->streams[packet.stream_index]->time_base, m_out_format_ctx->streams[packet.stream_index]->time_base);
    opkt.flags    = packet.flags;
    opkt.data = packet.data;
    opkt.size = packet.size;
    opkt.stream_index = packet.stream_index;

    if(m_out_format_ctx->streams[packet.stream_index]->pts.den == 0)
    {
        logD_(_func_, "stream's den is 0, skipping packet");
        return 0;
    }

    //int nres = av_interleaved_write_frame(m_out_format_ctx, &opkt); // may return -1094995529
    int nres = av_write_frame(m_out_format_ctx, &opkt);

    return nres;
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

    logD_(_func_);

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
                logD_(_func_, "stream is closed");
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
        if(!m_ffmpegStreamData.PushMediaPacket(this))
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
    logD_ (_func_);
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

//    if(m_ffmpegStreamData.Init( playback_item->stream_spec->cstr(), channel_opts->channel_name->cstr(), this->config, this->timers) != Result::Success)
//    {
//        mutex.lock ();
//        logE_ (_this_func, "stream is not initialized, uri \"", playback_item->stream_spec->cstr(), "\"");
//        goto _failure;
//    }

    while(m_ffmpegStreamData.Init( playback_item->stream_spec->cstr(), channel_opts->channel_name->cstr(), this->config, this->timers) != Result::Success)
    {
        m_ffmpegStreamData.Deinit();
        logD_(_func_, "wait for 1 sec and init ffmpegStreamData again");
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

    m_bIsPlaying = true;

    logD_ (_func, "all ok");
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
        logD_(_func_, "it is first video frame");
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
                logD_ (_func, "new codec data");
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
        logD_(_func_, "IT IS KEY FRAME\n");
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
    logD_ (_func, "fireVideoMessage succeed, msg.msg_len=", msg.msg_len, ", msg.timestamp_nanosec=", msg.timestamp_nanosec);

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

            logD_ (_func, "Audio Params, codec(", audio_codec_id, ") Frq(", metadata.audio_sample_rate,
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

StatMeasure
FFmpegStream::GetStatMeasure()
{
    return m_statMeasurer.GetStatMeasure();
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
      m_bIsPlaying(false),
      m_bReleaseCalled(false)
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

