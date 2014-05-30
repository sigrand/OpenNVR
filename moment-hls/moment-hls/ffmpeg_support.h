
#ifndef __MOMENT_HLS__FFMPEG__SUPPORT__H__
#define __MOMENT_HLS__FFMPEG__SUPPORT__H__

#include <libmary/libmary.h>

// no necessary to show ffmpeg headers for another sources, so only declare:
struct AVFormatContext;

namespace MomentHls
{

namespace ffmpegHelper
{

void RegisterFFMpeg(void);

void CloseCodecs(AVFormatContext * pAVFrmtCntxt);

}   // namespace ffmpegHelper

}   // namespace MomentHls

#endif /* __MOMENT_HLS__FFMPEG__SUPPORT__H__ */

