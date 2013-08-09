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


#ifndef MOMENT_NVR__TYPES__H__
#define MOMENT_NVR__TYPES__H__


#include <moment/libmoment.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

struct AudioRecordFrameType
{
    enum Value {
        Unknown           = 0,
        RawData           = 1,
        AacSequenceHeader = 2,
        SpeexHeader       = 3
    };
    operator Value () const { return value; }
    AudioRecordFrameType (Value const value) : value (value) {}
    AudioRecordFrameType () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
private:
    Value value;
};

AudioRecordFrameType        toAudioRecordFrameType (VideoStream::AudioFrameType ftype);
VideoStream::AudioFrameType toAudioFrameType       (unsigned ftype);

struct VideoRecordFrameType
{
    enum Value  {
        Unknown              = 0,
        KeyFrame             = 1,
        InterFrame           = 2,
        DisposableInterFrame = 3,
        AvcSequenceHeader    = 4,
        AvcEndOfSequence     = 5
    };
    operator Value () const { return value; }
    VideoRecordFrameType (Value const value) : value (value) {}
    VideoRecordFrameType () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
private:
    Value value;
};

VideoRecordFrameType        toVideoRecordFrameType (VideoStream::VideoFrameType ftype);
VideoStream::VideoFrameType toVideoFrameType       (unsigned ftype);

struct AudioRecordCodecId
{
    enum Value {
        Unknown               = 0,
        AAC                   = 1,
        MP3                   = 2,
        Speex                 = 3,
        ADPCM                 = 4,
        LinearPcmLittleEndian = 5
    };
    operator Value () const { return value; }
    AudioRecordCodecId (Value const value) : value (value) {}
    AudioRecordCodecId () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
private:
    Value value;
};

AudioRecordCodecId toAudioRecordCodecId (VideoStream::AudioCodecId cid);

struct VideoRecordCodecId
{
    enum Value {
        Unknown      = 0,
        AVC          = 1,
        SorensonH263 = 2
    };
    operator Value () const { return value; }
    VideoRecordCodecId (Value const value) : value (value) {}
    VideoRecordCodecId () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
private:
    Value value;
};

VideoRecordCodecId toVideoRecordCodecId (VideoStream::VideoCodecId cid);

}


#endif /* MOMENT_NVR__TYPES__H__ */

