
#ifndef __MOMENT_FFMPEG__FFMPEG__BUFFER__PROTOCOL__H__
#define __MOMENT_FFMPEG__FFMPEG__BUFFER__PROTOCOL__H__

#include <libmary/libmary.h>
#include "moment-ffmpeg/video_part_maker.h"
#include <string>

struct URLContext;

namespace MomentFFmpeg
{

using namespace M;

class CPipelineProtocol
{

public:

	CPipelineProtocol();
	~CPipelineProtocol();

	bool Init(URLContext * pProtocol, const char * pFileName, int flags);

	bool Write(const unsigned char *buf, int size);

	static std::string GetProtocolName(void);

private /*functions*/:

	void Deinit();	// use just delete operator.

private /*variables*/:

    //CTsMuxer::TsSegmentInfo	m_Info;
    Uint64 m_Id;
};

}   // namespace MomentFFmpeg

#endif /* __MOMENT_FFMPEG__FFMPEG__BUFFER__PROTOCOL__H__ */

