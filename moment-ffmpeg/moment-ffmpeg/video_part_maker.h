#ifndef MOMENT__FFMPEG_VIDEO_PART_MAKER__H__
#define MOMENT__FFMPEG_VIDEO_PART_MAKER__H__

#include <moment/libmoment.h>
#include <libmary/types.h>
#include <moment-ffmpeg/media_reader.h>
#include <moment-ffmpeg/ffmpeg_stream.h>

using namespace M;
using namespace Moment;

namespace MomentFFmpeg {

class VideoPartMaker: public Object
{
public:

    VideoPartMaker();
    ~VideoPartMaker();

    bool Init(Vfs         * const mt_nonnull vfs,
              ConstMemory   const stream_name,
              ConstMemory   const record_dir,
              ConstMemory   const res_file_path,
              Time          const start_unixtime_sec,
              Time          const end_unixtime_sec);

    bool IsInit();

    bool Process();

private:

    bool tryOpenNextFile();

    FileReader      m_fileReader;
    nvrData         m_nvrData;
    NvrFileIterator m_fileIter;

    bool bIsInit;
    StRef<String>   record_dir;
    Time nStartTime;
    Time nCurFileStartTime;
    Time nCurFileShift;
    Time nEndTime;

};

}

#endif /* MOMENT__FFMPEG_VIDEO_PART_MAKER__H__ */
