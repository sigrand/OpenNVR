#ifndef MOMENT__SLAVE_MEDIA_SOURCE__H__
#define MOMENT__SLAVE_MEDIA_SOURCE__H__


#include <moment/media_source.h>

#include <moment/moment_server.h>


namespace Moment {

using namespace M;

class SlaveMediaSource : public MediaSource
{
private:
    mt_const StRef<String> master_stream_name;
    mt_const Cb<MediaSource::Frontend> frontend;
    mt_const Ref<VideoStream> bind_stream;

  mt_iface (MomentServer::VideoStreamHandler)
    static MomentServer::VideoStreamHandler const stream_handler;

    static void videoStreamAdded (VideoStream * mt_nonnull stream,
                                  ConstMemory  stream_name,
                                  void        *_self);
  mt_iface_end

public:
  mt_iface (MediaSource)
    void createPipeline () {}
    void releasePipeline () {}
    void reportStatusEvents () {}

    void getTrafficStats (TrafficStats * const mt_nonnull ret_traffic_stats) { ret_traffic_stats->reset(); }
    void resetTrafficStats () {}
  mt_iface_end

    void init (MomentServer * mt_nonnull moment,
               ConstMemory   stream_name,
               VideoStream  * mt_nonnull bind_stream,
               CbDesc<MediaSource::Frontend> const &frontend);
};

}


#endif /* MOMENT__SLAVE_MEDIA_SOURCE__H__ */

