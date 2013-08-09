#include <moment/slave_media_source.h>


using namespace M;

namespace Moment {

MomentServer::VideoStreamHandler const SlaveMediaSource::stream_handler = {
    videoStreamAdded
};

void
SlaveMediaSource::videoStreamAdded (VideoStream * const mt_nonnull stream,
                                    ConstMemory   const stream_name,
                                    void        * const _self)
{
    SlaveMediaSource * const self = static_cast <SlaveMediaSource*> (_self);

    if (!equal (stream_name, self->master_stream_name->mem()))
        return;

    if (self->frontend)
        self->frontend.call (self->frontend->gotVideo);

    self->bind_stream->bindToStream (stream, stream, true, true);
}

void
SlaveMediaSource::init (MomentServer * const mt_nonnull moment,
                        ConstMemory    const stream_name,
                        VideoStream  * const mt_nonnull bind_stream,
                        CbDesc<MediaSource::Frontend> const &frontend)
{
    this->master_stream_name = st_grab (new (std::nothrow) String (stream_name));
    this->bind_stream = bind_stream;
    this->frontend = frontend;

    moment->lock ();

    if (VideoStream * const stream = moment->getVideoStream_unlocked (stream_name))
        bind_stream->bindToStream (stream, stream, true, true);

    moment->addVideoStreamHandler_unlocked (
            CbDesc<MomentServer::VideoStreamHandler> (&stream_handler, this, this));

    moment->unlock ();
}

}

