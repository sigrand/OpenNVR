
#ifndef __MOMENT_FFMPEG__FFMPEG__SUPPORT__H__
#define __MOMENT_FFMPEG__FFMPEG__SUPPORT__H__

#include <libmary/libmary.h>

// no necessary to show ffmpeg headers for another sources, so only declare:
struct AVFormatContext;

namespace MomentFFmpeg
{

namespace ffmpegHelper
{

void RegisterFFMpeg(void);

void CloseCodecs(AVFormatContext * pAVFrmtCntxt);

}   // namespace ffmpegHelper

}   // namespace MomentFFmpeg

#endif /* __MOMENT_FFMPEG__FFMPEG__SUPPORT__H__ */

