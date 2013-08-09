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


#ifndef MOMENT__MEDIA_SOURCE__H__
#define MOMENT__MEDIA_SOURCE__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class MediaSource : public Object
{
public:
    struct Frontend {
        void (*error)       (void *cb_data);
        // End of stream.
        void (*eos)         (void *cb_data);
        void (*noVideo)     (void *cb_data);
        void (*gotVideo)    (void *cb_data);
    };

    virtual void createPipeline () = 0;
    virtual void releasePipeline () = 0;
    virtual void reportStatusEvents () = 0;

    class TrafficStats
    {
    public:
	Uint64 rx_bytes;
	Uint64 rx_audio_bytes;
	Uint64 rx_video_bytes;

	void reset ()
	{
	    rx_bytes = 0;
	    rx_audio_bytes = 0;
	    rx_video_bytes = 0;
	}
    };

    virtual void getTrafficStats (TrafficStats * mt_nonnull ret_traffic_stats) = 0;
    virtual void resetTrafficStats () = 0;
};

}


#endif /* MOMENT__MEDIA_SOURCE__H__ */

