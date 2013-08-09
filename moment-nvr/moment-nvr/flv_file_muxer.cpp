
#include <moment-nvr/flv_file_muxer.h>
#include <algorithm>

using namespace M;
using namespace Moment;

namespace MomentNvr
{

namespace FLV
{
// offsets for packed values 
#define FLV_AUDIO_SAMPLESSIZE_OFFSET 1
#define FLV_AUDIO_SAMPLERATE_OFFSET  2
#define FLV_AUDIO_CODECID_OFFSET     4

#define FLV_VIDEO_FRAMETYPE_OFFSET   4

// bitmasks to isolate specific values
#define FLV_AUDIO_CHANNEL_MASK    0x01
#define FLV_AUDIO_SAMPLESIZE_MASK 0x02
#define FLV_AUDIO_SAMPLERATE_MASK 0x0c
#define FLV_AUDIO_CODECID_MASK    0xf0

#define FLV_VIDEO_CODECID_MASK    0x0f
#define FLV_VIDEO_FRAMETYPE_MASK  0xf0

#define AMF_END_OF_OBJECT         0x09

#define KEYFRAMES_TAG            "keyframes"
#define KEYFRAMES_TIMESTAMP_TAG  "times"
#define KEYFRAMES_BYTEOFFSET_TAG "filepositions"


enum {
    FLV_HEADER_FLAG_HASVIDEO = 1,
    FLV_HEADER_FLAG_HASAUDIO = 4,
};

enum {
    FLV_TAG_TYPE_AUDIO = 0x08,
    FLV_TAG_TYPE_VIDEO = 0x09,
    FLV_TAG_TYPE_META  = 0x12,
};



enum {
    FLV_MONO   = 0,
    FLV_STEREO = 1,
};

enum {
    FLV_SAMPLESSIZE_8BIT  = 0,
    FLV_SAMPLESSIZE_16BIT = 1 << FLV_AUDIO_SAMPLESSIZE_OFFSET,
};

enum {
    FLV_SAMPLERATE_SPECIAL = 0, // signifies 5512Hz and 8000Hz in the case of NELLYMOSER
    FLV_SAMPLERATE_11025HZ = 1 << FLV_AUDIO_SAMPLERATE_OFFSET,
    FLV_SAMPLERATE_22050HZ = 2 << FLV_AUDIO_SAMPLERATE_OFFSET,
    FLV_SAMPLERATE_44100HZ = 3 << FLV_AUDIO_SAMPLERATE_OFFSET,
};

enum {
    FLV_CODECID_PCM                  = 0,
    FLV_CODECID_ADPCM                = 1 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_MP3                  = 2 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_PCM_LE               = 3 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_NELLYMOSER_16KHZ_MONO = 4 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_NELLYMOSER_8KHZ_MONO = 5 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_NELLYMOSER           = 6 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_PCM_ALAW             = 7 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_PCM_MULAW            = 8 << FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_AAC                  = 10<< FLV_AUDIO_CODECID_OFFSET,
    FLV_CODECID_SPEEX                = 11<< FLV_AUDIO_CODECID_OFFSET,
};

enum {
	FLV_CODECID_H263    = 2,
	FLV_CODECID_SCREEN  = 3,
	FLV_CODECID_VP6     = 4,
	FLV_CODECID_VP6A    = 5,
	FLV_CODECID_SCREEN2 = 6,
	FLV_CODECID_H264    = 7,
	FLV_CODECID_REALH263= 8,
	FLV_CODECID_MPEG4   = 9,
};

enum {
    FLV_FRAME_KEY            = 1 << FLV_VIDEO_FRAMETYPE_OFFSET, ///< key frame (for AVC, a seekable frame)
    FLV_FRAME_INTER          = 2 << FLV_VIDEO_FRAMETYPE_OFFSET, ///< inter frame (for AVC, a non-seekable frame)
    FLV_FRAME_DISP_INTER     = 3 << FLV_VIDEO_FRAMETYPE_OFFSET, ///< disposable inter frame (H.263 only)
    FLV_FRAME_GENERATED_KEY  = 4 << FLV_VIDEO_FRAMETYPE_OFFSET, ///< generated key frame (reserved for server use only)
    FLV_FRAME_VIDEO_INFO_CMD = 5 << FLV_VIDEO_FRAMETYPE_OFFSET, ///< video info/command frame
};

typedef enum {
    AMF_DATA_TYPE_NUMBER      = 0x00,
    AMF_DATA_TYPE_BOOL        = 0x01,
    AMF_DATA_TYPE_STRING      = 0x02,
    AMF_DATA_TYPE_OBJECT      = 0x03,
    AMF_DATA_TYPE_NULL        = 0x05,
    AMF_DATA_TYPE_UNDEFINED   = 0x06,
    AMF_DATA_TYPE_REFERENCE   = 0x07,
    AMF_DATA_TYPE_MIXEDARRAY  = 0x08,
    AMF_DATA_TYPE_OBJECT_END  = 0x09,
    AMF_DATA_TYPE_ARRAY       = 0x0a,
    AMF_DATA_TYPE_DATE        = 0x0b,
    AMF_DATA_TYPE_LONG_STRING = 0x0c,
    AMF_DATA_TYPE_UNSUPPORTED = 0x0d,
} AMFDataType;


/************************************************************************/
/*                       Static functions                               */
/************************************************************************/
static Uint32 GetVideoCodecTag(VideoFormat format)
{
	switch(format)
	{
	case FLV_VideoFormat_H263:			return FLV_CODECID_H263;
	case FLV_VideoFormat_Screen:		return FLV_CODECID_SCREEN;
	case FLV_VideoFormat_VP6:			return FLV_CODECID_VP6;
	case FLV_VideoFormat_VP6Alpha:		return FLV_CODECID_VP6A;
	case FLV_VideoFormat_ScreenV2:		return FLV_CODECID_SCREEN2;
	case FLV_VideoFormat_AVC:			return FLV_CODECID_H264;
	case FLV_VideoFormat_REALH263:		return FLV_CODECID_REALH263;
	case FLV_VideoFormat_MPEG4:			return FLV_CODECID_MPEG4;
	default:
		logE_ (_func, "Unknown video format ", (Int32)format);
		break;
	}

	return 0;
}

static Uint32 GetAudioCodecTag(AudioFormat format)
{
	switch(format)
	{
	case FLV_AudioFormat_LPCM:			return FLV_CODECID_PCM        >> FLV_AUDIO_CODECID_OFFSET;
	case FLV_AudioFormat_ADPCM:			return FLV_CODECID_ADPCM      >> FLV_AUDIO_CODECID_OFFSET;
	case FLV_AudioFormat_MP3_8:
	case FLV_AudioFormat_MP3:			return FLV_CODECID_MP3        >> FLV_AUDIO_CODECID_OFFSET;
	case FLV_AudioFormat_LPCM_le:		return FLV_CODECID_PCM_LE     >> FLV_AUDIO_CODECID_OFFSET;
	case FLV_AudioFormat_Nellymoser16:
	case FLV_AudioFormat_Nellymoser8:
	case FLV_AudioFormat_Nellymoser:	return FLV_CODECID_NELLYMOSER >> FLV_AUDIO_CODECID_OFFSET;
	case FLV_AudioFormat_G711_A:		return FLV_CODECID_PCM_ALAW   >> FLV_AUDIO_CODECID_OFFSET;
	case FLV_AudioFormat_G711_mu:		return FLV_CODECID_PCM_MULAW  >> FLV_AUDIO_CODECID_OFFSET;
	case FLV_AudioFormat_AAC:			return FLV_CODECID_AAC        >> FLV_AUDIO_CODECID_OFFSET;
	case FLV_AudioFormat_Speex:			return FLV_CODECID_SPEEX      >> FLV_AUDIO_CODECID_OFFSET;
	case FLV_AudioFormat_DSS:			return FLV_AudioFormat_DSS;
	default:
		logE_ (_func, "Unknown audio format ", (Int32)format);
		break;
	}

	return 0;
}

static Uint64 double2int(double f)
{
	union av_intfloat64
	{
		Uint64 i;
		double f;
	};

	union av_intfloat64 v;
	v.f = f;
	return v.i;
}

// avc part
static const Byte * AvcFindStartCodeInternal(const Byte *p, const Byte *end)
{
	const Byte *a = p + 4 - ((intptr_t)p & 3);

	for (end -= 3; p < a && p < end; p++) {
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	for (end -= 3; p < end; p += 4)
	{
		Uint32 x = *(const Uint32*)p;
		//      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
		//      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
		if ((x - 0x01010101) & (~x) & 0x80808080)
		{
			// generic
			if (p[1] == 0)
			{
				if (p[0] == 0 && p[2] == 1)
					return p;
				if (p[2] == 0 && p[3] == 1)
					return p+1;
			}

			if (p[3] == 0)
			{
				if (p[2] == 0 && p[4] == 1)
					return p+2;
				if (p[4] == 0 && p[5] == 1)
					return p+3;
			}
		}
	}

	for (end += 3; p < end; p++)
	{
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	return end + 3;
}

static const Byte * AvcFindStartCode(const Byte *p, const Byte *end)
{
	const Byte * out = AvcFindStartCodeInternal(p, end);

	if(p < out && out < end && !out[-1])
		out--;

	return out;
}

//////////////////////////////////////////////////////////////////////////

#define AV_RB16(x)									\
		((((const Byte*)(x))[0] << 8) |				\
		((const Byte*)(x))[1])

#define AV_RB32(x)									\
		(((Uint32)((const Byte*)(x))[0] << 24) |	\
		(((const Byte*)(x))[1] << 16) |				\
		(((const Byte*)(x))[2] <<  8) |				\
		( (const Byte*)(x))[3])

#define AV_RB24(x)									\
		((((const Byte*)(x))[0] << 16) |			\
		( ((const Byte*)(x))[1] <<  8) |			\
		(  (const Byte*)(x))[2])


/************************************************************************/
/*                    CFlvFileMuxer implementation                      */
/************************************************************************/

CFlvFileMuxer::CFlvFileMuxer ()
	: m_iiDuration(0)
	, m_iiDelay(Int64_Max)
	, m_uiNumStreams(0)
{
}

CFlvFileMuxer::~CFlvFileMuxer()
{
	FreeResources();
}

void CFlvFileMuxer::FreeResources(void)
{
	logD_(_func);

	if(!m_File.isNull())
	{
		FlvWriteEOS();
	}

	m_File = NULL;

	m_AllocatedMemory.allocate(0);
	m_OutMemory = MemoryEx();

	for(Size uiCnt = 0; uiCnt < sizeof(m_StreamsInfo) / sizeof(m_StreamsInfo[0]); ++uiCnt)
	{
		m_StreamsInfo[uiCnt] = StreamInfo();
	}

	m_iiDuration = 0;
	m_iiDelay = Int64_Max;
	m_uiNumStreams = 0;

	logD_(_func, "EXIT");
}

Int32 CFlvFileMuxer::GetAudioFlags(InitStreamParams * pInitParams)
{
	InitStreamParams::StreamParams::AudioParams * pAudioParams = &pInitParams->streamParams.audioParams;
	Int32 flags = (pAudioParams->bitsPerSample == 8) ?
					FLV_SAMPLESSIZE_8BIT : FLV_SAMPLESSIZE_16BIT;

	if (pAudioParams->format == FLV_AudioFormat_AAC) // specs force these parameters
	{
		return	FLV_CODECID_AAC | FLV_SAMPLERATE_44100HZ |
				FLV_SAMPLESSIZE_16BIT | FLV_STEREO;
	}
	else if (pAudioParams->format == FLV_AudioFormat_Speex)
	{
		if(pAudioParams->sampleRate != 16000)
		{
			logD_(_func, "FLV only supports wideband (16kHz) Speex audio\n");
		}
		if (pAudioParams->channels != 1)
		{
			logD_(_func, "FLV only supports mono Speex audio");
		}

		return FLV_CODECID_SPEEX | FLV_SAMPLERATE_11025HZ | FLV_SAMPLESSIZE_16BIT;
	}
	else
	{
		switch (pAudioParams->sampleRate)
		{
		case 44100:
			flags |= FLV_SAMPLERATE_44100HZ;
			break;

		case 22050:
			flags |= FLV_SAMPLERATE_22050HZ;
			break;

		case 11025:
			flags |= FLV_SAMPLERATE_11025HZ;
			break;

		case 16000: // nellymoser only
		case  8000: // nellymoser only
		case  5512: // not MP3
			if(pAudioParams->format != FLV_AudioFormat_MP3)
			{
				flags |= FLV_SAMPLERATE_SPECIAL;
				break;
			}

		default:
			logD_(_func, "FLV does not support sample rate ", pAudioParams->sampleRate, ", choose from (44100, 22050, 11025)");
			flags |= FLV_SAMPLERATE_44100HZ;	// JIC
			break;
		}
	}

	if (pAudioParams->channels != 1)
	{
		flags |= FLV_STEREO;
	}

	switch (pAudioParams->format)
	{
	case FLV_AudioFormat_MP3:
		flags |= FLV_CODECID_MP3    | FLV_SAMPLESSIZE_16BIT;
		break;

	case FLV_AudioFormat_LPCM:
		if(pAudioParams->bitsPerSample == 8)
			flags |= FLV_CODECID_PCM    | FLV_SAMPLESSIZE_8BIT;
		else
			flags |= FLV_CODECID_PCM    | FLV_SAMPLESSIZE_16BIT;
		break;

	case FLV_AudioFormat_LPCM_le:
		flags |= FLV_CODECID_PCM_LE | FLV_SAMPLESSIZE_16BIT;
		break;

	case FLV_AudioFormat_ADPCM:
		flags |= FLV_CODECID_ADPCM  | FLV_SAMPLESSIZE_16BIT;
		break;

	case FLV_AudioFormat_Nellymoser16:
	case FLV_AudioFormat_Nellymoser8:
	case FLV_AudioFormat_Nellymoser:
		if (pAudioParams->sampleRate == 8000)
			flags |= FLV_CODECID_NELLYMOSER_8KHZ_MONO  | FLV_SAMPLESSIZE_16BIT;
		else if (pAudioParams->sampleRate == 16000)
			flags |= FLV_CODECID_NELLYMOSER_16KHZ_MONO | FLV_SAMPLESSIZE_16BIT;
		else
			flags |= FLV_CODECID_NELLYMOSER            | FLV_SAMPLESSIZE_16BIT;
		break;

	case FLV_AudioFormat_G711_mu:
		flags = FLV_CODECID_PCM_MULAW | FLV_SAMPLERATE_SPECIAL | FLV_SAMPLESSIZE_16BIT;
		break;

	case FLV_AudioFormat_G711_A:
		flags = FLV_CODECID_PCM_ALAW  | FLV_SAMPLERATE_SPECIAL | FLV_SAMPLESSIZE_16BIT;
		break;

	default:
		flags |= (Int32)GetAudioCodecTag(pAudioParams->format);
		logD_(_func, "Audio codec ", (Int32)pAudioParams->format, " not compatible with FLV\n");
		break;
	}

	return flags;
}

Result CFlvFileMuxer::Init(CFlvFileMuxerInitParams * pParams, Ref<Vfs::VfsFile> fileToSave)
{
	FreeResources();

	if(fileToSave.isNull())
	{
		logE_ (_func, "fileToSave.isNull().");
		return Result::Failure;
	}

	if(pParams->m_uiNumStreams == 0)
	{
		logE_ (_func, "pParams->m_uiNumStreams == 0.");
		return Result::Failure;
	}

	bool bVideo = false, bAudio = false;

	for(Uint32 uiCnt = 0; uiCnt < pParams->m_uiNumStreams; ++uiCnt)
	{
		InitStreamParams * pStreamParam = &(pParams->m_StreamParams[uiCnt]);

		// copy init params to internal member
		m_StreamsInfo[uiCnt].initParams = *pStreamParam;

		if(!bVideo && pStreamParam->streamType == FLV_StreamType_Video)
		{
			logD_ (_func, "Input video format is ", (Int32)pStreamParam->streamParams.videoParams.format);
			// reset param, we will use it from stream
			m_StreamsInfo[uiCnt].initParams.streamParams.videoParams.format = FLV_VideoFormat_None;

			bVideo = true;
		}
		else if(!bAudio && pStreamParam->streamType == FLV_StreamType_Audio)
		{
			logD_ (_func, "Input audio format is ", (Int32)pStreamParam->streamParams.audioParams.format);

			// reset param, we will use it from stream
			m_StreamsInfo[uiCnt].initParams.streamParams.audioParams.format = FLV_AudioFormat_None;

			bAudio = true;
		}
		else
		{
			// skip 'FLV_StreamType_Data', it is not supported now
			continue;
		}

		// reset params
		m_StreamsInfo[uiCnt].iiLastTimeStamp = Int64_Min;
		m_StreamsInfo[uiCnt].bHeaderIsWritten = false;
		m_StreamsInfo[uiCnt].reserved = 0;

		++m_uiNumStreams;
	}

	if(m_uiNumStreams == 0)
	{
		logE_ (_func, "m_uiNumStreams == 0");
		return Result::Failure;
	}

	// store ptr on file
	m_File = fileToSave;

	const Uint32 uiAllocatedSize = 32 * 1024;
	m_AllocatedMemory.allocate(uiAllocatedSize);
	Result res = m_AllocatedMemory.cstr() ? Result::Success : Result::Failure;

	if(res == Result::Success)
	{
		m_OutMemory = MemoryEx((Byte *)m_AllocatedMemory.cstr(), m_AllocatedMemory.len());

		Int64 iiWritedDuration = (Int64)(pParams->m_dDuration * 1000000.0);
		res = FlvWriteHeader(&iiWritedDuration, NULL);

		if(res != Result::Success)
		{
			FreeResources();
		}
	}

	return res;
}

Result CFlvFileMuxer::beginMuxing()
{
	return m_File.isNull() ? Result::Failure : Result::Success;
}

Result CFlvFileMuxer::endMuxing()
{
	if(m_File.isNull())
		return Result::Failure;

	Result res = FlvWriteEOS();

	m_File = NULL;

	FreeResources();

	return res;
}

Result CFlvFileMuxer::muxAudioMessage(VideoStream::AudioMessage * mt_nonnull msg)
{
	logD_(_func, "MsgType(", (Int32)msg->msg_type, ")Time(", (double)msg->timestamp_nanosec / 1000000000.0,
		")MsgSize(", msg->msg_len, ")MsgOffset(", msg->msg_offset, ")frame_type(",
		msg->frame_type, ")codec_id(", msg->codec_id, ")rate(", msg->rate,
		")channels(", msg->channels, ")");

	// check data
	if(!msg->page_list.first || msg->page_list.first->data_len == 0)
	{
		logE_ (_func, "first(", msg->page_list.first ? "valid" : "invalid", " ptr)len(", msg->page_list.first ? msg->page_list.first->data_len : 0, ")");
		// we must return "Success" instead of "Failure" else moment will be crashed.
		return Result::Success;
	}

	switch(msg->frame_type)
	{
		case VideoStream::AudioFrameType::RawData:
		case VideoStream::AudioFrameType::AacSequenceHeader:
		case VideoStream::AudioFrameType::SpeexHeader:
			break;
		default:
			logE_ (_func, "This frame type(", msg->frame_type, ") is not supported for FLV audio.");
			// we must return "Success" instead of "Failure" else moment will be crashed.
			return Result::Success;
	}

	StreamInfo * pStreamInfo = NULL;

	for(Size uiCnt = 0; uiCnt < sizeof(m_StreamsInfo) / sizeof(m_StreamsInfo[0]); ++uiCnt)
	{
		if(m_StreamsInfo[uiCnt].initParams.streamType == FLV_StreamType_Audio)
		{
			pStreamInfo = &m_StreamsInfo[uiCnt];
			break;
		}
	}

	if(!pStreamInfo)
	{
		logE_ (_func, "!pStreamInfo");
		// we must return "Success" instead of "Failure" else moment will be crashed.
		return Result::Success;
	}

	if(!pStreamInfo->bHeaderIsWritten)
	{
		logD_ (_func, "AUDIO, bHeaderIsWritten = FALSE, try to save header.");
		// pStreamInfo->initParams can be is not initialized yet
		AudioFormat format = GetAudioFormat(msg->codec_id);

		if(format == FLV_AudioFormat_None)
		{
			logE_ (_func, "Cannot detect audio format from stream.");
			// we must return "Success" instead of "Failure" else moment will be crashed.
			return Result::Success;
		}

		if(pStreamInfo->initParams.streamParams.audioParams.format != FLV_AudioFormat_None)
		{
			if(pStreamInfo->initParams.streamParams.audioParams.format != format)
			{
				logE_ (_func, "Input audio format(", (Int32)format, ") != initialized format(",
					(Int32)pStreamInfo->initParams.streamParams.audioParams.format, ")");
				// we must return "Success" instead of "Failure" else moment will be crashed.
				return Result::Success;
			}
		}
		else
		{
			// init and set audio format now
			pStreamInfo->initParams.streamParams.audioParams.format			= format;
			pStreamInfo->initParams.streamParams.audioParams.bitsPerSample	= 16;	// while hardcoded
			pStreamInfo->initParams.streamParams.audioParams.channels		= msg->channels;
			pStreamInfo->initParams.streamParams.audioParams.sampleRate		= msg->rate;
		}

		if(format == FLV_AudioFormat_AAC)
		{
			if(msg->frame_type == VideoStream::AudioFrameType::AacSequenceHeader)
			{
				Result res = SaveStreamHeader(ConstMemory(msg->page_list.first->getData(), msg->page_list.first->data_len), pStreamInfo);

				pStreamInfo->bHeaderIsWritten = true;	// res == Result::Success ???
				// we must return "Success" instead of "Failure" else moment will be crashed.
				return Result::Success;
			}

			logD_ (_func, "Skip raw data and wait header.");
			// skip another data while we wait header
			return Result::Success;
		}
		else
		{
			pStreamInfo->bHeaderIsWritten = true;
		}
	}
	else
	{
		if( msg->frame_type == VideoStream::AudioFrameType::AacSequenceHeader ||
			msg->frame_type == VideoStream::AudioFrameType::SpeexHeader)
		{
			logD_ (_func, "Skip this audio data(", msg->frame_type, "), it is not necessary now.");
			// we must return "Success" instead of "Failure" else moment will be crashed.
			return Result::Success;
		}

		for(Size uiCnt = 0; uiCnt < sizeof(m_StreamsInfo) / sizeof(m_StreamsInfo[0]); ++uiCnt)
		{
			if(m_StreamsInfo[uiCnt].initParams.streamType == FLV_StreamType_Video)
			{
				if(!m_StreamsInfo[uiCnt].bHeaderIsWritten)
				{
					logD_ (_func, "Skipped AUDIO data, wait video.");
					//return Result::Success;
					m_StreamsInfo[uiCnt].bHeaderIsWritten = true;
					break;
				}
			}
		}
	}

	return FlvWritePacket(msg, pStreamInfo);
}

Result CFlvFileMuxer::muxVideoMessage(VideoStream::VideoMessage * mt_nonnull msg)
{
	logD_(_func, "MsgType(", (Int32)msg->msg_type, ")Time(", (double)msg->timestamp_nanosec / 1000000000.0,
		")MsgSize(", msg->msg_len, ")MsgOffset(", msg->msg_offset, ")frame_type(",
		msg->frame_type, ")codec_id(", msg->codec_id, ")");

	// check data
	if(!msg->page_list.first || msg->page_list.first->data_len == 0)
	{
		logE_ (_func, "first(", msg->page_list.first ? "valid" : "invalid", " ptr)len(", msg->page_list.first ? msg->page_list.first->data_len : 0, ")");
		// we must return "Success" instead of "Failure" else moment will be crashed.
		return Result::Success;
	}

	switch(msg->frame_type)
	{
	case VideoStream::VideoFrameType::KeyFrame:
	case VideoStream::VideoFrameType::InterFrame:
	case VideoStream::VideoFrameType::DisposableInterFrame:
	case VideoStream::VideoFrameType::GeneratedKeyFrame:
	case VideoStream::VideoFrameType::CommandFrame:
	case VideoStream::VideoFrameType::AvcSequenceHeader:
	//case VideoStream::VideoFrameType::AvcEndOfSequence:
		break;
	default:
		logE_ (_func, "This frame type(", msg->frame_type, ") is not supported for FLV video.");
		// we must return "Success" instead of "Failure" else moment will be crashed.
		return Result::Success;
	}

	StreamInfo * pStreamInfo = NULL;

	for(Size uiCnt = 0; uiCnt < sizeof(m_StreamsInfo) / sizeof(m_StreamsInfo[0]); ++uiCnt)
	{
		if(m_StreamsInfo[uiCnt].initParams.streamType == FLV_StreamType_Video)
		{
			pStreamInfo = &m_StreamsInfo[uiCnt];
			break;
		}
	}

	if(!pStreamInfo)
	{
		logE_ (_func, "!pStreamInfo");
		return Result::Failure;
	}

	if(!pStreamInfo->bHeaderIsWritten)
	{
		logD_ (_func, "VIDEO, bHeaderIsWritten = FALSE, try to save header.");
		// pStreamInfo->initParams can be is not initialized yet
		VideoFormat format = GetVideoFormat(msg->codec_id);

		if(format == FLV_VideoFormat_None)
		{
			logE_ (_func, "Cannot detect video format from stream.");
			// we must return "Success" instead of "Failure" else moment will be crashed.
			return Result::Success;
		}

		if(pStreamInfo->initParams.streamParams.videoParams.format != FLV_VideoFormat_None)
		{
			if(pStreamInfo->initParams.streamParams.videoParams.format != format)
			{
				logE_ (_func, "Input video format(", (Int32)format, ") != initialized format(",
					(Int32)pStreamInfo->initParams.streamParams.videoParams.format, ")");
				// we must return "Success" instead of "Failure" else moment will be crashed.
				return Result::Success;
			}
		}
		else
		{
			// init and set video format now
			// set only format
			pStreamInfo->initParams.streamParams.videoParams.format = format;
		}

		if(format == FLV_VideoFormat_AVC)
		{
			if(msg->frame_type == VideoStream::VideoFrameType::AvcSequenceHeader)
			{
				Result res = SaveStreamHeader(ConstMemory(msg->page_list.first->getData(), msg->page_list.first->data_len), pStreamInfo);

				pStreamInfo->bHeaderIsWritten = true;	// res == Result::Success ???
				// we must return "Success" instead of "Failure" else moment will be crashed.
				return Result::Success;
			}

			logD_ (_func, "Skip raw data and wait header.");
			// skip another data while we wait header
			return Result::Success;
		}
		else
		{
			pStreamInfo->bHeaderIsWritten = true;
		}
	}
	else
	{
		if(VideoStream::VideoFrameType::AvcSequenceHeader == msg->frame_type)
		{
			logD_ (_func, "Skip this AVC header, it is not necessary more.");
			// we must return "Success" instead of "Failure" else moment will be crashed.
			return Result::Success;
		}

		// we must store a header, after it we can store another raw data
		for(Size uiCnt = 0; uiCnt < sizeof(m_StreamsInfo) / sizeof(m_StreamsInfo[0]); ++uiCnt)
		{
			if(m_StreamsInfo[uiCnt].initParams.streamType == FLV_StreamType_Audio)
			{
				if(!m_StreamsInfo[uiCnt].bHeaderIsWritten)
				{
					logD_ (_func, "Skipped VIDEO data, wait audio.");
					//return Result::Success;
					m_StreamsInfo[uiCnt].bHeaderIsWritten = true;
					break;
				}
			}
		}
	}

	return FlvWritePacket(msg, pStreamInfo, (msg->frame_type == VideoStream::VideoFrameType::KeyFrame));
}

Result CFlvFileMuxer::FlvWriteHeader(Int64 * piiDuration, Int64 * piiFileSize)
{
	logD_ (_func);

	Int32 iMetadataCount = 0;
	FileSize iMetaDataSizePos = 0;
	Int64 iiDataSize, iMetaDataCountPos;
	InitStreamParams::StreamParams::VideoParams * pVideoParams = NULL;
	InitStreamParams::StreamParams::AudioParams * pAudioParams = NULL;

	for(Uint32 uiCnt = 0; uiCnt < m_uiNumStreams; ++uiCnt)
	{
		if(!pVideoParams && m_StreamsInfo[uiCnt].initParams.streamType == FLV_StreamType_Video)
		{
			pVideoParams = &(m_StreamsInfo[uiCnt].initParams.streamParams.videoParams);

			logD_ (_func, "pVideoParams valid, format(", (Int32)pVideoParams->format, ")");
		}
		else if(!pAudioParams && m_StreamsInfo[uiCnt].initParams.streamType == FLV_StreamType_Audio)
		{
			pAudioParams = &(m_StreamsInfo[uiCnt].initParams.streamParams.audioParams);
			logD_ (_func, "pAudioParams valid, format(", (Int32)pAudioParams->format, ")Freq(", pAudioParams->sampleRate,
				")Chnn(", pAudioParams->channels, ")BPS(", pAudioParams->bitsPerSample, ")");
		}
	}

	if(!pVideoParams && !pAudioParams)
	{
		logE_ (_func, "!pVideoParams && !pAudioParams");
		return Result::Failure;
	}

	// set start position
	FileSeek(0, SeekOrigin::Beg);

    m_iiDelay = Int64_Max;
	// start writing of header data
	Byte FLVSignature[3] =
	{
		'F',			// Signature byte always 'F' (0x46)
		'L',			// Signature byte always 'L' (0x4C)
		'V',			// Signature byte always 'V' (0x56)
	};
    WriteDataInternal(ConstMemory(FLVSignature, sizeof(FLVSignature)));
    WriteB8Internal(1);
    WriteB8Internal(FLV_HEADER_FLAG_HASAUDIO * (!!pAudioParams) + FLV_HEADER_FLAG_HASVIDEO * (!!pVideoParams));
    WriteB32Internal(9);
	WriteB32Internal(0);

	/*
	// NOTE: we will not use this code to avoid a violation of header saving to file
	for(Uint32 uiCnt = 0; uiCnt < m_uiNumStreams; ++uiCnt)
	{
		bool bNeedWrite = false;
		const Int32 iMagicNumber = 5;	// do not ask me

		if(pStreamParam->streamType == FLV_StreamType_Video)
		{
			bNeedWrite = (GetVideoCodecTag(m_StreamsInfo[uiCnt].initParams.streamParams.videoParams.format) == iMagicNumber);
		}
		else if(pStreamParam->streamType == FLV_StreamType_Audio)
		{
			bNeedWrite = (GetAudioCodecTag(m_StreamsInfo[uiCnt].initParams.streamParams.audioParams.format) == iMagicNumber);
		}

		if(bNeedWrite)
		{
			WriteB8Internal(8);		// message type
			WriteB24Internal(0);	// include flags
			WriteB24Internal(0);	// time stamp
			WriteB32Internal(0);	// reserved
			WriteB32Internal(11);	// size

			m_StreamsInfo[uiCnt].reserved = iMagicNumber;
		}
	}
	*/
    // write meta_tag
    WriteB8Internal(18);		// tag type META
    iMetaDataSizePos = FileTell();
    WriteB24Internal(0);		// size of data part (sum of all parts below)
    WriteB24Internal(0);		// timestamp
    WriteB32Internal(0);		// reserved

    // now data of iiDataSize size

    // first event name as a string
    WriteB8Internal(AMF_DATA_TYPE_STRING);
    PutAmfString("onMetaData"); // 12 bytes

    // mixed array (hash) with size and string/type/data tuples
    WriteB8Internal(AMF_DATA_TYPE_MIXEDARRAY);
    iMetaDataCountPos = FileTell();
    iMetadataCount = 5 * (!!pVideoParams) +
                     5 * (!!pAudioParams) +
                     1 * 0  +	// for FLV_StreamType_Data, now it is not supported
                     2; // +2 for duration and file size

    WriteB32Internal(iMetadataCount);

    PutAmfString("duration");
    // fill in the guessed duration, it'll be corrected later if incorrect
    PutAmfDouble(piiDuration ? ((double)(*piiDuration) / 1000.0) : 0);

    if (pVideoParams)
	{
		// NOTE: we will not use this code to avoid a violation of header on saving to file

		//if(pVideoParams->width > 0 && pVideoParams->height > 0 && pVideoParams->framerate > 0)
		{
			//PutAmfString("width");
			//PutAmfDouble(640/*pVideoParams->width*/);

			//PutAmfString("height");
			//PutAmfDouble(480/*pVideoParams->height*/);

			//PutAmfString("videodatarate");
			//PutAmfDouble(0);	// TODO: we can calculate it

			//PutAmfString("framerate");
			//PutAmfDouble(30/*pVideoParams->framerate*/);
		}

        PutAmfString("videocodecid");
        PutAmfDouble(GetVideoCodecTag(pVideoParams->format));
    }

    if (pAudioParams)
	{
		// NOTE: we will not use this code to avoid a violation of header on saving to file

		//if(pAudioParams->sampleRate > 0 && pAudioParams->bitsPerSample > 0 && pAudioParams->channels > 0)
		{
			//PutAmfString("audiodatarate");
			//PutAmfDouble(0);	// TODO: we can calculate it

			PutAmfString("audiosamplerate");
			PutAmfDouble(pAudioParams->sampleRate);

			// we do not store "audiosamplesize", because we do not have precise information about bitsPerSample.
			//PutAmfString("audiosamplesize");
			//PutAmfDouble((pAudioParams->bitsPerSample == 8) ? 8 : 16);

			PutAmfString("stereo");
			PutAmfBool(pAudioParams->channels == 2);
		}

		PutAmfString("audiocodecid");
		PutAmfDouble(GetAudioCodecTag(pAudioParams->format));
    }

// 	PutAmfString("datastream");
// 	PutAmfDouble(0.0);

    PutAmfString("filesize");
    PutAmfDouble(piiFileSize ? (*piiFileSize) : NULL); // delayed write

    PutAmfString("");
    WriteB8Internal(AMF_END_OF_OBJECT);

    // write total size of tag
    iiDataSize = FileTell() - iMetaDataSizePos - 10;

    FileSeek(iMetaDataCountPos, SeekOrigin::Beg);
    WriteB32Internal(iMetadataCount);

	FileSeek(iMetaDataSizePos, SeekOrigin::Beg);
    WriteB24Internal(iiDataSize);
    FileSkip(iiDataSize + 10 - 3);
    WriteB32Internal(iiDataSize + 11);

	/*for(Uint32 uiCnt = 0; uiCnt < m_uiNumStreams; ++uiCnt)
	{
		if( (m_StreamsInfo[uiCnt].initParams.streamType == FLV_StreamType_Video &&
			(m_StreamsInfo[uiCnt].initParams.streamParams.videoParams.format == FLV_VideoFormat_AVC  ||
			m_StreamsInfo[uiCnt].initParams.streamParams.videoParams.format == FLV_VideoFormat_MPEG4 ))	 ||
			(m_StreamsInfo[uiCnt].initParams.streamType == FLV_StreamType_Audio &&
			(m_StreamsInfo[uiCnt].initParams.streamParams.audioParams.format == FLV_AudioFormat_AAC)))
		{
            Int64 pos;

            WriteB8Internal((m_StreamsInfo[uiCnt].initParams.streamType == FLV_StreamType_Video) ? FLV_TAG_TYPE_VIDEO : FLV_TAG_TYPE_AUDIO);
            WriteB24Internal(0); // size patched later
            WriteB24Internal(0); // ts
            WriteB8Internal(0);   // ts ext
            WriteB24Internal(0); // streamid

            pos = FileTell();

            if(m_StreamsInfo[uiCnt].initParams.streamType == FLV_StreamType_Audio)
			{
                WriteB8Internal(GetAudioFlags(s, enc));
                WriteB8Internal(0); // AAC sequence header
                WriteDataInternal(enc->extradata, enc->extradata_size);
            }
			else
			{
                WriteB8Internal(enc->codec_tag | FLV_FRAME_KEY); // flags
                WriteB8Internal(0); // AVC sequence header
                WriteB24Internal(0); // composition time
                IsomWriteAvcc(@@@@);
            }
			iiDataSize = FileTell() - pos;
			FileSeek(-iiDataSize - 10, SeekOrigin::Cur);
            WriteB24Internal(iiDataSize);
            FileSkip(iiDataSize + 10 - 3);
            WriteB32Internal(iiDataSize + 11); // previous tag size
        }
    }*/

    return Result::Success;
}

Result CFlvFileMuxer::FlvWriteEOS(void)
{
	logD_ (_func);

	// Add EOS tag
	for(Uint32 uiCnt = 0; uiCnt < m_uiNumStreams; ++uiCnt)
	{
		if( m_StreamsInfo[uiCnt].initParams.streamType == FLV_StreamType_Video &&
			(m_StreamsInfo[uiCnt].initParams.streamParams.videoParams.format == FLV_VideoFormat_AVC  ||
			 m_StreamsInfo[uiCnt].initParams.streamParams.videoParams.format == FLV_VideoFormat_MPEG4 )	&&
			 m_StreamsInfo[uiCnt].iiLastTimeStamp != Int64_Min)
		{
			PutAvcEosTag(m_StreamsInfo[uiCnt].iiLastTimeStamp);
		}
	}

	Int64 iiFileSize = FileTell();
	Int64 iDuration = m_iiDuration;
	// rewrite header
	FlvWriteHeader(&iDuration, &iiFileSize);
	// go to end of file.
    FileSeek(iiFileSize, SeekOrigin::Beg);

    return Result::Success;
}

Result CFlvFileMuxer::FlvWritePacket(VideoStream::Message * const mt_nonnull msg, StreamInfo * pStreamInfo, bool bKeyFrame)
{
    Int32 packetSize = PagePool::countPageListDataLen(msg->page_list.first, msg->msg_offset);
    Int32 flags = -1, flagsSize = 1;
	Byte * pbFirstMem = msg->page_list.first->getData();
	Int32 iFirstMemSize = msg->page_list.first->data_len - msg->msg_offset;
	Int64 const iiInputTimeStamp = msg->timestamp_nanosec / 1000000;
	FLVStreamType streamType = pStreamInfo->initParams.streamType;

    switch (streamType)
	{
	case FLV_StreamType_Video:
		{
			VideoFormat format = pStreamInfo->initParams.streamParams.videoParams.format;
			//logD_ (_func, "Video packet, format = ", (Int32)format, ", keyframe = ", bKeyFrame);

			WriteB8Internal(FLV_TAG_TYPE_VIDEO);

			flags = GetVideoCodecTag(format);
			flags |= bKeyFrame ? FLV_FRAME_KEY : FLV_FRAME_INTER;

			if( format == FLV_VideoFormat_AVC || format == FLV_VideoFormat_MPEG4)
			{
				flagsSize = 5;

				if(AvcParseNalUnits(ConstMemory(pbFirstMem, iFirstMemSize), NULL) < 0)
				{
					logE_ (_func, "AvcParseNalUnits fails");
					// we must return "Success" instead of "Failure" else moment will be crashed.
					return Result::Success;
				}
			}
			else if(format == FLV_VideoFormat_VP6 || format == FLV_VideoFormat_VP6Alpha)
			{
				flagsSize = 2;
			}
		}

        break;

	case FLV_StreamType_Audio:
		{
			AudioFormat format = pStreamInfo->initParams.streamParams.audioParams.format;
			//logD_ (_func, "Audio packet, format = ", (Int32)format);

			WriteB8Internal(FLV_TAG_TYPE_AUDIO);

			flags = GetAudioFlags(&pStreamInfo->initParams);

			if(format == FLV_AudioFormat_AAC)
			{
				if(!pStreamInfo->bHeaderIsWritten)
				{
					logE_ (_func, "AAC: !pStreamInfo->bHeaderIsWritten");
					// we must return "Success" instead of "Failure" else moment will be crashed.
					return Result::Success;
				}

				flagsSize = 2;

				if (iFirstMemSize > 2 && (AV_RB16(pbFirstMem) & 0xfff0) == 0xfff0)
				{
					logE_ (_func, "Malformed AAC bitstream detected: use audio bitstream filter 'aac_adtstoasc' to fix it");
					// we must return "Success" instead of "Failure" else moment will be crashed.
					return Result::Success;
				}
			}
		}

        break;

    default:
        return Result::Failure;
    }

    if (m_iiDelay == Int64_Max)
	{
		m_iiDelay = -iiInputTimeStamp;
		logD_ (_func, "First time stamp, m_iiDelay = ", m_iiDelay);
	}

    if (iiInputTimeStamp < -m_iiDelay)
	{
		logE_ (_func, "Packets are not in the proper order with respect to DTS");
		// we must return "Success" instead of "Failure" else moment will be crashed.
		return Result::Success;
    }

    Int64 const timeStamp = iiInputTimeStamp + m_iiDelay; // add delay to force positive dts

    if (pStreamInfo->iiLastTimeStamp == Int64_Min)
	{
		logD_ (_func, "Set last time stamp = ", timeStamp, ", for streamType = ", (Int32)streamType);
        pStreamInfo->iiLastTimeStamp = timeStamp;
	}

    WriteB24Internal(packetSize + flagsSize);
    WriteB24Internal(timeStamp);
    WriteB8Internal((timeStamp >> 24) & 0x7F); // timestamps are 32 bits _signed_
    WriteB24Internal(pStreamInfo->reserved);

	WriteB8Internal(flags);

	if(streamType == FLV_StreamType_Video)
	{
		VideoFormat format = pStreamInfo->initParams.streamParams.videoParams.format;

		if(format == FLV_VideoFormat_VP6 || format == FLV_VideoFormat_VP6Alpha)
		{
			WriteB8Internal(0);
		}
		else if(format == FLV_VideoFormat_AVC || format == FLV_VideoFormat_MPEG4)
		{
			WriteB8Internal(1); // AVC NALU
			WriteB24Internal(0 /*pts - dts*/);
		}
	}
	else if(streamType == FLV_StreamType_Audio)
	{
		if(pStreamInfo->initParams.streamParams.audioParams.format == FLV_AudioFormat_AAC)
		{
			WriteB8Internal(1); // AAC raw
		}
	}

	Size written = 0;
	PagePool::Page * pPage = msg->page_list.first;

	for(Size uiOffset = msg->msg_offset; pPage; uiOffset = 0)
	{
		WriteDataInternal(ConstMemory(pPage->getData() + uiOffset, pPage->data_len - uiOffset));

		written += pPage->data_len - uiOffset;

		pPage = pPage->getNextMsgPage();
	}

	//logD_ (_func, "packetSize = ", packetSize, ", written = ", written);

	WriteB32Internal(packetSize + flagsSize + 11); // previous tag size

	//m_iiDuration = std::max(m_iiDuration, pkt->pts + m_iiDelay + pkt->duration);
	if(timeStamp > m_iiDuration)
		m_iiDuration = timeStamp;	// TODO: need add a duration of frame

    return Result::Success;
}

Result CFlvFileMuxer::SaveStreamHeader(ConstMemory const memory, StreamInfo * pStreamInfo)
{
	logD_ (_func);

	if( (pStreamInfo->initParams.streamType == FLV_StreamType_Video &&
		(pStreamInfo->initParams.streamParams.videoParams.format == FLV_VideoFormat_AVC  ||
		pStreamInfo->initParams.streamParams.videoParams.format == FLV_VideoFormat_MPEG4 ))	 ||
		(pStreamInfo->initParams.streamType == FLV_StreamType_Audio &&
		(pStreamInfo->initParams.streamParams.audioParams.format == FLV_AudioFormat_AAC)))
	{
		WriteB8Internal((pStreamInfo->initParams.streamType == FLV_StreamType_Video) ? FLV_TAG_TYPE_VIDEO : FLV_TAG_TYPE_AUDIO);
		WriteB24Internal(0); // size patched later
		WriteB24Internal(0); // ts
		WriteB8Internal(0);   // ts ext
		WriteB24Internal(0); // streamid

		Int64 pos = FileTell();

		if(pStreamInfo->initParams.streamType == FLV_StreamType_Audio)
		{
			logD_ (_func, "AAC header to file");
			WriteB8Internal(GetAudioFlags(&pStreamInfo->initParams));
			WriteB8Internal(0); // AAC sequence header
			WriteDataInternal(memory);
		}
		else
		{
			WriteB8Internal(GetVideoCodecTag(pStreamInfo->initParams.streamParams.videoParams.format) | FLV_FRAME_KEY); // flags
			WriteB8Internal(0); // AVC sequence header
			WriteB24Internal(0); // composition time
			logD_ (_func, "AVC header to file");
			IsomWriteAvcc(memory);
		}

		Int64 iiDataSize = FileTell() - pos;

		FileSeek(-iiDataSize - 10, SeekOrigin::Cur);
		WriteB24Internal(iiDataSize);
		FileSkip(iiDataSize + 10 - 3);
		WriteB32Internal(iiDataSize + 11); // previous tag size

		return Result::Success;
	}
	else
	{
		logE_ (_func, "Fail, streamType(", (Int32)pStreamInfo->initParams.streamType, ") format (", (Int32)pStreamInfo->initParams.streamParams.videoParams.format, ")");
	}

	return Result::Failure;
}

Int64 CFlvFileMuxer::FileTell(void)
{
	FileSize pos = 0;
	m_File->getFile()->tell(&pos);

	return (pos + m_OutMemory.size());
}

Result CFlvFileMuxer::FileSeek(FileOffset offset, SeekOrigin origin)
{
	logD_ (_func);
	FlushBufferInternal();

	return m_File->getFile()->seek(offset, origin);
}

Result CFlvFileMuxer::FileSkip(FileOffset offset)
{
	return FileSeek(offset, SeekOrigin::Cur);
}

void CFlvFileMuxer::FlushBufferInternal(void)
{
	if(m_OutMemory.size() > 0)
	{
		logD_ (_func, "FlushBufferInternal, size = ", m_OutMemory.size());
		m_File->getFile()->write(ConstMemory(m_OutMemory.mem(), m_OutMemory.size()), NULL);
		m_OutMemory.setSize(0);
	}
}

void CFlvFileMuxer::PutAmfString(ConstMemory const str)
{
	WriteB16Internal(str.len());
	WriteDataInternal(str);
}

void CFlvFileMuxer::PutAmfDouble(double d)
{
	WriteB8Internal(AMF_DATA_TYPE_NUMBER);
	WriteB64Internal(double2int(d));
}

void CFlvFileMuxer::PutAmfBool(Int32 b)
{
	WriteB8Internal(AMF_DATA_TYPE_BOOL);
	WriteB8Internal(!!b);
}

void CFlvFileMuxer::WriteDataInternal(ConstMemory const memory)
{
	//m_File->getFile()->write(memory, NULL);
	Size inputSize = memory.len();

	if(m_OutMemory.size() + inputSize >= m_OutMemory.len())
	{
		FlushBufferInternal();
	}

	if(inputSize >= m_OutMemory.len())
	{
		m_File->getFile()->write(memory, NULL);
	}
	else if(inputSize > 0)
	{
		memcpy(m_OutMemory.mem() + m_OutMemory.size(), memory.mem(), inputSize);

		m_OutMemory.setSize(m_OutMemory.size() + inputSize);
	}
}

void CFlvFileMuxer::WriteB8Internal(Int32 b)
{
// 	Byte byte = (Byte)b;
// 	ConstMemory mem(&byte, 1);
// 	m_File->getFile()->write(mem, NULL);
	Size curPos = m_OutMemory.size();
	Byte * pMem = m_OutMemory.mem();
	pMem[curPos] = (Byte)b;

	m_OutMemory.setSize(curPos + 1);

	if(m_OutMemory.size() >= m_OutMemory.len())
	{
		FlushBufferInternal();
	}
}

void CFlvFileMuxer::WriteB16Internal(Uint32 val)
{
	WriteB8Internal((Int32)val >> 8);
	WriteB8Internal((Byte)val);
}

void CFlvFileMuxer::WriteB24Internal(Uint32 val)
{
	WriteB16Internal((Int32)val >> 8);
	WriteB8Internal((Byte)val);
}

void CFlvFileMuxer::WriteB32Internal(Uint32 val)
{
	WriteB8Internal(       val >> 24 );
	WriteB8Internal((Byte)(val >> 16));
	WriteB8Internal((Byte)(val >> 8 ));
	WriteB8Internal((Byte) val       );
}

void CFlvFileMuxer::WriteB64Internal(Uint64 val)
{
	WriteB32Internal((Uint32)(val >> 32));
	WriteB32Internal((Uint32)(val & 0xffffffff));
}

Result CFlvFileMuxer::WriteB8ToBuffer(Int32 b, MemoryEx & memory)
{
	if(memory.size() >= memory.len())
		return Result::Failure;

	Size curPos = memory.size();
	Byte * pMem = memory.mem();
	pMem[curPos] = (Byte)b;

	memory.setSize(curPos + 1);

	if(memory.size() >= memory.len())
	{
		logE_ (_func, "Fail, size(", memory.size(), ") > memoryOut.len(", memory.len(), ")");
		return Result::Failure;
	}

	return Result::Success;
}

Result CFlvFileMuxer::WriteB32ToBuffer(Uint32 val, MemoryEx & memory)
{
	if( WriteB8ToBuffer(       val >> 24 , memory) == Result::Success &&
		WriteB8ToBuffer((Byte)(val >> 16), memory) == Result::Success &&
		WriteB8ToBuffer((Byte)(val >> 8 ), memory) == Result::Success &&
		WriteB8ToBuffer((Byte) val       , memory) == Result::Success)
	{
		return Result::Success;
	}

	return Result::Failure;
}

Result CFlvFileMuxer::WriteDataToBuffer(ConstMemory const memory, MemoryEx & memoryOut)
{
	Size inputSize = memory.len();

	if(memoryOut.size() + inputSize >= memoryOut.len())
	{
		logE_ (_func, "Fail, size(", memoryOut.size(), ") + inputSize(", inputSize, ") > memoryOut.len(", memoryOut.len(), ")");
		return Result::Failure;
	}

	if(inputSize >= memoryOut.len())
	{
		logE_ (_func, "inputSize(", inputSize, ") >= memoryOut.len(", memoryOut.len(), ")");
		return Result::Failure;
	}
	else if(inputSize > 0)
	{
		memcpy(memoryOut.mem() + memoryOut.size(), memory.mem(), inputSize);

		memoryOut.setSize(memoryOut.size() + inputSize);
	}

	return Result::Success;
}

void CFlvFileMuxer::PutAvcEosTag(Uint32 ts)
{
	logD_ (_func);

	WriteB8Internal(FLV_TAG_TYPE_VIDEO);
	WriteB24Internal(5);				// Tag Data Size
	WriteB24Internal(ts);				// lower 24 bits of timestamp in ms
	WriteB8Internal((ts >> 24) & 0x7F);	// MSB of ts in ms
	WriteB24Internal(0);				// StreamId = 0
	WriteB8Internal(23);				// ub[4] FrameType = 1, ub[4] CodecId = 7
	WriteB8Internal(2);					// AVC end of sequence
	WriteB24Internal(0);				// Always 0 for AVC EOS.
	WriteB32Internal(16);				// Size of FLV tag
}

Int32 CFlvFileMuxer::AvcParseNalUnits(ConstMemory const mem, MemoryEx * pMemoryOut)
{
	//logD_ (_func);

	const Byte *p = mem.mem();
	const Byte *end = p + mem.len();
	const Byte *nal_start, *nal_end;

	Int32 size = 0;

	nal_start = AvcFindStartCode(p, end);

	for (;;)
	{
		while (nal_start < end && !*(nal_start++));

		if (nal_start == end)
			break;

		nal_end = AvcFindStartCode(nal_start, end);

		if(pMemoryOut)
		{
			WriteB32ToBuffer(nal_end - nal_start, *pMemoryOut);
			WriteDataToBuffer(ConstMemory(nal_start, nal_end - nal_start), *pMemoryOut);
		}
		else
		{
			WriteB32Internal(nal_end - nal_start);
			WriteDataInternal(ConstMemory(nal_start, nal_end - nal_start));
		}

		size += 4 + nal_end - nal_start;
		nal_start = nal_end;
	}

	//logD_ (_func, "Returned size = ", size);

	return size;
}

Result CFlvFileMuxer::IsomWriteAvcc(ConstMemory const memory)
{
	logD_ (_func, "IsomWriteAvcc = ", memory.len());

	if (memory.len() > 6)
	{
		Byte const * data = memory.mem();

		// check for h264 start code
		if (AV_RB32(data) == 0x00000001 || AV_RB24(data) == 0x000001)
		{
			MemorySafe allocMemory(memory.len() * 2);
			MemoryEx localMemory((Byte *)allocMemory.cstr(), allocMemory.len());

			if(AvcParseNalUnits(memory, &localMemory) < 0)
			{
				logE_ (_func, "AvcParseNalUnits fails");
				return Result::Failure;
			}

			Byte * buf = localMemory.mem();
			Byte * end = buf + localMemory.size();
			Uint32 sps_size = 0, pps_size = 0;
			Byte * sps = 0, *pps = 0;

			// look for sps and pps
			while (end - buf > 4)
			{
				Uint32 size;
				Byte nal_type;

				size = std::min((Uint32)AV_RB32(buf), (Uint32)(end - buf - 4));

				buf += 4;
				nal_type = buf[0] & 0x1f;

				if (nal_type == 7)
				{
					// SPS
					sps = buf;
					sps_size = size;
				}
				else if (nal_type == 8)
				{
					// PPS
					pps = buf;
					pps_size = size;
				}

				buf += size;
			}

			logD_(_func, "sps = ", sps ? "valid" : "invalid", ", pps = ", pps ? "valid" : "invalid", ", sps_size = ", sps_size, ", pps_size = ", pps_size);

			if (!sps || !pps || sps_size < 4 || sps_size > Uint16_Max || pps_size > Uint16_Max)
			{
				return Result::Failure;
			}

			WriteB8Internal(1);			// version
			WriteB8Internal(sps[1]);	// profile
			WriteB8Internal(sps[2]);	// profile compat
			WriteB8Internal(sps[3]);	// level
			WriteB8Internal(0xff);		// 6 bits reserved (111111) + 2 bits nal size length - 1 (11)
			WriteB8Internal(0xe1);		// 3 bits reserved (111) + 5 bits number of sps (00001)

			WriteB16Internal(sps_size);
			WriteDataInternal(ConstMemory(sps, sps_size));
			WriteB8Internal(1);			// number of pps
			WriteB16Internal(pps_size);
			WriteDataInternal(ConstMemory(pps, pps_size));
		}
		else
		{
			logD_ (_func, "AVC header, ONLY WriteDataInternal, ", data[0], ", ", data[1], ", ", data[2], ", ", data[3], ", ");
			WriteDataInternal(memory);
		}
	}

	return Result::Success;
}

VideoFormat CFlvFileMuxer::GetVideoFormat(VideoStream::VideoCodecId codecId)
{
	switch(codecId)
	{
	case VideoStream::VideoCodecId::SorensonH263:	return FLV_VideoFormat_H263;
	case VideoStream::VideoCodecId::ScreenVideo:	return FLV_VideoFormat_Screen;
	case VideoStream::VideoCodecId::ScreenVideoV2:	return FLV_VideoFormat_ScreenV2;
	case VideoStream::VideoCodecId::VP6:			return FLV_VideoFormat_VP6;
	case VideoStream::VideoCodecId::VP6Alpha:		return FLV_VideoFormat_VP6Alpha;
	case VideoStream::VideoCodecId::AVC:			return FLV_VideoFormat_AVC;
	}

	return FLV_VideoFormat_None;
}


AudioFormat CFlvFileMuxer::GetAudioFormat(VideoStream::AudioCodecId codecId)
{
	switch(codecId)
	{
		case VideoStream::AudioCodecId::LinearPcmPlatformEndian:	return FLV_AudioFormat_LPCM;
	    case VideoStream::AudioCodecId::ADPCM:						return FLV_AudioFormat_ADPCM;
	    case VideoStream::AudioCodecId::MP3:						return FLV_AudioFormat_MP3;
	    case VideoStream::AudioCodecId::LinearPcmLittleEndian:		return FLV_AudioFormat_LPCM_le;
	    case VideoStream::AudioCodecId::Nellymoser_16kHz_mono:		return FLV_AudioFormat_Nellymoser16;
	    case VideoStream::AudioCodecId::Nellymoser_8kHz_mono:		return FLV_AudioFormat_Nellymoser8;
	    case VideoStream::AudioCodecId::Nellymoser:					return FLV_AudioFormat_Nellymoser;
	    case VideoStream::AudioCodecId::G711ALaw:					return FLV_AudioFormat_G711_A;
	    case VideoStream::AudioCodecId::G711MuLaw:					return FLV_AudioFormat_G711_mu;
		case VideoStream::AudioCodecId::AAC:						return FLV_AudioFormat_AAC;
	    case VideoStream::AudioCodecId::Speex:						return FLV_AudioFormat_Speex;
	    case VideoStream::AudioCodecId::MP3_8kHz:					return FLV_AudioFormat_MP3_8;
	    case VideoStream::AudioCodecId::DeviceSpecific:				return FLV_AudioFormat_DSS;
	}

	return FLV_AudioFormat_None;
}

}	// namespace FLV

}	// namespace MomentNvr
