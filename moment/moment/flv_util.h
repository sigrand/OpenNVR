/*  Moment Video Server - High performance media server
    Copyright (C) 2012 Dmitry Shatrov
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


#ifndef LIBMOMENT__FLV_UTIL__H__
#define LIBMOMENT__FLV_UTIL__H__


#include <moment/video_stream.h>


namespace Moment {

enum {
    FlvAudioHeader_MaxLen = 2,
    FlvVideoHeader_MaxLen = 5
};

unsigned fillFlvAudioHeader (VideoStream::AudioMessage * mt_nonnull audio_msg,
                             Memory mem);

unsigned fillFlvVideoHeader (VideoStream::VideoMessage * mt_nonnull video_msg,
                             Memory mem);

static inline unsigned flvSamplingRateToNumeric (Byte const flv_rate)
{
    switch (flv_rate) {
        case 0:
            return 5512;
        case 1:
            return 11025;
        case 2:
            return 22050;
        case 3:
            return 44100;
    }

    return 44100;
}

static inline unsigned numericSamplingRateToFlv (unsigned const rate)
{
    switch (rate) {
        case 5512:
        case 5513:
            return 0;
        case 8000:
        case 11025:
        case 16000:
            return 1;
        case 22050:
            return 2;
        case 44100:
            return 3;
    }

    return 3;
}

}


#endif /* LIBMOMENT__FLV_UTIL__H__ */

