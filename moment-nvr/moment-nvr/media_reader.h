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


namespace MomentNvr {

using namespace M;
using namespace Moment;

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

    mt_mutex (mutex) SessionState session_state;
    mt_mutex (mutex) NvrFileIterator file_iter;

    mt_mutex (mutex) Ref<Vfs::VfsFile> vdat_file;
    mt_mutex (mutex) StRef<String> cur_filename;
    mt_mutex (mutex) Uint64 vdat_data_start;

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

    mt_mutex (mutex) void releaseSequenceHeaders_unlocked ();

    mt_mutex (mutex) void reset_unlocked ()
    {
        session_state = SessionState_FileHeader;
        releaseSequenceHeaders_unlocked ();
        first_file = true;
        sequence_headers_sent = false;
        first_frame = true;
        file_iter.reset (start_unixtime_sec);
        vdat_file = NULL;
    }

    mt_mutex (mutex) bool tryOpenNextFile  ();
    mt_mutex (mutex) Result readFileHeader ();
    mt_mutex (mutex) Result readIndexAndSeek (bool * mt_nonnull ret_seeked);

    mt_mutex (mutex) ReadFrameResult readFrame (ReadFrameBackend const *read_frame_cb,
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
                        Size         burst_size_limit);

    MediaReader (Object * const coderef_container)
        : DependentCodeReferenced (coderef_container),
          page_pool             (coderef_container),
          start_unixtime_sec    (0),
          burst_size_limit      (0),
          session_state         (SessionState_FileHeader),
          vdat_data_start       (0),
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

