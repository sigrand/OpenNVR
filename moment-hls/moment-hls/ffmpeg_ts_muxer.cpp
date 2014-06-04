

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
}


namespace MomentHls
{

static LogGroup libMary_logGroup_hls_tsmux   ("mod_hls.tsmux",   LogLevel::E);

static const size_t VIDEO_ALLOC_PADDING_SIZE = 1024;
static const Uint64 ONE_SEC_NANOSEC = 1000000000ULL;


Uint64 CTsMuxer::g_NextId = 0;
std::map<Uint64, CTsMuxer *> CTsMuxer::g_MuxersMap;

CTsMuxer::CTsMuxer()
	: m_Id(g_NextId)
	, m_pOutFormatCtx(NULL)
	, m_iSegmentDuration(-1)
	, m_iVideoIndex(-1)
	, m_pBSFH264(NULL)
	, m_uiiFirstTime(Uint64_Max)
	, m_uiiPreviousTime(Uint64_Max)
	, m_uiMaxSegmentSizeInBytes(static_cast<size_t>(-1))
	, m_MuxerData(g_NextId)
{
	++g_NextId;
	g_MuxersMap[m_Id] = this;
	logD(hls_tsmux, _func_, "start, this = ", (ptrdiff_t)this, ", mapsize = ", g_MuxersMap.size(), ",m_Id#", m_Id);
    ffmpegHelper::RegisterFFMpeg();
}

CTsMuxer::~CTsMuxer()
{
	logD(hls_tsmux, _func_, "start, this = ", (ptrdiff_t)this, ",m_Id#", m_Id);
	Deinit();
	logD(hls_tsmux, _func_, "end, this = ", (ptrdiff_t)this, ",m_Id#", m_Id);
	g_MuxersMap.erase(m_Id);
}

void CTsMuxer::Deinit()
{
	InternalDeinit();

	m_AVCHeaderData.clear();
	m_iSegmentDuration = -1;
}

void CTsMuxer::InternalDeinit( void )
{
	if(m_pOutFormatCtx)
	{
		logD(hls_tsmux, _func_, "start, m_pOutFormatCtx is not a NULL", ",m_Id#", m_Id);
		if(m_iVideoIndex >= 0)
		{
			logD(hls_tsmux, _func_, "m_iVideoIndex = ", m_iVideoIndex, ",m_Id#", m_Id);
			AVCodecContext * pCodecContext = m_pOutFormatCtx->streams[m_iVideoIndex]->codec;

			if(pCodecContext->extradata)
			{
				logD(hls_tsmux, _func_, "free pCodecContext->extradata", ",m_Id#", m_Id);
				// we do it, because we allocated it using av_malloc function.
				av_free(pCodecContext->extradata);
				pCodecContext->extradata = NULL;
				pCodecContext->extradata_size = 0;
				logD(hls_tsmux, _func_, "pCodecContext->extradata released", ",m_Id#", m_Id);
			}
		}

		logD(hls_tsmux, _func_, "before ffmpegHelper::CloseCodecs", ",m_Id#", m_Id);
		ffmpegHelper::CloseCodecs(m_pOutFormatCtx);
		logD(hls_tsmux, _func_, "after ffmpegHelper::CloseCodecs", ",m_Id#", m_Id);

		avio_close(m_pOutFormatCtx->pb);
		logD(hls_tsmux, _func_, "after avio_close", ",m_Id#", m_Id);

		avformat_free_context(m_pOutFormatCtx);
		logD(hls_tsmux, _func_, "after avformat_free_context", ",m_Id#", m_Id);

		m_pOutFormatCtx = NULL;
	}

	if(m_pBSFH264)
	{
		logD(hls_tsmux, _func_, "before av_bitstream_filter_close", ",m_Id#", m_Id);
		av_bitstream_filter_close(m_pBSFH264);
		m_pBSFH264 = NULL;
		logD(hls_tsmux, _func_, "after av_bitstream_filter_close", ",m_Id#", m_Id);
	}

	// here we do not clear m_AVCHeaderData, because AVC header is sent very rarely,
	// also the header data is not changed for stream

	m_MuxerData.Deinit();
	m_PagesMemory.clear();
	m_iVideoIndex = -1;
	m_uiiPreviousTime = m_uiiFirstTime = Uint64_Max;
	logD(hls_tsmux, _func_, "end", ",m_Id#", m_Id);
}

bool CTsMuxer::Init(int segmentDuration, size_t uiMaxSegmentSizeInBytes)
{
    logD(hls_tsmux, _func_, "start, segment duration = ", segmentDuration, ",m_Id#", m_Id);
    Deinit();

	if(segmentDuration <= 0 || uiMaxSegmentSizeInBytes < 32 * 1024)
	{
		logE(hls_tsmux, _func_, "invalid arg, segment duration = ", segmentDuration, ",m_Id#", m_Id);
		return false;
	}

	m_iSegmentDuration = segmentDuration;
	m_uiMaxSegmentSizeInBytes = uiMaxSegmentSizeInBytes;
    logD(hls_tsmux, _func_, "end", ",m_Id#", m_Id);
    return true;
}

bool CTsMuxer::InternalInit(VideoStream::VideoCodecId videoCodecId, Int32 width, Int32 height, Int32 frame_rate_num, Int32 frame_rate_den)
{
	logD(hls_tsmux, _func_, "start, videoCodecId = ", videoCodecId, ", width = ", width, ", height = ", height,
		", frameRate_num = ", frame_rate_num, ", frameRate_den = ", frame_rate_den, ",m_Id#", m_Id);

	if(videoCodecId != VideoStream::VideoCodecId::AVC)
	{
		logE(hls_tsmux, _func_, "msg->codec_id is not AVC, codec_id = ", videoCodecId, ",m_Id#", m_Id);
		logE(hls_tsmux, _func_, "HLS module supports only H264 (AVC)", ",m_Id#", m_Id);
		return false;
	}

	assert(m_iSegmentDuration != -1);
	if(m_iSegmentDuration == -1)
	{
		logE(hls_tsmux, _func_, "(m_iSegmentDuration == -1), very bad situation, check it", ",m_Id#", m_Id);
		return false;
	}

	assert(m_pOutFormatCtx == NULL);
	if(m_pOutFormatCtx)
	{
		logE(hls_tsmux, _func_, "m_pOutFormatCtx != NULL, very bad situation, check it", ",m_Id#", m_Id);
		return false;
	}

	if(width <= 0 || height <= 0 || frame_rate_num <= 0 || frame_rate_den <= 0)
	{
		logE(hls_tsmux, _func_, "invalid args, width = ", width, ", height = ", height,
			", frameRate_num = ", frame_rate_num, ", frameRate_den = ", frame_rate_den, ",m_Id#", m_Id);
		return false;
	}

// 	if(codecId != AV_CODEC_ID_H264)
// 	{
// 		logE(hls_tsmux, _func_, "codecId(", codecId, ") != AV_CODEC_ID_H264(", static_cast<Int32>(AV_CODEC_ID_H264), ")", ",m_Id#", m_Id);
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
	logD(hls_tsmux, _func_, "szFilePath = ", fullName.c_str(), ", m_pOutFormatCtx->filename = ", m_pOutFormatCtx->filename, ",m_Id#", m_Id);
    if(ffRes < 0 || !m_pOutFormatCtx)
    {
        logE(hls_tsmux, _func_, "avformat_alloc_output_context2 fails = ", ffRes, ",m_Id#", m_Id);
        return false;
	}
	logD(hls_tsmux, _func_, "format name = ", m_pOutFormatCtx->oformat->name, ", long name = ", m_pOutFormatCtx->oformat->long_name, ",m_Id#", m_Id);
	// initialize video stream
	{
		AVStream * pStream = avformat_new_stream(m_pOutFormatCtx, NULL);

		if(!pStream)
		{
			logE(hls_tsmux, _func_, "avformat_new_stream failed!", ",m_Id#", m_Id);
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
		logD(hls_tsmux, _func_, "m_pOutFormatCtx has no AVFMT_NOFILE flag", ",m_Id#", m_Id);

		if(avio_open(&m_pOutFormatCtx->pb, fullName.c_str(), AVIO_FLAG_WRITE) < 0)
		{
			logE(hls_tsmux, _func_, "avio_open failed!", ",m_Id#", m_Id);
			return false;
		}
	}
	else
	{
		logD(hls_tsmux, _func_, "m_pOutFormatCtx has AVFMT_NOFILE flag", ",m_Id#", m_Id);
	}

	// write header
	{
		StRef<String> segmentDurationStr = st_makeString(m_iSegmentDuration);
		AVDictionary * pOptions = NULL;

		//av_dict_set(&pOptions, "segment_list_type", "m3u8", 0);
		av_dict_set(&pOptions, "segment_time", segmentDurationStr->cstr(), 0);
		av_dict_set(&pOptions, "segment_format", "mpegts", 0);
		logD(hls_tsmux, _func_, "Segment Duration = ", segmentDurationStr->cstr(), ",m_Id#", m_Id);
		int ffRes = avformat_write_header(m_pOutFormatCtx, &pOptions);

		av_dict_free(&pOptions);

		if(ffRes < 0)
		{
			logE(hls_tsmux, _func_, "avformat_write_header failed!", ",m_Id#", m_Id);
			return false;
		}
	}

	const std::string & bsfName = GetOwnBitStreamFilterName();

	m_pBSFH264 = av_bitstream_filter_init(bsfName.c_str());	// we use own bsf to avoid many reallocation operations
	if (!m_pBSFH264)
	{
		logE_(_func_, "No available '", bsfName.c_str(), "' bitstream filter", ",m_Id#", m_Id);
		return false;
	}

	if(!m_MuxerData.Init(0, m_uiMaxSegmentSizeInBytes))
	{
		logE_(_func_, "m_MuxerData.Init(0, ", m_uiMaxSegmentSizeInBytes, ") failed!", ",m_Id#", m_Id);
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
			logE(hls_tsmux, _func_, "first timestamp_nanosec(", timestamp_nanosec, ") is incorrect, PLEASE CHECK IT!", ",m_Id#", m_Id);
			return AV_NOPTS_VALUE;
		}

		logE(hls_tsmux, _func_, "set first time = ", timestamp_nanosec, ",m_Id#", m_Id);
		// first frame
		m_uiiPreviousTime = m_uiiFirstTime = timestamp_nanosec;
	}

	if(timestamp_nanosec < m_uiiFirstTime)
	{
		logE(hls_tsmux, _func_, "msg->timestamp_nanosec(", timestamp_nanosec, ") < m_uiiFirstTime(", m_uiiFirstTime, "), PLEASE CHECK IT!", ",m_Id#", m_Id);
		return AV_NOPTS_VALUE;
	}

	if((timestamp_nanosec - m_uiiPreviousTime) > ONE_SEC_NANOSEC)
	{
		logE(hls_tsmux, _func_, "diff(", (timestamp_nanosec - m_uiiPreviousTime), ") > ONE_SEC_NANOSEC(", ONE_SEC_NANOSEC,
			"), PLEASE CHECK IT! time=", timestamp_nanosec, ", previous = ", m_uiiPreviousTime, ",m_Id#", m_Id);
		//return AV_NOPTS_VALUE;	// NEW_TIME_HANDLING: segment_muxer will handle a time
	}

	m_uiiPreviousTime = timestamp_nanosec;

	//const Uint64 diff = timestamp_nanosec - m_uiiFirstTime;
	const Uint64 diff = timestamp_nanosec;	// NEW_TIME_HANDLING: segment_muxer will handle a time

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
		logE(hls_tsmux, _func_, "msg is NULL", ",m_Id#", m_Id);
		return CTsMuxer::Fail;
	}

	if(msg->codec_id != VideoStream::VideoCodecId::AVC)
	{
		logE(hls_tsmux, _func_, "msg->codec_id is not AVC, codec_id = ", msg->codec_id, ",m_Id#", m_Id);
		logE(hls_tsmux, _func_, "HLS module supports only H264 (AVC)", ",m_Id#", m_Id);
		return CTsMuxer::Fail;
	}

	if(!IsInternalsInit())
	{
		if(!InternalInit(msg->codec_id, msg->width, msg->height, msg->frame_rate.num, msg->frame_rate.den))
		{
			logE(hls_tsmux, _func_, "InternalInit failed!", ",m_Id#", m_Id);
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
			logE(hls_tsmux, _func_, "PrepareVideoMessage failed!", ",m_Id#", m_Id);
		}

		return bufRes;
	}

	if(uiBufferSize == 0)
	{
		logE(hls_tsmux, _func_, "skip empty frame, size is 0", ",m_Id#", m_Id);
		return CTsMuxer::NeedMoreData;
	}

	if(m_uiiFirstTime == Uint64_Max)
	{
		// check first frame:
		// this frame must be KeyFrame and has good time stamp
		if((msg->timestamp_nanosec == 0) || (msg->timestamp_nanosec > (Uint64_Max / 2)) ||	// 0 and time more than (Uint64_Max/2) is very suspicious time
			(msg->frame_type != VideoStream::VideoFrameType::KeyFrame))
		{
			logE(hls_tsmux, _func_, "SKIP FRAME: timestamp_nanosec(", msg->timestamp_nanosec, ") is incorrect and frame type [",
				msg->frame_type, "], PLEASE CHECK IT!", ",m_Id#", m_Id);
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
	//	"size=", uiBufferSize, ", timestamp_nanosec=", msg->timestamp_nanosec, ",pts=", videoPacket.pts, ", m_iVideoIndex = ", m_iVideoIndex, ",m_Id#", m_Id);

	m_MuxerData.PrepareWriting();

	// we replaced the write module for segment muxer and av_interleaved_write_frame() will call CTsMuxer::WriteData function.
	if(av_interleaved_write_frame(m_pOutFormatCtx, &videoPacket) < 0)
	{
		logE(hls_tsmux, _func_, "av_interleaved_write_frame failed! we will restart TS muxer", ",m_Id#", m_Id);
		InternalDeinit();
		return CTsMuxer::Fail;
	}

	//logD(hls_tsmux, _func_, "av_interleaved_write_frame success", ",m_Id#", m_Id);
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
		logE(hls_tsmux, _func_, "failed! uiBufferSize == 0", ",m_Id#", m_Id);
		return false;
	}

	if(bufCnt == 1)
	{
		//logD(hls_tsmux, _func_, "pages cnt = 1, without copying, uiBufferSize = ", uiOutputBufferSize, ",m_Id#", m_Id);
		// here we do not copy any memory
		pOutputBuffer = pageList.first->getData();
		uiOutputBufferSize = pageList.first->data_len;
		return true;
	}
	else
	{
		logD(hls_tsmux, _func_, "pages cnt = ", bufCnt, ", uiBufferSize = ", uiOutputBufferSize, ", previous buffer.size = ", buffer.size(), ",m_Id#", m_Id);
	}

	if(!ReallocBuffer(m_Id, buffer, uiOutputBufferSize, VIDEO_ALLOC_PADDING_SIZE))
	{
		uiOutputBufferSize = 0;	// JIC
		logE(hls_tsmux, _func_, "ReallocBuffer failed!", ",m_Id#", m_Id);
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
		logE(hls_tsmux, _func_, "MovePagesToBuffer for video failed!", ",m_Id#", m_Id);
		return CTsMuxer::Fail;
	}

	AVCodecContext * pCodecContext = m_pOutFormatCtx->streams[m_iVideoIndex]->codec;

	if(!pCodecContext->extradata)
	{
		// we do not initialize encoder, because we have x264 stream already, and therefore we need to fill
		// extradata themselves, so h264_mp4toannexb filter does not work without valid extradata.
		logD(hls_tsmux, _func_, "pCodecContext->extradata is NULL and frame is ", msg->frame_type, ",m_Id#", m_Id);
		if(msg->frame_type == VideoStream::VideoFrameType::AvcSequenceHeader)
		{
			logD(hls_tsmux, _func_, "set new AVC header data,m_Id#", m_Id);
			m_AVCHeaderData.clear();

			if(!ReallocBuffer(m_Id, m_AVCHeaderData, uiBufSize, 0, false))
			{
				logE(hls_tsmux, _func_, "out of memory, m_AVCHeaderData,m_Id#", m_Id);
				return CTsMuxer::Fail;
			}

			memcpy(&m_AVCHeaderData[0], pBuffer, uiBufSize);
		}
		else if(m_AVCHeaderData.empty())
		{
			logE(hls_tsmux, _func_, "frame is not AvcSequenceHeader and m_AVCHeaderData is empty,m_Id#", m_Id);
			return CTsMuxer::NeedMoreData;
		}
		else
		{
			logD(hls_tsmux, _func_, "for extradata is used old AVC header data, size = ", m_AVCHeaderData.size(), ",m_Id#", m_Id);
		}

		pCodecContext->extradata = reinterpret_cast<uint8_t *>(av_malloc(m_AVCHeaderData.size()));

		if(!pCodecContext->extradata)
		{
			logE(hls_tsmux, _func_, "out of memory", ",m_Id#", m_Id);
			return CTsMuxer::Fail;
		}

		pCodecContext->extradata_size = m_AVCHeaderData.size();

		memcpy(pCodecContext->extradata, &m_AVCHeaderData[0], m_AVCHeaderData.size());
		logD(hls_tsmux, _func_, "pCodecContext->extradata_size = ", pCodecContext->extradata_size, ",m_Id#", m_Id);
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

		logE(hls_tsmux, _func_, "av_bitstream_filter_filter failed! ffRes = ", ffRes, ", iOutBufSize = ", iOutBufSize, ",m_Id#", m_Id);
		return CTsMuxer::Fail;
	}
	//logD(hls_tsmux, _func_, "av_bitstream_filter_filter, pSrc(", (ptrdiff_t)pBuffer, ")=?pDst(", (ptrdiff_t)pTempOutBuf,
	//	"), srcSize(", uiBufSize, ")=?dstSize(", iOutBufSize, ")", ",m_Id#", m_Id);
	if(ffRes == 0 && pBuffer != pTempOutBuf)
	{
		// THIS WAY IS NOT TESTED, because it never called
		// pTempOutBuf is part of pVideoBuffer
		logD(hls_tsmux, _func_, "ffRes == 0 && pVideoBuffer != pTempOutBuf", ",m_Id#", m_Id);
		if(pBuffer == &m_PagesMemory[0])
		{
			size_t uiFullSize = pTempOutBuf - pBuffer + iOutBufSize;	// uiFullSize must <= uiBufSize
			logD(hls_tsmux, _func_, "ffRes = 0, uiFullSize = ", uiFullSize, ", m_PagesMemory.size() = ",
				m_PagesMemory.size(), ", remainder = ", m_PagesMemory.size() - uiFullSize, ",m_Id#", m_Id);
			assert(m_PagesMemory.size() > (uiFullSize + FF_INPUT_BUFFER_PADDING_SIZE) && uiFullSize <= uiBufSize);
			// pTempOutBuf is pointer on m_PagesMemory, we allocate m_PagesMemory always more on VIDEO_ALLOC_PADDING_SIZE bytes
			memset(pTempOutBuf + iOutBufSize, 0, std::min(static_cast<size_t>(FF_INPUT_BUFFER_PADDING_SIZE), m_PagesMemory.size() - uiFullSize));

			pOutBuffer = pTempOutBuf;
			uiOutBufferSize = iOutBufSize;
		}
		else
		{
			logD(hls_tsmux, _func_, "uiBufSize = ", uiBufSize, ", iOutBufSize = ", iOutBufSize, ", m_PagesMemory.size() = ",
				m_PagesMemory.size(), ", remainder = ", m_PagesMemory.size() - iOutBufSize, ",m_Id#", m_Id);

			if(!ReallocBuffer(m_Id, m_PagesMemory, std::max(uiBufSize, static_cast<size_t>(iOutBufSize)), VIDEO_ALLOC_PADDING_SIZE))
			{
				logE(hls_tsmux, _func_, "ReallocBuffer failed", ",m_Id#", m_Id);
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
		//	uiBufSize, ", m_PagesMemory.size() = ", m_PagesMemory.size(), ",m_Id#", m_Id);

		const size_t maxBufSize = std::max(uiBufSize, static_cast<size_t>(iOutBufSize));

		if(pBuffer != &m_PagesMemory[0] || maxBufSize > m_PagesMemory.size())
		{
			if(!ReallocBuffer(m_Id, m_PagesMemory, maxBufSize, VIDEO_ALLOC_PADDING_SIZE))
			{
				logE(hls_tsmux, _func_, "ReallocBuffer failed", ",m_Id#", m_Id);
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
		logE(hls_tsmux, _func_, "Failed h264_mp4toannexb bitstream filter, ffRes = ", ffRes, ",m_Id#", m_Id);
		return CTsMuxer::Fail;
	}
	else
	{
		// ffRes == 0 && pVideoBuffer == pTempOutBuf
		logD(hls_tsmux, _func_, "bsf is not applied", ",m_Id#", m_Id);
		pOutBuffer = pBuffer;
		uiOutBufferSize = uiBufSize;
	}

	return CTsMuxer::Success;
}

bool CTsMuxer::ReallocBuffer(Uint64 id, std::vector<Uint8> & buffer, size_t newSize, size_t addAllocSize/* = 0*/, bool align/* = true*/)
{
	size_t uiAllocSize = newSize + addAllocSize;

	if(buffer.size() < uiAllocSize)
	{
		if(align)
		{
			// align 1024
			uiAllocSize = ((uiAllocSize + 1023)>>10)<<10;
		}

		try
		{
			logD(hls_tsmux, _func_, "buffer was reallocated from = ", buffer.size(), " to ", uiAllocSize, ", newSize = ", newSize, ",ptr=", (UintPtr)&buffer, ",m_Id#", id);
			buffer.resize(uiAllocSize);
		}
		catch(std::bad_alloc&) 	// catch bad_alloc
		{
			logE(hls_tsmux, _func_, "exception std::bad_alloc", ",m_Id#", id);
			return false;
		}

		if(buffer.size() != uiAllocSize)	// JIC
		{
			logE(hls_tsmux, _func_, "failed! out of memory", ",m_Id#", id);
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

CTsMuxer::CMuxedData::CMuxedData( Uint64 parentId )
	: m_uiActualSize(0)
	, m_uiSegmentSize(static_cast<size_t>(-1))
	, m_uiiCurrentSegmentId(0)
	, m_uiMaxSegmentSize(static_cast<size_t>(-1))
	, m_Id(parentId)
{
	logD(hls_tsmux, _func_, ",m_Id#", m_Id);
}

CTsMuxer::CMuxedData::~CMuxedData( void )
{
	logD(hls_tsmux, _func_, ",m_Id#", m_Id);
	Deinit();
}

void CTsMuxer::CMuxedData::Deinit( void )
{
	logD(hls_tsmux, _func_, "start", ",m_Id#", m_Id);
	m_uiActualSize = 0;
	m_uiSegmentSize = static_cast<size_t>(-1);
	m_uiiCurrentSegmentId = 0;
	m_Data.clear();
	logD(hls_tsmux, _func_, "end", ",m_Id#", m_Id);
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
		", remainder and new m_uiActualSize = ", uiRemainder, ", m_Data.size() = ", m_Data.size(), ",m_Id#", m_Id);
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

	if(m_uiSegmentSize > 0)
	{
		logD(hls_tsmux, _func_, "new segment, size = ", m_uiSegmentSize, ",m_Id#", m_Id);
		muxedData = ConstMemory(&m_Data[0], m_uiSegmentSize);

		return CTsMuxer::Success;
	}

	return CTsMuxer::NeedMoreData;
}

bool CTsMuxer::CMuxedData::StoreData( Uint64 curSegmId, const Uint8 * buf, size_t size )
{
	if(!buf || !size)
	{
		logE(hls_tsmux, _func_, "failed, buf=", (UintPtr)buf, "size=", size, ",m_Id#", m_Id);
		return false;
	}

	const size_t uiNewActualSize = m_uiActualSize + size;

	if(uiNewActualSize > m_Data.size())
	{
		logD(hls_tsmux, _func_, "uiNewActualSize(", uiNewActualSize, ") > m_Data.size(", m_Data.size(), "), start realloc,m_Id#", m_Id);
		if(uiNewActualSize > m_uiMaxSegmentSize)
		{
			logE(hls_tsmux, _func_, "uiNewActualSize(", uiNewActualSize, ") > m_uiMaxSegmentSize(", m_uiMaxSegmentSize,  "),m_Id#", m_Id);
			return false;
		}

		if(!CTsMuxer::ReallocBuffer(m_Id, m_Data, uiNewActualSize, 4 * 1024))
		{
			logE(hls_tsmux, _func_, "failed! out of memory, m_Data", ",m_Id#", m_Id);
			return false;
		}
	}

	const size_t uiPreviousSize = m_uiActualSize;

	memcpy((&m_Data[0]) + m_uiActualSize, buf, size);

	m_uiActualSize = uiNewActualSize;

	if(m_uiiCurrentSegmentId != curSegmId)
	{
		assert(m_uiSegmentSize == static_cast<size_t>(-1));
		m_uiSegmentSize = (uiPreviousSize > 0) ? uiPreviousSize : static_cast<size_t>(-1);
		logD(hls_tsmux, _func_, "new segment, size = ", m_uiSegmentSize, ", newSegmId = ", curSegmId, ", oldSegmId = ", m_uiiCurrentSegmentId, ",m_Id#", m_Id);
		m_uiiCurrentSegmentId = curSegmId;
	}

	return true;
}

}   // namespace MomentHls

