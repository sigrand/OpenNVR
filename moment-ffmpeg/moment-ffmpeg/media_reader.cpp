/*  Moment Video Server - High performance media server
    Copyright (C) 2013 Dmitry Shatrov
    e-mail: shatrov@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <moment-ffmpeg/media_reader.h>
#include <moment-ffmpeg/time_checker.h>
#include <moment-ffmpeg/naming_scheme.h>
#include <string>


using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

static LogGroup libMary_logGroup_reader ("mod_ffmpeg.media_reader", LogLevel::D);

#define AV_RB32(x)									\
        (((Uint32)((const Byte*)(x))[0] << 24) |	\
        (((const Byte*)(x))[1] << 16) |				\
        (((const Byte*)(x))[2] <<  8) |				\
        ( (const Byte*)(x))[3])

#define AV_RB24(x)									\
        ((((const Byte*)(x))[0] << 16) |			\
        ( ((const Byte*)(x))[1] <<  8) |			\
        (  (const Byte*)(x))[2])

StateMutex FileReader::m_mutexFFmpeg;

static void RegisterFFMpeg(void)
{
    static Uint32 uiInitialized = 0;

    if(uiInitialized != 0)
        return;

    uiInitialized = 1;

    // global ffmpeg initialization
    av_register_all();
    avformat_network_init();
}

int FileReader::WriteB8ToBuffer(Int32 b, MemoryEx & memory)
{
    if(memory.size() >= memory.len())
        return -1;

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

int FileReader::WriteB16ToBuffer(Uint32 val, MemoryEx & memory)
{
    if( WriteB8ToBuffer((Byte)(val >> 8 ), memory) == Result::Success &&
        WriteB8ToBuffer((Byte) val       , memory) == Result::Success)
    {
        return Result::Success;
    }

    return Result::Failure;
}

int FileReader::WriteB32ToBuffer(Uint32 val, MemoryEx & memory)
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

int FileReader::WriteDataToBuffer(ConstMemory const memory, MemoryEx & memoryOut)
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

int FileReader::AvcParseNalUnits(ConstMemory const mem, MemoryEx * pMemoryOut)
{
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

        size += 4 + nal_end - nal_start;
        nal_start = nal_end;
    }

    //logD (reader, _func, "Returned size = ", size);

    return size;
}

int FileReader::IsomWriteAvcc(ConstMemory const memory, MemoryEx & memoryOut)
{
    //logD (reader, _func, "IsomWriteAvcc = ", memory.len());

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

            //logD (reader, _func, "sps = ", sps ? "valid" : "invalid", ", pps = ", pps ? "valid" : "invalid", ", sps_size = ", sps_size, ", pps_size = ", pps_size);

            if (!sps || !pps || sps_size < 4 || sps_size > Uint16_Max || pps_size > Uint16_Max)
            {
                logE_ (_func, "Failed to get sps, pps");
                return Result::Failure;
            }

            WriteB8ToBuffer(1, memoryOut);			// version
            WriteB8ToBuffer(sps[1], memoryOut);	// profile
            WriteB8ToBuffer(sps[2], memoryOut);	// profile compat
            WriteB8ToBuffer(sps[3], memoryOut);	// level
            WriteB8ToBuffer(0xff, memoryOut);		// 6 bits reserved (111111) + 2 bits nal size length - 1 (11)
            WriteB8ToBuffer(0xe1, memoryOut);		// 3 bits reserved (111) + 5 bits number of sps (00001)

            WriteB16ToBuffer(sps_size, memoryOut);
            WriteDataToBuffer(ConstMemory(sps, sps_size), memoryOut);
            WriteB8ToBuffer(1, memoryOut);			// number of pps
            WriteB16ToBuffer(pps_size, memoryOut);
            WriteDataToBuffer(ConstMemory(pps, pps_size), memoryOut);
        }
        else
        {
            //logD (reader, _func, "AVC header, ONLY WriteDataInternal, ", data[0], ", ", data[1], ", ", data[2], ", ", data[3], ", ");
            WriteDataToBuffer(memory, memoryOut);
        }
    }

    return Result::Success;
}

void FileReader::CloseCodecs(AVFormatContext * pAVFrmtCntxt)
{
    if(!pAVFrmtCntxt)
        return;

    for(unsigned int uiCnt = 0; uiCnt < pAVFrmtCntxt->nb_streams; ++uiCnt)
    {
        // close each AVCodec
        if(pAVFrmtCntxt->streams[uiCnt])
        {
            AVStream * pAVStream = pAVFrmtCntxt->streams[uiCnt];

            if(pAVStream->codec)
                avcodec_close(pAVStream->codec);
        }
    }
}

bool FileReader::IsInit()
{
    return (format_ctx != NULL);
}

bool FileReader::Init(StRef<String> & fileName)
{
    DeInit();

    m_fileName = st_grab (new (std::nothrow) String (fileName->mem()));
    logD_(_func_, "m_fileName = ", m_fileName);

    const char * file_name_path = m_fileName->cstr();

    Result res = Result::Success;
    if(avformat_open_input(&format_ctx, file_name_path, NULL, NULL) == 0)
    {
        // Retrieve stream information
        m_mutexFFmpeg.lock();
        if(avformat_find_stream_info(format_ctx, NULL) >= 0)
        {
            // Dump information about file onto standard error
            av_dump_format(format_ctx, 0, file_name_path, 0);
        }
        else
        {
            logE_(_func_,"Fail to retrieve stream info");
            res = Result::Failure;
        }
        m_mutexFFmpeg.unlock();
    }
    else
    {
        logE_(_func_,"Fail to open file: ", m_fileName);
        res = Result::Failure;
    }

    if(res == Result::Success)
    {
        // Find the first video stream
        for(int i = 0; i < format_ctx->nb_streams; i++)
        {
            if(format_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
            {
                video_stream_idx = i;
                break;
            }
        }

        if(video_stream_idx == -1)
        {
            logE_(_func_,"ffmpegStreamData::Init, Didn't find a video stream");
            res = Result::Failure;
        }
    }
    else
    {
        logE_(_func_,"ffmpegStreamData::Init, Didn't find a video stream");
    }

    if(res == Result::Success)
    {
        m_dDuration = (double)(format_ctx->duration) / AV_TIME_BASE;
        logD_(_func_,"m_dDuration: ", m_dDuration);
        res = (m_dDuration >= 0.0)? Result::Success: Result::Failure;
    }

    if(res == Result::Success)
    {
        res = FileNameToUnixTimeStamp().Convert(m_fileName, m_initTimeOfRecord);
    }

    if(res != Result::Success)
    {
        logE_(_func_, "init failed, run deinit");
        DeInit();
    }

    logD_(_func_, "init succeeded");

    return (res == Result::Success);
}

void FileReader::DeInit()
{
    if(!avc_codec_data.isNull())
    {
        // free memory
        avc_codec_data.allocate(0);
    }

    if(format_ctx)
    {
        CloseCodecs(format_ctx);

        avformat_close_input(&format_ctx);

        format_ctx = NULL;
    }

    m_fileName = NULL;

    m_initTimeOfRecord = 0;

    video_stream_idx = -1;

    m_dDuration = -1.0;

    first_key_frame_received = false;
}

FileReader::FileReader()
{
    RegisterFFMpeg();

    format_ctx = NULL;

    m_initTimeOfRecord = 0;

    video_stream_idx = -1;

    m_dDuration = -1.0;

    first_key_frame_received = false;
}

FileReader::~FileReader()
{
    DeInit();
}

bool FileReader::ReadFrame(Frame & readframe)
{
    TimeChecker tc;tc.Start();

    if(!IsInit())
    {
        logE_(_func_, "IsInit returns false");
        return false;
    }

    AVPacket packet = {};

    while(true)
    {
        AVPacket readpacket = {};
        int err = 0;

        if((err = av_read_frame(format_ctx, &readpacket)) < 0)
        {
            if((size_t)err == AVERROR(EAGAIN))
            {
                 logD (reader, _func, "av_read_frame, err == AVERROR(EAGAIN), err = ", err);
                 usleep(100000);
                 continue;
            }
            else
            {
                char errmsg[4096] = {};
                av_strerror (err, errmsg, 4096);
                logE_(_func_, "Error while av_read_frame: ", errmsg);
                return false;
            }
        }

        // let it be just video for now
        if(readpacket.stream_index == video_stream_idx)
        {
            if(!first_key_frame_received)
            {
                if(readpacket.flags & AV_PKT_FLAG_KEY)
                {
                    first_key_frame_received = true;
                    logD (reader, _func, "first_key_frame_received");
                }
                else
                {
                    logD (reader, _func, "SKIPPED PACKET");
                    av_free_packet(&readpacket);
                    // we skip first frames until we get key frame to avoid corrupted playing
                    continue;
                }
            }

            packet = readpacket;

            break;
        }
    }   // while(1)

    AVCodecContext * pctx = format_ctx->streams[video_stream_idx]->codec;

    if(pctx->extradata_size > 0)
    {
        MemorySafe allocMemory(pctx->extradata_size * 2);
        MemoryEx memEx((Byte *)allocMemory.cstr(), allocMemory.len());
        memset(memEx.mem(), 0, memEx.len());
        bool bEmptyHeader = false;

        IsomWriteAvcc(ConstMemory(pctx->extradata, pctx->extradata_size), memEx);

        if(memEx.len())
        {
            if (avc_codec_data.len() && // we have old header data
                memEx.len() == avc_codec_data.len() &&
                !memcmp(memEx.mem(), avc_codec_data.cstr(), avc_codec_data.len()))
            {
                readframe.header = ConstMemory();
                bEmptyHeader = true;
            }
        }
        else
        {
            bEmptyHeader = true;
        }

        if(!bEmptyHeader)
        {
            logD_ (_func, "Codec data has changed");
            avc_codec_data.allocate(memEx.len());

            memcpy(avc_codec_data.cstr(), memEx.mem(), memEx.len());
            avc_codec_data.setLength(memEx.len());
            readframe.header = ConstMemory(avc_codec_data.cstr(), avc_codec_data.len());
        }
    }

    // retrive frame
    readframe.frame = ConstMemory(packet.data, packet.size);
    readframe.bKeyFrame = (packet.flags & AV_PKT_FLAG_KEY);
    readframe.timestamp_nanosec = m_initTimeOfRecord + packet.pts / (double)format_ctx->streams[video_stream_idx]->time_base.den * 1000000000LL;
    readframe.src_packet = packet;

    Time t;tc.Stop(&t);
    logD_(_func_, "FileReader.ReadFrame exectime = [", t, "]");

    return true;
}

void FileReader::FreeFrame(Frame & readframe)
{
    av_free_packet(&readframe.src_packet);
    memset(&readframe.src_packet, 0, sizeof(readframe.src_packet));
}

bool FileReader::Seek(double dSeconds)
{
    TimeChecker tc;tc.Start();

    if(!IsInit())
    {
        logD_(_func_, "IsInit returns false");
        return false;
    }

    int64_t llTimeStamp = (int64_t)(dSeconds * AV_TIME_BASE);
    int res = avformat_seek_file(format_ctx, -1, Int64_Min, llTimeStamp, Int64_Max, 0);

    // alter variant: avformat_seek_file(ic, -1, INT64_MIN, llTimeStamp, llTimeStamp, 0);

    Time t;tc.Stop(&t);
    logD_(_func_, "FileReader.Seek exectime = [", t, "]");

    return (res >= 0);
}

// end of FileReader implementation

mt_mutex (mutex) bool
MediaReader::tryOpenNextFile ()
{
    TimeChecker tc;tc.Start();

    StRef<String> const filename = file_iter.getNext ();
    if (!filename) {
        logD (reader, _func, "filename is null");
        return false;
    }

    cur_filename = st_makeString(record_dir, "/", filename, ".flv");

    logD (reader, _func, "new filename: ", cur_filename);

    if(!m_fileReader.Init(cur_filename))
    {
        logE_(_func_,"m_fileReader.Init failed");
        return false;
    }

    Time unix_time_stamp = 0;

    if(FileNameToUnixTimeStamp().Convert(cur_filename, unix_time_stamp) == Result::Success)
    {
        unix_time_stamp /= 1000000000LL;
        if(unix_time_stamp < start_unixtime_sec)
        {
            logD (reader, _func,"unix_time_stamp(", unix_time_stamp, ") < start_unixtime_sec (", start_unixtime_sec, ")");
            double dNumSeconds = (start_unixtime_sec - unix_time_stamp);
            logD (reader, _func,"we will skip ", dNumSeconds, "seconds");
            // DISCUSSION: may be we must get duration from m_fileReader and if it less than dNumSeconds
            // call tryOpenNextFile again (recursively).

            if(!m_fileReader.Seek(dNumSeconds))
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }

    Time t;tc.Stop(&t);
    logD_(_func_, "MediaReader.tryOpenNextFile exectime = [", t, "]");

    return true;
}

mt_mutex (mutex) MediaReader::ReadFrameResult
MediaReader::sendFrame (const FileReader::Frame & inputFrame,
                        ReadFrameBackend const * const read_frame_cb,
                        void                   * const read_frame_cb_data)
{
    TimeChecker tc;tc.Start();

    ReadFrameResult client_res = ReadFrameResult_Success;
    VideoStream::VideoCodecId video_codec_id = VideoStream::VideoCodecId::AVC;

    if (inputFrame.header.len())
    {
        Size msg_len = 0;

        PagePool::PageListHead page_list;

        page_pool->getFillPages (&page_list,
                                 inputFrame.header);
        msg_len += inputFrame.header.len();

        VideoStream::VideoMessage msg;
        msg.timestamp_nanosec = inputFrame.timestamp_nanosec;
        msg.prechunk_size = 0;
        msg.frame_type = VideoStream::VideoFrameType::AvcSequenceHeader;
        msg.codec_id = video_codec_id;

        msg.page_pool = page_pool;
        msg.page_list = page_list;
        msg.msg_len = msg_len;
        msg.msg_offset = 0;
        msg.is_saved_frame = false;

        client_res = read_frame_cb->videoFrame (&msg, read_frame_cb_data);

        page_pool->msgUnref (page_list.first);
    }

    VideoStream::VideoMessage msg;
    msg.frame_type = VideoStream::VideoFrameType::InterFrame;
    msg.codec_id = video_codec_id;

    if (inputFrame.bKeyFrame)
    {
        msg.frame_type = VideoStream::VideoFrameType::KeyFrame;
    }

    PagePool::PageListHead page_list;

    page_pool->getFillPages (&page_list,
                             inputFrame.frame);

    Size msg_len = inputFrame.frame.len();
    msg.timestamp_nanosec = inputFrame.timestamp_nanosec;

    msg.prechunk_size = 0;

    msg.page_pool = page_pool;
    msg.page_list = page_list;
    msg.msg_len = msg_len;
    msg.msg_offset = 0;
    msg.is_saved_frame = false;

    client_res = read_frame_cb->videoFrame (&msg, read_frame_cb_data);

    page_pool->msgUnref (page_list.first);

    Time t;tc.Stop(&t);
    logD_(_func_, "MediaReader.sendFrame exectime = [", t, "]");

    if (client_res != ReadFrameResult_Success)
    {
        logD (reader, _func, "return burst");
        return client_res;
    }

    return ReadFrameResult_Success;
}

MediaReader::ReadFrameResult
MediaReader::readMoreData (ReadFrameBackend const * const read_frame_cb,
                           void                   * const read_frame_cb_data)
{
    logD_(_func_);
    TimeChecker tc;tc.Start();

    ReadFrameResult rf_res = ReadFrameResult_Success;

    if (!m_fileReader.IsInit())
    {
        if (!tryOpenNextFile ())
        {
            return ReadFrameResult_NoData;
        }
    }

    // read packets from file
    while (1)
    {
        Result res = Result::Failure;
        FileReader::Frame frame;

        if(m_fileReader.ReadFrame(frame))
        {
            rf_res = sendFrame (frame, read_frame_cb, read_frame_cb_data);
            if (rf_res == ReadFrameResult_BurstLimit) {
                logD (reader, _func, "session 0x", fmt_hex, (UintPtr) this, ": BurstLimit, ", "Filename: ", m_fileReader.GetFilename());
                break;
            } else
            if (rf_res == ReadFrameResult_Finish) {
                break;
            } else
            if (rf_res == ReadFrameResult_Success) {
                res = Result::Success;
            } else {
                res = Result::Failure;
            }

            m_fileReader.FreeFrame(frame);
        }
        else
        {
            if (!tryOpenNextFile ())
            {
                rf_res = ReadFrameResult_NoData;
                break;
            }
        }
    }

    Time t;tc.Stop(&t);
    logD_(_func_, "MediaReader.readMoreData exectime = [", t, "]");

    return rf_res;
}

mt_const void
MediaReader::init (PagePool    * const mt_nonnull page_pool,
                   Vfs         * const mt_nonnull vfs,
                   ConstMemory   const stream_name,
                   Time          const start_unixtime_sec,
                   Size          const burst_size_limit,
                   StRef<String> const record_dir)
{
    this->page_pool = page_pool;
    this->vfs = vfs;
    this->start_unixtime_sec = start_unixtime_sec;
    this->burst_size_limit = burst_size_limit;
    this->record_dir = record_dir;

    logD_(_func_, "start_unixtime_sec = ", this->start_unixtime_sec);
    logD_(_func_, "burst_size_limit = ", this->burst_size_limit);
    logD_(_func_, "record_dir = ", this->record_dir);

    logD (reader, _func, "stream_name: ", (const char *)stream_name.mem());
    file_iter.init (vfs, stream_name, start_unixtime_sec);
}

mt_mutex (mutex) void
MediaReader::releaseSequenceHeaders_unlocked ()
{
    if (got_aac_seq_hdr) {
        page_pool->msgUnref (aac_seq_hdr.first);
        got_aac_seq_hdr = false;
    }

    if (got_avc_seq_hdr) {
        page_pool->msgUnref (avc_seq_hdr.first);
        got_avc_seq_hdr = false;
    }
}

MediaReader::~MediaReader ()
{
    mutex.lock ();
    releaseSequenceHeaders_unlocked ();
    mutex.unlock ();

}

}

