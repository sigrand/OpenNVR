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


#include <moment-nvr/media_reader.h>


using namespace M;
using namespace Moment;

namespace MomentNvr {

static LogGroup libMary_logGroup_reader ("mod_nvr.media_reader", LogLevel::I);

mt_mutex (mutex) bool
MediaReader::tryOpenNextFile ()
{
    StRef<String> const filename = file_iter.getNext ();
    if (!filename) {
        logD (reader, _func, "filename is null");
        return false;
    }

    session_state = SessionState_FileHeader;

    logD (reader, _func, "filename: ", filename);

    {
        StRef<String> const vdat_filename = st_makeString (filename, ".vdat");
        vdat_file = vfs->openFile (vdat_filename->mem(), 0 /* open_flags */, FileAccessMode::ReadOnly);
        if (!vdat_file) {
            logE_ (_func, "vfs->openFile() failed for filename ",
                   vdat_filename, ": ", exc->toString());
            return false;
        }

        cur_filename = filename;
    }

    return true;
}

mt_mutex (mutex) Result
MediaReader::readFileHeader ()
{
    File * const file = vdat_file->getFile();

    FileSize fpos = 0;
    if (!file->tell (&fpos)) {
        logE_ (_func, "tell() failed: ", exc->toString());
        return Result::Failure;
    }

    Byte header [20];
    Size bytes_read = 0;
    IoResult const res = file->readFull (Memory::forObject (header), &bytes_read);
    if (res == IoResult::Error) {
        logE_ (_func, "vdat file read error: ", exc->toString());
        if (!file->seek (fpos, SeekOrigin::Beg))
            logE_ (_func, "seek() failed: ", exc->toString());
        return Result::Failure;
    } else
    if (res != IoResult::Normal) {
        logE_ (_func, "Could not read vdat header");
        if (!file->seek (fpos, SeekOrigin::Beg))
            logE_ (_func, "seek() failed: ", exc->toString());
        return Result::Failure;
    }

    if (bytes_read < sizeof (header)) {
        if (!file->seek (fpos, SeekOrigin::Beg))
            logE_ (_func, "seek() failed: ", exc->toString());
        return Result::Failure;
    }

    if (!equal (ConstMemory (header, 4), "MMNT")) {
        logE_ (_func, "Invalid vdat header: no magic bytes");
        return Result::Failure;
    }

    // TODO Parse format version and header length.

    if (!sequence_headers_sent && first_frame)
        session_state = SessionState_SequenceHeaders;
    else
        session_state = SessionState_Frame;

    vdat_data_start = sizeof (header);

    return Result::Success;
}

mt_mutex (mutex) Result
MediaReader::readIndexAndSeek (bool * const mt_nonnull ret_seeked)
{
    *ret_seeked = false;

    if (!first_file) {
        return Result::Success;
    }
    first_file = false;

    logD_ (_func_);

    StRef<String> const idx_filename = st_makeString (cur_filename, ".idx");
    Ref<Vfs::VfsFile> const idx_file = vfs->openFile (idx_filename->mem(),
                                                      0 /* open_flags */,
                                                      FileAccessMode::ReadOnly);
    if (!idx_file) {
        logE_ (_func, "vfs->openFile() failed for filename ",
               idx_filename, ": ", exc->toString());
        return Result::Failure;
    }

    Uint64 last_offset = 0;

    Byte buf [1 << 16 /* 64 Kb */];
    File * const file = idx_file->getFile();
    for (;;) {
        Size bytes_read = 0;
        IoResult const res = file->readFull (Memory::forObject (buf), &bytes_read);
        if (res == IoResult::Error) {
            logE_ (_func, "idx file read error: ", exc->toString());
            return Result::Failure;
        }

        if (res == IoResult::Eof) {
            logD_ (_func, "idx eof");
            break;
        }

        for (Size i = 0; i + 16 <= bytes_read; i += 16) {
            Uint64 const unixtime_timestamp_nanosec = ((Uint64) buf [i + 0] << 56) |
                                                      ((Uint64) buf [i + 1] << 48) |
                                                      ((Uint64) buf [i + 2] << 40) |
                                                      ((Uint64) buf [i + 3] << 32) |
                                                      ((Uint64) buf [i + 4] << 24) |
                                                      ((Uint64) buf [i + 5] << 16) |
                                                      ((Uint64) buf [i + 6] <<  8) |
                                                      ((Uint64) buf [i + 7] <<  0);

            Uint64 const data_offset = ((Uint64) buf [i +  8] << 56) |
                                       ((Uint64) buf [i +  9] << 48) |
                                       ((Uint64) buf [i + 10] << 40) |
                                       ((Uint64) buf [i + 11] << 32) |
                                       ((Uint64) buf [i + 12] << 24) |
                                       ((Uint64) buf [i + 13] << 16) |
                                       ((Uint64) buf [i + 14] <<  8) |
                                       ((Uint64) buf [i + 15] <<  0);

            logD_ (_func, "idx ts ", unixtime_timestamp_nanosec, " offs ", data_offset);

            if (unixtime_timestamp_nanosec > start_unixtime_sec * 1000000000) {
                logD_ (_func, "idx hit");
                break;
            }

            last_offset = data_offset;
        }
    }

    if (last_offset > 0) {
        *ret_seeked = true;

        logD_ (_func, "seek (", last_offset, ")");
        if (!vdat_file->getFile()->seek (vdat_data_start + last_offset, SeekOrigin::Beg)) {
            logE_ (_func, "seek() failed: ", exc->toString());
            return Result::Failure;
        }
    } else {
        logD_ (_func, "no seek");
    }

    return Result::Success;
}

mt_mutex (mutex) MediaReader::ReadFrameResult
MediaReader::readFrame (ReadFrameBackend const * const read_frame_cb,
                        void                   * const read_frame_cb_data)
{
    File * const file = vdat_file->getFile();

    Size total_read = 0;
    for (;;) {
        FileSize fpos = 0;
        if (!file->tell (&fpos)) {
            logE_ (_func, "tell() failed: ", exc->toString());
            return ReadFrameResult_Failure;
        }

        Byte frame_header [14];
        Size bytes_read = 0;
        IoResult const res = file->readFull (Memory::forObject (frame_header), &bytes_read);
        if (res == IoResult::Error) {
            logE_ (_func, "vdat file read error: ", exc->toString());
            return ReadFrameResult_Failure;
        }

        if (res != IoResult::Normal) {
//            logE_ (_func, "Could not read media header");
            if (!file->seek (fpos, SeekOrigin::Beg))
                logE_ (_func, "seek() failed: ", exc->toString());
            return ReadFrameResult_Failure;
        }

        if (bytes_read < sizeof (frame_header)) {
            if (!file->seek (fpos, SeekOrigin::Beg))
                logE_ (_func, "seek() failed: ", exc->toString());
            return ReadFrameResult_Failure;
        }

        Time const msg_unixtime_ts_nanosec = ((Time) frame_header [0] << 56) |
                                             ((Time) frame_header [1] << 48) |
                                             ((Time) frame_header [2] << 40) |
                                             ((Time) frame_header [3] << 32) |
                                             ((Time) frame_header [4] << 24) |
                                             ((Time) frame_header [5] << 16) |
                                             ((Time) frame_header [6] <<  8) |
                                             ((Time) frame_header [7] <<  0);
//        logD_ (_func, "msg ts: ", msg_unixtime_ts_nanosec);

        Size const msg_ext_len = ((Size) frame_header [ 8] << 24) |
                                 ((Size) frame_header [ 9] << 16) |
                                 ((Size) frame_header [10] <<  8) |
                                 ((Size) frame_header [11] <<  0);
//        logD_ (_func, "msg_ext_len: ", msg_ext_len);

        if (session_state == SessionState_SequenceHeaders) {
            if (!(   (frame_header [12] == 2 && frame_header [13] == AudioRecordFrameType::AacSequenceHeader)
                  || (frame_header [12] == 1 && frame_header [13] == VideoRecordFrameType::AvcSequenceHeader)))
            {
                session_state = SessionState_Frame;
                bool seeked = false;
                if (!readIndexAndSeek (&seeked))
                    return ReadFrameResult_Failure;

                if (seeked)
                    continue;
            }
        }

        if (session_state == SessionState_Frame) {
            if (msg_unixtime_ts_nanosec < start_unixtime_sec * 1000000000) {
//                logD_ (_func, "skipping ts ", msg_unixtime_ts_nanosec, " < ", start_unixtime_sec * 1000000000);
                if (!file->seek (msg_ext_len - 2, SeekOrigin::Cur)) {
                    logE_ (_func, "seek() failed: ", exc->toString());
                    if (!file->seek (fpos, SeekOrigin::Beg))
                        logE_ (_func, "seek() failed: ", exc->toString());
                    return ReadFrameResult_Failure;
                }
                continue;
            }

            if (first_frame)
                first_frame = false;
        }

        ReadFrameResult client_res = ReadFrameResult_Success;

        if (msg_ext_len <= (1 << 25 /* 32 Mb */)
            && msg_ext_len >= 2)
        {
            PagePool::PageListHead page_list;
            Size const page_size = page_pool->getPageSize ();
            page_pool->getPages (&page_list, msg_ext_len - 2);

            PagePool::Page *cur_page = page_list.first;
            Size msg_read = 0;
            Size page_read = 0;
            for (;;) {
                if (msg_read >= msg_ext_len - 2)
                    break;

                Size nread = 0;
                Size toread = page_size - page_read;
                if (toread > msg_ext_len - 2 - msg_read)
                    toread = msg_ext_len - 2 - msg_read;
                IoResult const res = file->readFull (Memory (cur_page->getData() + page_read,
                                                             toread),
                                                     &nread);
                if (res == IoResult::Error) {
                    logE_ (_func, "vdat file read error: ", exc->toString());
                    page_pool->msgUnref (page_list.first);
                    return ReadFrameResult_Failure;
                }

                if (res != IoResult::Normal) {
                    logE_ (_func, "Could not read frame");
                    if (!file->seek (fpos, SeekOrigin::Beg))
                        logE_ (_func, "seek() failed: ", exc->toString());
                    page_pool->msgUnref (page_list.first);
                    return ReadFrameResult_Failure;
                }

                page_read += nread;
                msg_read  += nread;

                cur_page->data_len = page_read;

                if (page_read >= page_size) {
                    cur_page = cur_page->getNextMsgPage();
                    page_read = 0;
                }
            }

//            logD_ (_func, "msg_read: ", msg_read);

//            Byte * const hdr = page_list.first->getData();
            Byte * const hdr = frame_header + 12;
            if (hdr [0] == 2) {
              // Audio frame
                VideoStream::AudioFrameType const frame_type = toAudioFrameType (hdr [1]);
                logS_ (_func, "ts ", msg_unixtime_ts_nanosec, " ", frame_type);

                bool skip = false;
                if (frame_type == VideoStream::AudioFrameType::AacSequenceHeader) {
                    if (got_aac_seq_hdr
                        && PagePool::msgEqual (aac_seq_hdr.first, page_list.first))
                    {
                        skip = true;
                    } else {
                        if (got_aac_seq_hdr)
                            page_pool->msgUnref (aac_seq_hdr.first);

                        aac_seq_hdr_sent = true;
                        aac_seq_hdr = page_list;
                        aac_seq_hdr_len = msg_ext_len - 2;
                        page_pool->msgRef (aac_seq_hdr.first);
                    }
                }

                if (!skip && frame_type.isAudioData()) {
                    if (read_frame_cb && read_frame_cb->audioFrame) {
                        if (got_aac_seq_hdr && !aac_seq_hdr_sent) {
                            aac_seq_hdr_sent = true;

                            VideoStream::AudioMessage msg;
                            msg.timestamp_nanosec = msg_unixtime_ts_nanosec;

                            msg.page_pool = page_pool;
                            msg.page_list = aac_seq_hdr;
                            msg.msg_len = aac_seq_hdr_len;
                            msg.msg_offset = 0;
                            msg.prechunk_size = 0;

                            msg.codec_id = VideoStream::AudioCodecId::AAC;
                            msg.frame_type = frame_type;

                            // Note that callback's return value is not used.
                            read_frame_cb->audioFrame (&msg, read_frame_cb_data);
                        }

                        VideoStream::AudioMessage msg;
                        msg.timestamp_nanosec = msg_unixtime_ts_nanosec;

                        msg.page_pool = page_pool;
                        msg.page_list = page_list;
                        msg.msg_len = msg_ext_len - 2;
                        msg.msg_offset = 0;
                        msg.prechunk_size = 0;

                        msg.codec_id = VideoStream::AudioCodecId::AAC;
                        msg.frame_type = frame_type;

                        client_res = read_frame_cb->audioFrame (&msg, read_frame_cb_data);
                    }
                }
            } else
            if (hdr [0] == 1) {
              // Video frame
                VideoStream::VideoFrameType const frame_type = toVideoFrameType (hdr [1]);
                logS_ (_func, "ts ", msg_unixtime_ts_nanosec, " ", frame_type);

                bool skip = false;
                if (frame_type == VideoStream::VideoFrameType::AvcSequenceHeader) {
                    if (got_avc_seq_hdr
                        && PagePool::msgEqual (avc_seq_hdr.first, page_list.first))
                    {
                        skip = true;
                    } else {
                        if (got_avc_seq_hdr)
                            page_pool->msgUnref (avc_seq_hdr.first);

                        got_avc_seq_hdr = true;
                        avc_seq_hdr = page_list;
                        avc_seq_hdr_len = msg_ext_len - 2;
                        page_pool->msgRef (avc_seq_hdr.first);
                    }
                }

                if (!skip && frame_type.isVideoData()) {
                    if (read_frame_cb && read_frame_cb->videoFrame) {
                        if (got_avc_seq_hdr && !avc_seq_hdr_sent) {
                            avc_seq_hdr_sent = true;

                            VideoStream::VideoMessage msg;
                            msg.timestamp_nanosec = msg_unixtime_ts_nanosec;

                            msg.page_pool = page_pool;
                            msg.page_list = avc_seq_hdr;
                            msg.msg_len = avc_seq_hdr_len;
                            msg.msg_offset = 0;
                            msg.prechunk_size = 0;

                            msg.codec_id = VideoStream::VideoCodecId::AVC;
                            msg.frame_type = VideoStream::VideoFrameType::AvcSequenceHeader;
                            msg.is_saved_frame = false;

                            // Note that callback's return value is not used.
                            read_frame_cb->videoFrame (&msg, read_frame_cb_data);
                        }

                        VideoStream::VideoMessage msg;
                        msg.timestamp_nanosec = msg_unixtime_ts_nanosec;

                        msg.page_pool = page_pool;
                        msg.page_list = page_list;
                        msg.msg_len = msg_ext_len - 2;
                        msg.msg_offset = 0;
                        msg.prechunk_size = 0;

                        msg.codec_id = VideoStream::VideoCodecId::AVC;
                        msg.frame_type = frame_type;
                        msg.is_saved_frame = false;

                        client_res = read_frame_cb->videoFrame (&msg, read_frame_cb_data);
                    }
                }
            } else {
                logE_ (_func, "unknown frame type: ", hdr [0]);
            }

            page_pool->msgUnref (page_list.first);

            // TODO Handle AVC/AAC codec data
        } else {
            logE_ (_func, "frame is too large (", msg_ext_len, " bytes), skipping");
            if (!file->seek (msg_ext_len - 2, SeekOrigin::Cur)) {
                logE_ (_func, "seek() failed: ", exc->toString());
                if (!file->seek (fpos, SeekOrigin::Beg))
                    logE_ (_func, "seek() failed: ", exc->toString());
                return ReadFrameResult_Failure;
            }
        }

        total_read += msg_ext_len - 2;
        if (burst_size_limit) {
            if (total_read >= burst_size_limit) {
                logD_ (_func, "BurstLimit");
                return ReadFrameResult_BurstLimit;
            }
        }

        if (client_res != ReadFrameResult_Success)
            return client_res;
    } // for (;;)

    return ReadFrameResult_Success;
}

MediaReader::ReadFrameResult
MediaReader::readMoreData (ReadFrameBackend const * const read_frame_cb,
                           void                   * const read_frame_cb_data)
{
    if (!vdat_file) {
        if (!tryOpenNextFile ())
            return ReadFrameResult_NoData;
    }

    for (;;) {
        assert (vdat_file);

        Result res = Result::Failure;
        switch (session_state) {
            case SessionState_FileHeader:
                res = readFileHeader ();
                break;
            case SessionState_SequenceHeaders:
            case SessionState_Frame:
                ReadFrameResult const rf_res = readFrame (read_frame_cb, read_frame_cb_data);
                if (rf_res == ReadFrameResult_BurstLimit) {
                    logD (reader, _func, "session 0x", fmt_hex, (UintPtr) this, ": BurstLimit");
                    return ReadFrameResult_BurstLimit;
                } else
                if (rf_res == ReadFrameResult_Finish) {
                    return ReadFrameResult_Finish;
                } else
                if (rf_res == ReadFrameResult_Success) {
                    res = Result::Success;
                } else {
                    res = Result::Failure;
                }

                break;
        }

        if (!res) {
            if (!tryOpenNextFile ())
                return ReadFrameResult_NoData;
        }
    }

    return ReadFrameResult_Success;
}

mt_const void
MediaReader::init (PagePool    * const mt_nonnull page_pool,
                   Vfs         * const mt_nonnull vfs,
                   ConstMemory   const stream_name,
                   Time          const start_unixtime_sec,
                   Size          const burst_size_limit)
{
    this->page_pool = page_pool;
    this->vfs = vfs;
    this->start_unixtime_sec = start_unixtime_sec;
    this->burst_size_limit = burst_size_limit;

    logD_ (_func, "start_unixtime_sec: ", start_unixtime_sec);
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

