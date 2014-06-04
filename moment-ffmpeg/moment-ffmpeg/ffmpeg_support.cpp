

#include "moment-ffmpeg/ffmpeg_support.h"

extern "C"
{
    #ifndef INT64_C
        #define INT64_C(c) (c ## LL)
        #define UINT64_C(c) (c ## ULL)
    #endif

    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavformat/url.h>
    #include <libswscale/swscale.h>

    extern AVOutputFormat ff_segment2_muxer;
    extern AVOutputFormat ff_stream_segment2_muxer;

	extern URLProtocol ff_buffer_protocol;
}


namespace MomentFFmpeg
{

using namespace M;
	
static LogGroup libMary_logGroup_fflog   ("mod_ffmpeg.fflog",   LogLevel::E);

#define ENABLE_FFMPEG_LOG

namespace ffmpegHelper
{

extern "C"
{
	static void CustomAVLogCallback(void * /*ptr*/, int /*level*/, const char * outFmt, va_list vl)
	{
#ifdef ENABLE_FFMPEG_LOG
		if(outFmt)
		{
			char chLogBuffer[2048] = {};
			vsnprintf(chLogBuffer, sizeof(chLogBuffer) - 1, outFmt, vl);
			//printf(chLogBuffer);
            logD(fflog, _func_, "ffmpeg LOG: ", chLogBuffer);
		}
#endif
	}
}   // extern "C" ]

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

void RegisterFFMpeg(void)
{
    logD(fflog, _func_, "RegisterFFMpeg");

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
    ffurl_register_protocol(&ff_buffer_protocol, sizeof(ff_buffer_protocol));
    av_lockmgr_register(ff_lockmgr);

    logD(fflog, _func_, "RegisterFFMpeg succeed");
}

void CloseCodecs(AVFormatContext * pAVFrmtCntxt)
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

}   // namespace ffmpegHelper

}   // namespace MomentFFmpeg

