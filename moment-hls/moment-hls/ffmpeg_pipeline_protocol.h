
#ifndef __MOMENT_HLS__FFMPEG__BUFFER__PROTOCOL__H__
#define __MOMENT_HLS__FFMPEG__BUFFER__PROTOCOL__H__

#include <libmary/libmary.h>
#include "ffmpeg_ts_muxer.h"
#include <string>

struct URLContext;

namespace MomentHls
{

using namespace M;
using namespace Moment;

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

	CTsMuxer::TsSegmentInfo	m_Info;
};

}   // namespace MomentHls

#endif /* __MOMENT_HLS__FFMPEG__BUFFER__PROTOCOL__H__ */

