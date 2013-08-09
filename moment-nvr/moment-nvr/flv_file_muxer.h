
#ifndef MOMENT_NVR__FLV_FILE_MUXER_H__
#define MOMENT_NVR__FLV_FILE_MUXER_H__

#include <libmary/libmary.h>
#include <moment/libmoment.h>

namespace MomentNvr
{

using namespace M;
using namespace Moment;

namespace FLV
{

typedef enum _VideoFormat
{
	FLV_VideoFormat_None			= -1,
	FLV_VideoFormat_H263			= 2,
	FLV_VideoFormat_Screen			= 3,
	FLV_VideoFormat_VP6				= 4,
	FLV_VideoFormat_VP6Alpha		= 5,
	FLV_VideoFormat_ScreenV2		= 6,
	FLV_VideoFormat_AVC				= 7,
	FLV_VideoFormat_REALH263		= 8,
	FLV_VideoFormat_MPEG4			= 9,
} VideoFormat;

typedef enum _AudioFormat
{
	FLV_AudioFormat_None			= -1,
	FLV_AudioFormat_LPCM			= 0,	//Linear PCM, platform endian
	FLV_AudioFormat_ADPCM			= 1,
	FLV_AudioFormat_MP3				= 2,
	FLV_AudioFormat_LPCM_le			= 3,	//Linear PCM, little endian
	FLV_AudioFormat_Nellymoser16	= 4,	//Nellymoser 16-kHz mono
	FLV_AudioFormat_Nellymoser8		= 5,	//Nellymoser 8-kHz mono
	FLV_AudioFormat_Nellymoser		= 6,	//Nellymoser
	FLV_AudioFormat_G711_A			= 7,	//G.711 A-law logarithmic PCM
	FLV_AudioFormat_G711_mu			= 8,	//G.711 mu-law logarithmic PCM
	//								= 9,	//reversed
	FLV_AudioFormat_AAC				= 10,
	FLV_AudioFormat_Speex			= 11,	//Speex
	//								= 12,	//reversed
	//								= 13,	//reversed
	FLV_AudioFormat_MP3_8			= 14,	//MP3 8-Khz
	FLV_AudioFormat_DSS				= 15	//Device-specific sound
} AudioFormat;


typedef enum _FLVStreamType
{
	FLV_StreamType_Video,
	FLV_StreamType_Audio,
	FLV_StreamType_Data,
} FLVStreamType;


typedef struct _InitStreamParams
{
	FLVStreamType streamType;

	union StreamParams
	{
		struct VideoParams
		{
			VideoFormat format;
			double framerate;	// can be 0
			Int32 width;		// can be 0
			Int32 height;		// can be 0
		} videoParams;

		struct AudioParams
		{
			AudioFormat format;
			Int32 bitsPerSample;	// 8 or 16, can be 0
			Int32 sampleRate;
			Int32 channels;
		} audioParams;

	} streamParams;
} InitStreamParams;


class CFlvFileMuxerInitParams
{

public:

	// c-tor
	CFlvFileMuxerInitParams(void)
	{
		m_uiNumStreams = 0;
		m_dDuration = 0;
		memset(&m_StreamParams, 0, sizeof(m_StreamParams));
	}

	CFlvFileMuxerInitParams(bool bVideo, bool bAudio)
	{
		m_dDuration = 0;
		memset(&m_StreamParams, 0, sizeof(m_StreamParams));
		m_uiNumStreams = (!!bVideo) + (!!bAudio);

		Int32 iNum = 0;

		if(bVideo)
		{
			m_StreamParams[iNum].streamType = FLV_StreamType_Video;

			++iNum;
		}


		if(bAudio)
		{
			m_StreamParams[iNum].streamType = FLV_StreamType_Audio;

			++iNum;
		}
	}

	// now FLV can have only 2 streams
	InitStreamParams m_StreamParams[2];
	Uint32 m_uiNumStreams;

	double m_dDuration;	// in seconds, can be 0
};

mt_unsafe class CFlvFileMuxer
{
private:

	class MemoryEx : public Memory
	{

	private:

		Size cur_size;

	public:

		Size size() const { return cur_size; }
		void setSize(Size newPos) { cur_size = newPos; }

		MemoryEx ()
			: Memory()
			, cur_size(0)
		{
		}

		MemoryEx (Byte * const mem, Size const len)
			: Memory(mem, len)
			, cur_size(0)
		{
		}
	};

	typedef String MemorySafe;

	class StreamInfo
	{
	public:

		InitStreamParams initParams;
		Int64 iiLastTimeStamp;    // last timestamp for each stream
		bool bHeaderIsWritten;
		Int32 reserved;

		StreamInfo(void)
		{
			bHeaderIsWritten = false;
			memset(&initParams, 0, sizeof(initParams));
			iiLastTimeStamp = Int64_Min;
			reserved = 0;
		}
	};

public:

	Result Init(CFlvFileMuxerInitParams * pParams, Ref<Vfs::VfsFile> fileToSave);

	mt_throws Result beginMuxing ();
	mt_throws Result endMuxing   ();

	mt_throws Result muxAudioMessage (VideoStream::AudioMessage * mt_nonnull msg);
	mt_throws Result muxVideoMessage (VideoStream::VideoMessage * mt_nonnull msg);

	CFlvFileMuxer (void);
	~CFlvFileMuxer (void);

protected:

	Result FlvWriteHeader(Int64 * piDuration, Int64 * piiFileSize);	// pDuration can be NULL, if you want to save duration on start use it.
	Result FlvWriteEOS(void);
	Result FlvWritePacket(VideoStream::Message * const mt_nonnull msg, StreamInfo * pStreamInfo,
		bool bKeyFrame = false/*it actual only for Video packet*/);

	Result SaveStreamHeader(ConstMemory const memory, StreamInfo * pStreamInfo);

	// clean all resources
	void FreeResources(void);

	Int64 m_iiDuration;
	Int64 m_iiDelay;      // first dts delay (needed for AVC & Speex), by default Int64_Max

	// now FLV can have only 2 streams
	StreamInfo m_StreamsInfo[2];
	Uint32	m_uiNumStreams;

protected:

	Int32 GetAudioFlags(InitStreamParams * pAudioParams);

	VideoFormat GetVideoFormat(VideoStream::VideoCodecId codecId);
	AudioFormat GetAudioFormat(VideoStream::AudioCodecId codecId);

	// file save
	Ref<Vfs::VfsFile> m_File;
	MemorySafe m_AllocatedMemory;	// it contains allocated memory
	MemoryEx m_OutMemory;	// m_OutMemory uses m_AllocatedMemory
	// file operations
	Int64 FileTell(void);
	Result FileSeek(FileOffset offset, SeekOrigin origin);
	Result FileSkip(FileOffset offset);
	// write file operations
	void FlushBufferInternal(void);
	void WriteDataInternal(ConstMemory const memory);
	void WriteB8Internal(Int32 b);
	void WriteB16Internal(Uint32 val);
	void WriteB24Internal(Uint32 val);
	void WriteB32Internal(Uint32 val);
	void WriteB64Internal(Uint64 val);
	void PutAmfString(ConstMemory const str);
	void PutAmfDouble(double d);
	void PutAmfBool(Int32 b);
	void PutAvcEosTag(Uint32 ts);

	Result WriteB8ToBuffer(Int32 b, MemoryEx & memory);
	Result WriteB32ToBuffer(Uint32 val, MemoryEx & memory);
	Result WriteDataToBuffer(ConstMemory const memory, MemoryEx & memoryOut);

	Int32 AvcParseNalUnits(ConstMemory const mem, MemoryEx * pMemoryOut);
	Result IsomWriteAvcc(ConstMemory const memory);
};

}	// namespace FLV

}	// namespace MomentNvr

#endif	// MOMENT_NVR__FLV_FILE_MUXER_H__

