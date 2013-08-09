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


#ifndef MOMENT__MP4_MUXER__H__
#define MOMENT__MP4_MUXER__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

mt_unsafe class Mp4Muxer
{
public:
    enum FrameType {
        FrameType_Audio,
        FrameType_Video
    };

private:
    struct TrackInfo
    {
        PagePool       *hdr_page_pool;
        PagePool::Page *hdr_msg;
        Size            hdr_offs;
        Size            hdr_size;

        Size num_frames;
        Size total_frame_size;

        PagePool::PageListHead stsz_pages;
        Size stsz_pos;

        PagePool::PageListHead stss_pages;
        Size stss_pos;
        Count num_stss_entries;

        PagePool::PageListHead stts_pages;
        Size stts_pos;
        Time prv_stts_value;

        PagePool::PageListHead ctts_pages;
        Size ctts_pos;

        PagePool::PageListHead stco_pages;
        Size stco_pos;

        Time prv_pts;
        Time min_pts;

        void clear (PagePool * mt_nonnull page_pool);

        TrackInfo ()
            : hdr_page_pool    (NULL),
              hdr_msg          (NULL),
              hdr_offs         (0),
              hdr_size         (0),
              num_frames       (0),
              total_frame_size (0),
              stsz_pos         (0),
              stss_pos         (0),
              num_stss_entries (0),
              stts_pos         (0),
              prv_stts_value   (0),
              ctts_pos         (0),
              stco_pos         (0),
              prv_pts          (0),
              min_pts          (0)
        {}
    };

    mt_const CodeDepRef<PagePool> page_pool;
    mt_const Time duration_millisec;

    TrackInfo audio_track;
    TrackInfo video_track;

    Uint64 mdat_pos;

    void patchTrackStco (TrackInfo * mt_nonnull track,
                         Uint32     offset);

    PagePool::PageListHead writeMoovAtom ();

    void processFrame (TrackInfo * mt_nonnull track,
                       Time       timestamp_nanosec,
                       Size       frame_size,
                       bool       is_sync_sample);

    void finalizeTrack (TrackInfo * mt_nonnull track);

public:
    void pass1_aacSequenceHeader (PagePool       * mt_nonnull msg_page_pool,
                                  PagePool::Page *msg,
                                  Size            msg_offs,
                                  Size            frame_size);

    void pass1_avcSequenceHeader (PagePool       * mt_nonnull msg_page_pool,
                                  PagePool::Page *msg,
                                  Size            msg_offs,
                                  Size            frame_size);

    void pass1_frame (FrameType frame_type,
                      Time      timestamp_nanosec,
                      Size      frame_size,
                      bool      is_sync_sample);

    PagePool::PageListHead pass1_complete ();

    Size getTotalDataSize () const
        { return audio_track.total_frame_size + video_track.total_frame_size; }

    void clear ();

    mt_const void init (PagePool * mt_nonnull page_pool,
                        Time       const duration_millisec);

    Mp4Muxer ()
        : duration_millisec (0),
          mdat_pos (0)
    {}

    ~Mp4Muxer ();
};

}


#endif /* MOMENT__MP4_MUXER__H__ */

