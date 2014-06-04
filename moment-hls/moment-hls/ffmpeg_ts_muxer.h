
#ifndef __MOMENT_HLS__FFMPEG__TS__MUXER__H__
#define __MOMENT_HLS__FFMPEG__TS__MUXER__H__

#include <moment/video_stream.h>
#include <vector>
#include <map>

// no necessary to show ffmpeg headers for another sources, so only declare:
struct AVFormatContext;
struct AVBitStreamFilterContext;

namespace MomentHls
{

using namespace M;
using namespace Moment;

// CTsMuxer is not thread-safe class
class CTsMuxer
{

public:

	enum Result
	{
		Success,
		Fail,
		NeedMoreData,
	};

	struct TsSegmentInfo
	{
		UintPtr	owner;
		Uint64 id;

		TsSegmentInfo() : owner(0), id(0) {}
	};

public:

    CTsMuxer();
    ~CTsMuxer();

    bool Init(int segmentDuration, size_t uiMaxSegmentSizeInBytes);
	bool IsInit() const { return (m_iSegmentDuration != -1); }
    void Deinit();

	// CTsMuxer is not thread-safe class, so you must call WriteVideoMessage using lock, because
	// the memory that returned in 'muxedData' is one memory for CTsMuxer instance.
    Result WriteVideoMessage(VideoStream::VideoMessage * mt_nonnull msg, ConstMemory & muxedData);

	bool IsInternalsInit() const { return (m_pOutFormatCtx != NULL); }

public /*static functions*/:

	// for buffer protocol
	static bool GetTsSegmentInfo(const char * pszFileName, TsSegmentInfo & info);
	static bool WriteData(const TsSegmentInfo & info, const Uint8 * buf, size_t size);

private /*functions for internal init and deinit*/:

	bool InternalInit(VideoStream::VideoCodecId codecId, Int32 width, Int32 height, Int32 frame_rate_num, Int32 frame_rate_den);
	void InternalDeinit(void);

private /*functions for service of WriteVideoMessage*/:

	Result PrepareVideoMessage(VideoStream::VideoMessage * mt_nonnull msg, Uint8 *& pBuffer, size_t & uiBufferSize);
	bool MovePagesToBuffer(const PagePool::PageListHead & pageList, Uint8 *& pBuffer, size_t & uiBufferSize, std::vector<Uint8> & buffer);

	static bool ReallocBuffer(Uint64 id, std::vector<Uint8> & buffer, size_t newSize, size_t addAllocSize = 0, bool align = true);
	inline Int64 PtsFromMsg(VideoStream::VideoMessage * mt_nonnull msg);

private /*functions for service of ffmpeg/write part*/:

	bool StoreData(Uint64 curSegmId, const Uint8 * buf, size_t size);

private /*muxer memory class*/:

	class CMuxedData
	{
	private:

		size_t				m_uiActualSize;	// actual data size of 'data' member
		std::vector<Uint8>	m_Data;	// result of TS muxer processing (TS muxer is created inside of segment muxer)
		size_t				m_uiSegmentSize;	// if current segment is finished so m_uiSegmentSize contains the size of it else -1.
		Uint64				m_uiiCurrentSegmentId;
		size_t				m_uiMaxSegmentSize;
		const Uint64		m_Id;	// it is parent Id only for logging

	public:

		CMuxedData(Uint64 parentId);
		~CMuxedData(void);

		bool Init(Uint64 firstSegmentId, size_t uiMaxSegmentSize);
		void Deinit(void);

		void PrepareWriting(void);
		Result GetData(ConstMemory & muxedData);
		bool StoreData(Uint64 curSegmId, const Uint8 * buf, size_t size);
	};

private /*variables*/:

	const Uint64		m_Id;

    AVFormatContext *	m_pOutFormatCtx;
    Int32				m_iSegmentDuration;	// in seconds

	// for video stream
	Int32						m_iVideoIndex;	// index of video stream in array m_pOutFormatCtx->streams[].
	std::vector<Uint8>			m_PagesMemory;	// it is used for WriteVideoMessage: when input VideoStream::Message has many pages
												// in page_list (PagePool::PageListHead),  we copy all pages to one data buffer (m_PagesMemory).
	AVBitStreamFilterContext *	m_pBSFH264; // filter for malformed H264 (h264_mp4toannexb bitstream filter)
	Uint64						m_uiiFirstTime;	// by default is Int64_Min, first timestamp of VideoStream::VideoMessage
	Uint64						m_uiiPreviousTime;	// by default is Int64_Min, timestamp of previous VideoStream::VideoMessage
	//std::vector<Uint8>			m_FrameMemory;	// it is used for bitstream filter and to avoid frequent reallocations.
	size_t						m_uiMaxSegmentSizeInBytes;
	CMuxedData					m_MuxerData;
	std::vector<Uint8>			m_AVCHeaderData;

private /*helper classes*/:

	class CAutoInternalDeinit
	{
		bool m_bInitSucceeded;
		CTsMuxer * m_pParent;

	public:

		CAutoInternalDeinit(CTsMuxer * parent) : m_bInitSucceeded(false), m_pParent(parent) {}

		~CAutoInternalDeinit()
		{
			if(!m_bInitSucceeded)
			{
				m_pParent->InternalDeinit();
			}
		}

		void InitSucceeded() { m_bInitSucceeded = true; }
	};

	friend class CAutoInternalDeinit;

private /*static variables*/:

	static Uint64							g_NextId;
	static std::map<Uint64, CTsMuxer *>		g_MuxersMap;
};

}   // namespace MomentHls

#endif /* __MOMENT_HLS__FFMPEG__TS__MUXER__H__ */

