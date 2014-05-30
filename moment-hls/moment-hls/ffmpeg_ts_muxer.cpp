

#include "ffmpeg_ts_muxer.h"
#include "ffmpeg_support.h"
#include "ffmpeg_pipeline_protocol.h"
#include "ffmpeg_h264_mp4toannexb_bsf.h"
#include <algorithm>
#include <sstream>

extern "C"
{
    #ifndef INT64_C
        #define INT64_C(c) (c ## LL)
        #define UINT64_C(c) (c ## ULL)
    #endif

    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>

	//extern int ff_avc_write_annexb_extradata(const uint8_t *in, uint8_t **buf, int *size);
}


namespace MomentHls
{

static LogGroup libMary_logGroup_hls_tsmux   ("mod_hls.tsmux",   LogLevel::D);

static const size_t VIDEO_ALLOC_PADDING_SIZE = 1024;
static const Uint64 ONE_SEC_NANOSEC = 1000000000ULL;


Uint64 CTsMuxer::g_NextId = 0;
std::map<Uint64, CTsMuxer *> CTsMuxer::g_MuxersMap;

CTsMuxer::CTsMuxer()
	: m_Id(g_NextId++)
	, m_pOutFormatCtx(NULL)
	, m_iSegmentDuration(-1)
	, m_iVideoIndex(-1)
	, m_pBSFH264(NULL)
	, m_uiiFirstTime(Uint64_Max)
	, m_uiiPreviousTime(Uint64_Max)
	, m_uiMaxSegmentSizeInBytes(static_cast<size_t>(-1))
{
	g_MuxersMap[m_Id] = this;
    logD(hls_tsmux, _func_, "start, this = ", (ptrdiff_t)this, ", mapsize = ", g_MuxersMap.size());
    ffmpegHelper::RegisterFFMpeg();
}

CTsMuxer::~CTsMuxer()
{
	g_MuxersMap.erase(m_Id);
	logD(hls_tsmux, _func_, "start, this = ", (ptrdiff_t)this);
	Deinit();
	logD(hls_tsmux, _func_, "end, this = ", (ptrdiff_t)this);
}

void CTsMuxer::Deinit()
{
	InternalDeinit();

	m_iSegmentDuration = -1;
}

void CTsMuxer::InternalDeinit( void )
{
	if(m_pOutFormatCtx)
	{
		if(m_iVideoIndex >= 0)
		{
			AVCodecContext * pCodecContext = m_pOutFormatCtx->streams[m_iVideoIndex]->codec;

			if(pCodecContext->extradata)
			{
				// we do it, because we allocated it using ff_avc_write_annexb_extradata/av_malloc function.
				av_free(pCodecContext->extradata);
				pCodecContext->extradata = NULL;
				pCodecContext->extradata_size = 0;
			}
		}

		ffmpegHelper::CloseCodecs(m_pOutFormatCtx);

		avio_close(m_pOutFormatCtx->pb);

		avformat_free_context(m_pOutFormatCtx);

		m_pOutFormatCtx = NULL;
	}

	if(m_pBSFH264)
	{
		av_bitstream_filter_close(m_pBSFH264);
		m_pBSFH264 = NULL;
	}

	m_MuxerData.Deinit();
	m_PagesMemory.clear();
	m_iVideoIndex = -1;
	m_uiiPreviousTime = m_uiiFirstTime = Uint64_Max;
}

bool CTsMuxer::Init(int segmentDuration, size_t uiMaxSegmentSizeInBytes)
{
    logD(hls_tsmux, _func_, "start, segment duration = ", segmentDuration);
    Deinit();

	if(segmentDuration <= 0 || uiMaxSegmentSizeInBytes < 32 * 1024)
	{
		logE(hls_tsmux, _func_, "invalid arg, segment duration = ", segmentDuration);
		return false;
	}

	m_iSegmentDuration = segmentDuration;
	m_uiMaxSegmentSizeInBytes = uiMaxSegmentSizeInBytes;
    logD(hls_tsmux, _func_, "end");
    return true;
}

bool CTsMuxer::InternalInit(VideoStream::VideoCodecId videoCodecId, Int32 width, Int32 height, Int32 frame_rate_num, Int32 frame_rate_den)
{
	logD(hls_tsmux, _func_, "start, videoCodecId = ", videoCodecId, ", width = ", width, ", height = ", height,
		", frameRate_num = ", frame_rate_num, ", frameRate_den = ", frame_rate_den);

	if(videoCodecId != VideoStream::VideoCodecId::AVC)
	{
		logE(hls_tsmux, _func_, "msg->codec_id is not AVC, codec_id = ", videoCodecId);
		logE(hls_tsmux, _func_, "HLS module supports only H264 (AVC)");
		return false;
	}

	assert(m_iSegmentDuration != -1);
	if(m_iSegmentDuration == -1)
	{
		logE(hls_tsmux, _func_, "(m_iSegmentDuration == -1), very bad situation, check it");
		return false;
	}

	assert(m_pOutFormatCtx == NULL);
	if(m_pOutFormatCtx)
	{
		logE(hls_tsmux, _func_, "m_pOutFormatCtx != NULL, very bad situation, check it");
		return false;
	}

	if(width <= 0 || height <= 0 || frame_rate_num <= 0 || frame_rate_den <= 0)
	{
		logE(hls_tsmux, _func_, "invalid args, width = ", width, ", height = ", height,
			", frameRate_num = ", frame_rate_num, ", frameRate_den = ", frame_rate_den);
		return false;
	}

// 	if(codecId != AV_CODEC_ID_H264)
// 	{
// 		logE(hls_tsmux, _func_, "codecId(", codecId, ") != AV_CODEC_ID_H264(", static_cast<Int32>(AV_CODEC_ID_H264), ")");
// 		return false;
// 	}

	CAutoInternalDeinit autoDeinit(this);

	const AVCodecID codecId = AV_CODEC_ID_H264;
	static const std::string protocolName = CPipelineProtocol::GetProtocolName();
	std::stringstream bufferForName;

	// make file name e.g.: protocolname:/222_%d.ts
	bufferForName << protocolName << ":/" << m_Id << "_%d.ts";

	const std::string & fullName = bufferForName.str();

	int ffRes = avformat_alloc_output_context2(&m_pOutFormatCtx, NULL, "segment", fullName.c_str());
	logD(hls_tsmux, _func_, "szFilePath = ", fullName.c_str(), ", m_pOutFormatCtx->filename = ", m_pOutFormatCtx->filename);
    if(ffRes < 0 || !m_pOutFormatCtx)
    {
        logE(hls_tsmux, _func_, "avformat_alloc_output_context2 fails = ", ffRes);
        return false;
	}
	logD(hls_tsmux, _func_, "format name = ", m_pOutFormatCtx->oformat->name, ", long name = ", m_pOutFormatCtx->oformat->long_name);
	// initialize video stream
	{
		AVStream * pStream = avformat_new_stream(m_pOutFormatCtx, NULL);

		if(!pStream)
		{
			logE(hls_tsmux, _func_, "avformat_new_stream failed!");
			return false;
		}

		AVCodecContext * pCodecContext = pStream->codec;

		avcodec_get_context_defaults3( pCodecContext, NULL );

		pCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
		pCodecContext->codec_id = codecId;
		pCodecContext->bit_rate = 400000;	// random value
		pCodecContext->width = width;
		pCodecContext->height = height;
		pCodecContext->time_base.num = frame_rate_num;
		pCodecContext->time_base.den = frame_rate_den;

		if(m_pOutFormatCtx->oformat->flags & AVFMT_GLOBALHEADER)
		{
			pCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}

		// set to stream the same time base
		pStream->time_base = pCodecContext->time_base;

		// fill index
		m_iVideoIndex = pStream->index;
	}

	if(!(m_pOutFormatCtx->oformat->flags & AVFMT_NOFILE))
	{
		assert(0);
		logD(hls_tsmux, _func_, "m_pOutFormatCtx has no AVFMT_NOFILE flag");

		if(avio_open(&m_pOutFormatCtx->pb, fullName.c_str(), AVIO_FLAG_WRITE) < 0)
		{
			logE(hls_tsmux, _func_, "avio_open failed!");
			return false;
		}
	}
	else
	{
		logD(hls_tsmux, _func_, "m_pOutFormatCtx has AVFMT_NOFILE flag");
	}

	// write header
	{
		StRef<String> segmentDurationStr = st_makeString(m_iSegmentDuration);
		AVDictionary * pOptions = NULL;

		//av_dict_set(&pOptions, "segment_list_type", "m3u8", 0);
		av_dict_set(&pOptions, "segment_time", segmentDurationStr->cstr(), 0);
		av_dict_set(&pOptions, "segment_format", "mpegts", 0);
		logD(hls_tsmux, _func_, "Segment Duration = ", segmentDurationStr->cstr());
		int ffRes = avformat_write_header(m_pOutFormatCtx, &pOptions);

		av_dict_free(&pOptions);

		if(ffRes < 0)
		{
			logE(hls_tsmux, _func_, "avformat_write_header failed!");
			return false;
		}
	}

	const std::string & bsfName = GetOwnBitStreamFilterName();

	m_pBSFH264 = av_bitstream_filter_init(bsfName.c_str());	// we use own bsf to avoid many reallocation operations
	if (!m_pBSFH264)
	{
		logE_(_func_, "No available '", bsfName.c_str(), "' bitstream filter");
		return false;
	}

	if(!m_MuxerData.Init(0, m_uiMaxSegmentSizeInBytes))
	{
		logE_(_func_, "m_MuxerData.Init(0, ", m_uiMaxSegmentSizeInBytes, ") failed!");
		return false;
	}

	av_dump_format(m_pOutFormatCtx, 0, fullName.c_str(), 1);

	autoDeinit.InitSucceeded();

    return true;
}

Int64 CTsMuxer::PtsFromMsg(VideoStream::VideoMessage * mt_nonnull msg)
{
	const Uint64 timestamp_nanosec = msg->timestamp_nanosec;

	if(m_uiiFirstTime == Uint64_Max)
	{
		if(timestamp_nanosec > (Uint64_Max / 2))
		{
			logE(hls_tsmux, _func_, "first timestamp_nanosec(", timestamp_nanosec, ") is incorrect, PLEASE CHECK IT!");
			return AV_NOPTS_VALUE;
		}

		// first frame
		m_uiiPreviousTime = m_uiiFirstTime = timestamp_nanosec;
	}

	if(timestamp_nanosec < m_uiiFirstTime)
	{
		logE(hls_tsmux, _func_, "msg->timestamp_nanosec(", timestamp_nanosec, ") < m_uiiFirstTime(", m_uiiFirstTime, "), PLEASE CHECK IT!");
		return AV_NOPTS_VALUE;
	}

	if((timestamp_nanosec - m_uiiPreviousTime) > ONE_SEC_NANOSEC)
	{
		logE(hls_tsmux, _func_, "diff(", (timestamp_nanosec - m_uiiPreviousTime), ") > ONE_SEC_NANOSEC(", ONE_SEC_NANOSEC, "), PLEASE CHECK IT!");
		return AV_NOPTS_VALUE;
	}

	m_uiiPreviousTime = timestamp_nanosec;

	const Uint64 diff = timestamp_nanosec - m_uiiFirstTime;

	const Uint64 uiiFrameRateNum = (msg->frame_rate.num > 0) ? msg->frame_rate.num : m_pOutFormatCtx->streams[m_iVideoIndex]->time_base.num;
	const Uint64 uiFrameRateDen = (msg->frame_rate.den > 0) ? msg->frame_rate.den : m_pOutFormatCtx->streams[m_iVideoIndex]->time_base.den;

	Int64 outPts;

	if(uiiFrameRateNum == 1)
	{
		// only integer math
		outPts = static_cast<Int64>((diff * uiFrameRateDen) / ONE_SEC_NANOSEC);
	}
	else
	{
		const double dFrameRateDen = uiFrameRateDen / static_cast<double>(uiiFrameRateNum);
		outPts = static_cast<Int64>((diff * dFrameRateDen) / ONE_SEC_NANOSEC);
	}

	return outPts;
}

CTsMuxer::Result CTsMuxer::WriteVideoMessage(VideoStream::VideoMessage * mt_nonnull msg, ConstMemory & muxedData)
{
	assert(msg);
	if(!msg)
	{
		logE(hls_tsmux, _func_, "msg is NULL");
		return CTsMuxer::Fail;
	}

	if(msg->codec_id != VideoStream::VideoCodecId::AVC)
	{
		logE(hls_tsmux, _func_, "msg->codec_id is not AVC, codec_id = ", msg->codec_id);
		logE(hls_tsmux, _func_, "HLS module supports only H264 (AVC)");
		return CTsMuxer::Fail;
	}

	if(!IsInternalsInit())
	{
		if(!InternalInit(msg->codec_id, msg->width, msg->height, msg->frame_rate.num, msg->frame_rate.den))
		{
			logE(hls_tsmux, _func_, "InternalInit failed!");
			return CTsMuxer::Fail;
		}
	}

	Uint8 * pVideoBuffer;
	size_t uiBufferSize;

	Result bufRes = PrepareVideoMessage(msg, pVideoBuffer, uiBufferSize);
	if(bufRes != CTsMuxer::Success)
	{
		if(bufRes == CTsMuxer::Fail)
		{
			logE(hls_tsmux, _func_, "PrepareVideoMessage failed!");
		}

		return bufRes;
	}

	if(uiBufferSize == 0)
	{
		logE(hls_tsmux, _func_, "skip empty frame, size is 0");
		return CTsMuxer::NeedMoreData;
	}

	if(m_uiiFirstTime == Uint64_Max)
	{
		// check first frame:
		// this frame must be KeyFrame and has good time stamp
		if((msg->timestamp_nanosec > (Uint64_Max / 2)) || (msg->frame_type != VideoStream::VideoFrameType::KeyFrame))
		{
			logE(hls_tsmux, _func_, "SKIP FRAME: timestamp_nanosec(", msg->timestamp_nanosec, ") is incorrect and frame type [", msg->frame_type, "], PLEASE CHECK IT!");
			return CTsMuxer::NeedMoreData;
		}
	}

	AVPacket videoPacket;

	av_init_packet(&videoPacket); 

	videoPacket.pts = PtsFromMsg(msg);
	videoPacket.dts = AV_NOPTS_VALUE;
	videoPacket.duration = 0;
	videoPacket.convergence_duration = AV_NOPTS_VALUE; 

	if(msg->frame_type == VideoStream::VideoFrameType::KeyFrame)
	{
		videoPacket.flags |= AV_PKT_FLAG_KEY;
	}

	videoPacket.data = pVideoBuffer; 
	videoPacket.size = static_cast<int>(uiBufferSize); 
	videoPacket.stream_index = m_iVideoIndex;

	//logD(hls_tsmux, _func_, "Write frame,", (videoPacket.flags & AV_PKT_FLAG_KEY) ? " " : " no ", "key frame, ",
	//	"size=", uiBufferSize, ", timestamp_nanosec=", msg->timestamp_nanosec, ",pts=", videoPacket.pts, ", m_iVideoIndex = ", m_iVideoIndex);

	m_MuxerData.PrepareWriting();

	// we replaced the write module for segment muxer and av_interleaved_write_frame() will call CTsMuxer::WriteData function.
	if(av_interleaved_write_frame(m_pOutFormatCtx, &videoPacket) < 0)
	{
		logE(hls_tsmux, _func_, "av_interleaved_write_frame failed! we will restart TS muxer");
		InternalDeinit();
		return CTsMuxer::Fail;
	}

	//logD(hls_tsmux, _func_, "av_interleaved_write_frame success");
    return m_MuxerData.GetData(muxedData);
}

bool CTsMuxer::MovePagesToBuffer( const PagePool::PageListHead & pageList, Uint8 *& pOutputBuffer, size_t & uiOutputBufferSize, std::vector<Uint8> & buffer )
{
	// reset output parameters
	pOutputBuffer = NULL;
	uiOutputBufferSize = 0;

	Count bufCnt = 0;

	for(PagePool::Page * page = pageList.first; page; page = page->getNextMsgPage(), ++bufCnt)
	{
		uiOutputBufferSize += page->data_len;
	}

	if(uiOutputBufferSize == 0)
	{
		logE(hls_tsmux, _func_, "failed! uiBufferSize == 0");
		return false;
	}

	if(bufCnt == 1)
	{
		//logD(hls_tsmux, _func_, "pages cnt = 1, without copying, uiBufferSize = ", uiOutputBufferSize);
		// here we do not copy any memory
		pOutputBuffer = pageList.first->getData();
		uiOutputBufferSize = pageList.first->data_len;
		return true;
	}
	else
	{
		//logD(hls_tsmux, _func_, "pages cnt = ", bufCnt, ", uiBufferSize = ", uiOutputBufferSize, ", previous buffer.size = ", buffer.size());
	}

	if(!ReallocBuffer(buffer, uiOutputBufferSize, VIDEO_ALLOC_PADDING_SIZE))
	{
		uiOutputBufferSize = 0;	// JIC
		logE(hls_tsmux, _func_, "ReallocBuffer failed!");
		return false;
	}

	// copy memory from several pages to one array
	pOutputBuffer = &buffer[0];

	Uint8 * pBufDest = pOutputBuffer;

	for(PagePool::Page * page = pageList.first; page; page = page->getNextMsgPage())
	{
		if(page->data_len > 0)
		{
			memcpy(pBufDest, page->getData(), page->data_len);
			pBufDest += page->data_len;
		}
	}

	return true;
}

CTsMuxer::Result CTsMuxer::PrepareVideoMessage(VideoStream::VideoMessage * mt_nonnull msg, Uint8 *& pOutBuffer, size_t & uiOutBufferSize )
{
	Uint8 * pBuffer;
	size_t uiBufSize;

	if(!MovePagesToBuffer(msg->page_list, pBuffer, uiBufSize, m_PagesMemory))
	{
		logE(hls_tsmux, _func_, "MovePagesToBuffer for video failed!");
		return CTsMuxer::Fail;
	}

	AVCodecContext * pCodecContext = m_pOutFormatCtx->streams[m_iVideoIndex]->codec;

	if(!pCodecContext->extradata)
	{
		// we do not initialize encoder, because we have x264 stream already, and therefore we need to fill
		// extradata themselves, so h264_mp4toannexb filter does not work without valid extradata.
		logD(hls_tsmux, _func_, "pCodecContext->extradata  is NULL and frame is ", msg->frame_type);

		if(msg->frame_type != VideoStream::VideoFrameType::AvcSequenceHeader)
		{
			logE(hls_tsmux, _func_, "frame is not AvcSequenceHeader");
			return CTsMuxer::NeedMoreData;
		}
#if 0
		int newSize = uiBufSize;

		if(ff_avc_write_annexb_extradata(pVideoBuffer, &pCodecContext->extradata, &newSize) < 0)
		{
			logE(hls_tsmux, _func_, "ff_avc_write_annexb_extradata failed!");
			return CTsMuxer::Fail;
		}

		pCodecContext->extradata_size = newSize;
#endif
		pCodecContext->extradata = reinterpret_cast<uint8_t *>(av_malloc(uiBufSize));

		if(!pCodecContext->extradata)
		{
			logE(hls_tsmux, _func_, "out of memory");
			return CTsMuxer::Fail;
		}

		pCodecContext->extradata_size = uiBufSize;

		memcpy(pCodecContext->extradata, pBuffer, uiBufSize);
		logD(hls_tsmux, _func_, "pCodecContext->extradata_size = ", pCodecContext->extradata_size);
		// now we have extradata and ready to use h264_mp4toannexb bitstream filter
	}

	uint8_t * pTempOutBuf = pBuffer;
	int iOutBufSize = uiBufSize;

	int ffRes = av_bitstream_filter_filter(m_pBSFH264, pCodecContext, NULL, &pTempOutBuf, &iOutBufSize, pBuffer, uiBufSize,
		(msg->frame_type == VideoStream::VideoFrameType::KeyFrame));
	if(iOutBufSize <= 0)
	{
		if(msg->frame_type == VideoStream::VideoFrameType::AvcSequenceHeader)
		{
			return CTsMuxer::NeedMoreData;
		}

		logE(hls_tsmux, _func_, "av_bitstream_filter_filter failed! ffRes = ", ffRes, ", iOutBufSize = ", iOutBufSize);
		return CTsMuxer::Fail;
	}
	//logD(hls_tsmux, _func_, "av_bitstream_filter_filter, pSrc(", (ptrdiff_t)pBuffer, ")=?pDst(", (ptrdiff_t)pTempOutBuf,
	//	"), srcSize(", uiBufSize, ")=?dstSize(", iOutBufSize, ")");
	if(ffRes == 0 && pBuffer != pTempOutBuf)
	{
		// THIS WAY IS NOT TESTED, because it never called
		// pTempOutBuf is part of pVideoBuffer
		logD(hls_tsmux, _func_, "ffRes == 0 && pVideoBuffer != pTempOutBuf");
		if(pBuffer == &m_PagesMemory[0])
		{
			size_t uiFullSize = pTempOutBuf - pBuffer + iOutBufSize;	// uiFullSize must <= uiBufSize
			logD(hls_tsmux, _func_, "ffRes = 0, uiFullSize = ", uiFullSize, ", m_PagesMemory.size() = ",
				m_PagesMemory.size(), ", remainder = ", m_PagesMemory.size() - uiFullSize);
			assert(m_PagesMemory.size() > (uiFullSize + FF_INPUT_BUFFER_PADDING_SIZE) && uiFullSize <= uiBufSize);
			// pTempOutBuf is pointer on m_PagesMemory, we allocate m_PagesMemory always more on VIDEO_ALLOC_PADDING_SIZE bytes
			memset(pTempOutBuf + iOutBufSize, 0, std::min(static_cast<size_t>(FF_INPUT_BUFFER_PADDING_SIZE), m_PagesMemory.size() - uiFullSize));

			pOutBuffer = pTempOutBuf;
			uiOutBufferSize = iOutBufSize;
		}
		else
		{
			logD(hls_tsmux, _func_, "uiBufSize = ", uiBufSize, ", iOutBufSize = ", iOutBufSize, ", m_PagesMemory.size() = ",
				m_PagesMemory.size(), ", remainder = ", m_PagesMemory.size() - iOutBufSize);

			if(!ReallocBuffer(m_PagesMemory, std::max(uiBufSize, static_cast<size_t>(iOutBufSize)), VIDEO_ALLOC_PADDING_SIZE))
			{
				logE(hls_tsmux, _func_, "ReallocBuffer failed");
				return CTsMuxer::Fail;
			}

			pOutBuffer = &m_PagesMemory[0];
			uiOutBufferSize = iOutBufSize;

			memcpy(pOutBuffer, pTempOutBuf, iOutBufSize);
			memset(pOutBuffer + iOutBufSize, 0, std::min(static_cast<size_t>(FF_INPUT_BUFFER_PADDING_SIZE), m_PagesMemory.size() - iOutBufSize));
		}
	}
	else if(ffRes > 0)
	{
		//logD(hls_tsmux, _func_, "ffRes = ", ffRes, ", new iOutBufSize = ", iOutBufSize, ", previous uiBufSize = ",
		//	uiBufSize, ", m_PagesMemory.size() = ", m_PagesMemory.size());

		const size_t maxBufSize = std::max(uiBufSize, static_cast<size_t>(iOutBufSize));

		if(pBuffer != &m_PagesMemory[0] || maxBufSize > m_PagesMemory.size())
		{
			if(!ReallocBuffer(m_PagesMemory, maxBufSize, VIDEO_ALLOC_PADDING_SIZE))
			{
				logE(hls_tsmux, _func_, "ReallocBuffer failed");
				return CTsMuxer::Fail;
			}
		}

		pOutBuffer = &m_PagesMemory[0];
		uiOutBufferSize = iOutBufSize;

		memcpy(pOutBuffer, pTempOutBuf, iOutBufSize);

		// If we use standard 'h264_mp4toannexb', so we need to call av_free to free memory from bsf.
		// av_free(pTempOutBuf);
		// But we use 'own_h264_mp4toannexb' which manages a memory itself.
	}
	else if(ffRes < 0)
	{
		logE(hls_tsmux, _func_, "Failed h264_mp4toannexb bitstream filter, ffRes = ", ffRes);
		return CTsMuxer::Fail;
	}
	else
	{
		// ffRes == 0 && pVideoBuffer == pTempOutBuf
		logD(hls_tsmux, _func_, "bsf is not applied");
		pOutBuffer = pBuffer;
		uiOutBufferSize = uiBufSize;
	}

	return CTsMuxer::Success;
}

bool CTsMuxer::ReallocBuffer(std::vector<Uint8> & buffer, size_t newSize, size_t addAllocSize/* = 0*/)
{
	if(buffer.size() < newSize + addAllocSize)
	{
		size_t uiAllocSize = newSize + 2 * addAllocSize;	// + 2 * addAllocSize to avoid many reallocations
		// align 1024
		uiAllocSize = ((uiAllocSize + 1023)>>10)<<10;

		try
		{
			logD(hls_tsmux, _func_, "buffer was reallocated from = ", buffer.size(), " to ", uiAllocSize, ", newSize = ", newSize);
			buffer.resize(uiAllocSize);
		}
		catch(std::bad_alloc&) 	// catch bad_alloc
		{
			logE(hls_tsmux, _func_, "exception std::bad_alloc");
		}

		if(buffer.size() != uiAllocSize)
		{
			logE(hls_tsmux, _func_, "failed! out of memory");
			return false;
		}
	}

	return true;
}

bool CTsMuxer::GetTsSegmentInfo( const char * pszFileName, TsSegmentInfo & info )
{
	if(!pszFileName)
	{
		logE(hls_tsmux, _func_, "failed! pszFileName is NULL");
		return false;
	}

	const std::string fileName = pszFileName;

	// we parse the file name like : protocolname:/252_3.ts
	size_t backslashIdx = fileName.find('/');
	size_t dashIdx = fileName.find('_');
	size_t pointIdx = fileName.find('.');

	if( backslashIdx == std::string::npos || dashIdx == std::string::npos || pointIdx == std::string::npos ||
		backslashIdx >= dashIdx || dashIdx >= pointIdx)
	{
		logE(hls_tsmux, _func_, "bad file name, pszFileName = ", pszFileName);
		return false;
	}

	++backslashIdx;

	const std::string strTsMuxerId = fileName.substr(backslashIdx, dashIdx - backslashIdx);

	++dashIdx;

	const std::string strSegmentId = fileName.substr(dashIdx, pointIdx - dashIdx);

	Uint64 uiiMuxerId = 0, uiiSegmentId = 0;

	if(!strToUint64_safe(strTsMuxerId.c_str(), &uiiMuxerId))
	{
		logE(hls_tsmux, _func_, "strToUint64_safe failed, strTsMuxerId = ", strTsMuxerId.c_str());
		return false;
	}

	if(!strToUint64_safe(strSegmentId.c_str(), &uiiSegmentId))
	{
		logE(hls_tsmux, _func_, "strToUint64_safe failed, strSegmentId = ", strSegmentId.c_str());
		return false;
	}

	if(g_MuxersMap.find(uiiMuxerId) == g_MuxersMap.end())
	{
		logE(hls_tsmux, _func_, "map does not have following muxer id = ", uiiMuxerId);
		return false;
	}
	logD(hls_tsmux, _func_, "file name = ", pszFileName, ", MuxerId = ", uiiMuxerId, ", SegmentId = ", uiiSegmentId);
	CTsMuxer * pTsMuxer = g_MuxersMap[uiiMuxerId];
	info.owner = reinterpret_cast<UintPtr>(pTsMuxer);
	info.id = uiiSegmentId;

	return true;
}

bool CTsMuxer::WriteData( const TsSegmentInfo & info, const Uint8 * buf, size_t size )
{
	//logD(hls_tsmux, _func_, "owner = ", info.owner, ", id = ", info.id, ", size = ", size);
	CTsMuxer * pTsMuxer = reinterpret_cast<CTsMuxer *>(info.owner);

	return pTsMuxer->StoreData(info.id, buf, size);
}

bool CTsMuxer::StoreData( Uint64 curSegmId, const Uint8 * buf, size_t size )
{
	return m_MuxerData.StoreData(curSegmId, buf, size);
}

/************************************************************************/
/*               CTsMuxer::CMuxedData implementation                    */
/************************************************************************/

CTsMuxer::CMuxedData::CMuxedData( void )
	: m_uiActualSize(0)
	, m_uiSegmentSize(static_cast<size_t>(-1))
	, m_uiiCurrentSegmentId(0)
	, m_uiMaxSegmentSize(static_cast<size_t>(-1))
{
}

CTsMuxer::CMuxedData::~CMuxedData( void )
{
	Deinit();
}

void CTsMuxer::CMuxedData::Deinit( void )
{
	m_uiActualSize = 0;
	m_uiSegmentSize = static_cast<size_t>(-1);
	m_uiiCurrentSegmentId = 0;
	m_Data.clear();
}

bool CTsMuxer::CMuxedData::Init( Uint64 firstSegmentId, size_t uiMaxSegmentSize)
{
	Deinit();

	m_uiiCurrentSegmentId = firstSegmentId;
	m_uiMaxSegmentSize = uiMaxSegmentSize;

	return true;
}

void CTsMuxer::CMuxedData::PrepareWriting( void )
{
	if(m_uiSegmentSize == static_cast<size_t>(-1))
	{
		return;
	}

	assert(m_uiActualSize >= m_uiSegmentSize && m_Data.size() >= m_uiActualSize);

	const size_t uiRemainder = m_uiActualSize - m_uiSegmentSize;
	logD(hls_tsmux, _func_, "m_uiActualSize = ", m_uiActualSize, ", m_uiSegmentSize = ", m_uiSegmentSize,
		", remainder and new m_uiActualSize = ", uiRemainder, ", m_Data.size() = ", m_Data.size());
	if(uiRemainder > 0)
	{
		Uint8 * pbStartPtr = &m_Data[0];
		memcpy(pbStartPtr, pbStartPtr + m_uiSegmentSize, uiRemainder);
	}

	m_uiActualSize = uiRemainder;
	m_uiSegmentSize = static_cast<size_t>(-1);
}

CTsMuxer::Result CTsMuxer::CMuxedData::GetData( ConstMemory & muxedData )
{
	if(m_uiSegmentSize == static_cast<size_t>(-1))
	{
		return CTsMuxer::NeedMoreData;
	}

	assert(m_uiSegmentSize > 0);
	logD(hls_tsmux, _func_, "new segment, size = ", m_uiSegmentSize);
	muxedData = ConstMemory(&m_Data[0], m_uiSegmentSize);

	return CTsMuxer::Success;
}

bool CTsMuxer::CMuxedData::StoreData( Uint64 curSegmId, const Uint8 * buf, size_t size )
{
	const size_t uiNewActualSize = m_uiActualSize + size;

	if(uiNewActualSize > m_Data.size())
	{
		if(uiNewActualSize > m_uiMaxSegmentSize)
		{
			return false;
		}

		const size_t uiNewAllocSize = ((uiNewActualSize + 4 * 1024)>>10)<<10;	// align 1024
		logD(hls_tsmux, _func_, "realloc to new size = ", uiNewAllocSize);
		try
		{
			m_Data.resize(uiNewAllocSize);
		}
		catch(std::bad_alloc&) 	// catch bad_alloc
		{
			logE(hls_tsmux, _func_, "exception std::bad_alloc");
		}

		if(m_Data.size() != uiNewAllocSize)
		{
			logE(hls_tsmux, _func_, "failed! out of memory");
			return false;
		}
	}

	const size_t uiPreviousSize = m_uiActualSize;

	memcpy((&m_Data[0]) + m_uiActualSize, buf, size);

	m_uiActualSize = uiNewActualSize;

	if(m_uiiCurrentSegmentId != curSegmId)
	{
		assert(m_uiSegmentSize == static_cast<size_t>(-1));
		m_uiSegmentSize = uiPreviousSize;
		logD(hls_tsmux, _func_, "new segment, size = ", m_uiSegmentSize, ", newSegmId = ", curSegmId, ", oldSegmId = ", m_uiiCurrentSegmentId);
		m_uiiCurrentSegmentId = curSegmId;
	}

	return true;
}

}   // namespace MomentHls

