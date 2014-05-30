
#ifndef __MOMENT_HLS__FFMPEG__H264__MP4TOANNEXB__BSF__H__
#define __MOMENT_HLS__FFMPEG__H264__MP4TOANNEXB__BSF__H__

#include <libmary/libmary.h>
#include <string>

namespace MomentHls
{

std::string GetOwnBitStreamFilterName(void) { return "own_h264_mp4toannexb"; }

}   // namespace MomentHls

#endif /* __MOMENT_HLS__FFMPEG__H264__MP4TOANNEXB__BSF__H__ */

