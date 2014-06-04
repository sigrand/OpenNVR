

#include "ffmpeg_support.h"

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

	extern URLProtocol ff_buffer_protocol;
	extern AVBitStreamFilter own_h264_mp4toannexb_bsf;
}


namespace MomentHls
{

using namespace M;
	
static LogGroup libMary_logGroup_hls_ffsup   ("mod_hls.ffsup",   LogLevel::E);

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
			logD(hls_ffsup, _func_, "ffmpeg LOG: ", chLogBuffer);
		}
#endif
	}
}   // extern "C" ]

void RegisterFFMpeg(void)
{
    static Uint32 uiInitialized = 0;

    if(uiInitialized != 0)
        return;

	logD(hls_ffsup, _func_, "RegisterFFMpeg, first call");
    uiInitialized = 1;

    av_log_set_callback(CustomAVLogCallback);

    // global ffmpeg initialization
    av_register_all();
    avformat_network_init();
	ffurl_register_protocol(&ff_buffer_protocol, sizeof(ff_buffer_protocol));
	av_register_bitstream_filter(&own_h264_mp4toannexb_bsf);
    logD(hls_ffsup, _func_, "RegisterFFMpeg end");
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

}   // namespace MomentHls

