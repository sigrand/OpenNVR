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


#ifndef MOMENT_NVR__MEDIA_READER__H__
#define MOMENT_NVR__MEDIA_READER__H__


#include <moment-nvr/types.h>
#include <moment-nvr/nvr_file_iterator.h>

extern "C" {
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}


namespace MomentNvr {

using namespace M;
using namespace Moment;

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

class FileReader
{

private:

    typedef String MemorySafe;

    AVFormatContext *   format_ctx;
    int                 video_stream_idx;
    MemorySafe          avc_codec_data;
    bool                first_key_frame_received;

    StRef<String> m_fileName;
    Time m_initTimeOfRecord;
    double m_dDuration;

    int WriteB8ToBuffer(Int32 b, MemoryEx & memory);
    int WriteB16ToBuffer(Uint32 val, MemoryEx & memory);
    int WriteB32ToBuffer(Uint32 val, MemoryEx & memory);
    int WriteDataToBuffer(ConstMemory const memory, MemoryEx & memoryOut);
    int AvcParseNalUnits(ConstMemory const mem, MemoryEx * pMemoryOut);
    int IsomWriteAvcc(ConstMemory const memory, MemoryEx & memoryOut);

    static void CloseCodecs(AVFormatContext * pAVFrmtCntxt);

public:

    class Frame
    {
        friend class FileReader;

    public:

        Frame(void) : timestamp_nanosec(0), bKeyFrame(false)
        {
            memset(&src_packet, 0, sizeof(src_packet));
        }

        ConstMemory header;
        ConstMemory frame;
        Uint64 timestamp_nanosec;
        bool bKeyFrame;

    private:

        AVPacket src_packet;
    };

    FileReader();
    ~FileReader();

    bool Init(StRef<String> & fileName);
    void DeInit();
    bool IsInit();
    bool Seek(double dSeconds);
    double GetDuration(void) const
    {
        return m_dDuration;
    }
    StRef<String> GetFilename() const
    {
        return m_fileName;
    };

    // if ReadFrame returns 'true' use FreeFrame() function after using of Frame object.
    bool ReadFrame(Frame & readframe);
    void FreeFrame(Frame & readframe);
};

class MediaReader : public DependentCodeReferenced
{
private:
    StateMutex mutex;

public:
    enum ReadFrameResult
    {
        ReadFrameResult_Success,
        ReadFrameResult_Finish,
        ReadFrameResult_BurstLimit,
        ReadFrameResult_NoData,
        ReadFrameResult_Failure
    };

    struct ReadFrameBackend
    {
        ReadFrameResult (*audioFrame) (VideoStream::AudioMessage * mt_nonnull msg,
                                       void                      *cb_data);

        ReadFrameResult (*videoFrame) (VideoStream::VideoMessage * mt_nonnull msg,
                                       void                      *cb_data);
    };

private:
    enum SessionState {
        SessionState_FileHeader,
        SessionState_SequenceHeaders,
        SessionState_Frame
    };

    mt_const DataDepRef<PagePool> page_pool;
    mt_const Ref<Vfs> vfs;

    mt_const Time start_unixtime_sec;
    // 0 means no limit
    mt_const Size burst_size_limit;

    mt_mutex (mutex) NvrFileIterator file_iter;

    mt_mutex (mutex) StRef<String> cur_filename;
    mt_mutex (mutex) StRef<String> record_dir;

    mt_mutex (mutex) bool first_file;

    mt_mutex (mutex) bool sequence_headers_sent;
    mt_mutex (mutex) bool first_frame;

    mt_mutex (mutex) bool got_aac_seq_hdr;
    mt_mutex (mutex) bool aac_seq_hdr_sent;
    mt_mutex (mutex) PagePool::PageListHead aac_seq_hdr;
    mt_mutex (mutex) Size aac_seq_hdr_len;

    mt_mutex (mutex) bool got_avc_seq_hdr;
    mt_mutex (mutex) bool avc_seq_hdr_sent;
    mt_mutex (mutex) PagePool::PageListHead avc_seq_hdr;
    mt_mutex (mutex) Size avc_seq_hdr_len;

    FileReader m_fileReader;

    mt_mutex (mutex) void releaseSequenceHeaders_unlocked ();

    mt_mutex (mutex) void reset_unlocked ()
    {
        releaseSequenceHeaders_unlocked ();
        first_file = true;
        sequence_headers_sent = false;
        first_frame = true;
        file_iter.reset (start_unixtime_sec);
    }

    mt_mutex (mutex) bool tryOpenNextFile ();
    mt_mutex (mutex) Result readFileHeader ();
    mt_mutex (mutex) Result readIndexAndSeek (bool * mt_nonnull ret_seeked);

    mt_mutex (mutex) ReadFrameResult sendFrame (const FileReader::Frame & inputFrame,
                                                ReadFrameBackend const *read_frame_cb,
                                                void                   *read_frame_cb_data);

public:
    ReadFrameResult readMoreData (ReadFrameBackend const *read_frame_cb,
                                  void                   *read_frame_cb_data);

    void reset ()
    {
        mutex.lock ();
        reset_unlocked ();
        mutex.unlock ();
    }

    mt_const void init (PagePool    * mt_nonnull page_pool,
                        Vfs         * mt_nonnull vfs,
                        ConstMemory  stream_name,
                        Time         start_unixtime_sec,
                        Size         burst_size_limit,
                        StRef<String> record_dir);

    MediaReader (Object * const coderef_container)
        : DependentCodeReferenced (coderef_container),
          page_pool             (coderef_container),
          start_unixtime_sec    (0),
          burst_size_limit      (0),
          first_file            (true),
          sequence_headers_sent (false),
          first_frame           (true),
          got_aac_seq_hdr       (false),
          aac_seq_hdr_sent      (false),
          aac_seq_hdr_len       (0),
          got_avc_seq_hdr       (false),
          avc_seq_hdr_sent      (false),
          avc_seq_hdr_len       (0)
    {}

    ~MediaReader ();
};

}


#endif /* MOMENT_NVR__MEDIA_READER__H__ */

