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


#include <moment-nvr/types.h>


using namespace M;
using namespace Moment;

namespace MomentNvr {

Size
AudioRecordFrameType::toString_ (Memory const &mem,
                                 Format const & /* fmt */) const
{
    switch (value) {
        case Unknown:
            return toString (mem, "Unknown");
	case RawData:
	    return toString (mem, "RawData");
	case AacSequenceHeader:
	    return toString (mem, "AacSequenceHeader");
	case SpeexHeader:
	    return toString (mem, "SpeexHeader");
    }
    unreachable ();
    return 0;
}

AudioRecordFrameType
toAudioRecordFrameType (VideoStream::AudioFrameType const ftype)
{
    switch (ftype) {
        case VideoStream::AudioFrameType::RawData:
            return  AudioRecordFrameType::RawData;
        case VideoStream::AudioFrameType::AacSequenceHeader:
            return  AudioRecordFrameType::AacSequenceHeader;
        case VideoStream::AudioFrameType::SpeexHeader:
            return  AudioRecordFrameType::SpeexHeader;
        default:
            return  AudioRecordFrameType::Unknown;
    }
    unreachable ();
    return AudioRecordFrameType::Unknown;
}

VideoStream::AudioFrameType
toAudioFrameType (unsigned const ftype)
{
    switch (ftype) {
        case AudioRecordFrameType::RawData:
            return VideoStream::AudioFrameType::RawData;
        case AudioRecordFrameType::AacSequenceHeader:
            return VideoStream::AudioFrameType::AacSequenceHeader;
        case AudioRecordFrameType::SpeexHeader:
            return VideoStream::AudioFrameType::SpeexHeader;
        default:
            return VideoStream::AudioFrameType::Unknown;
    }
    unreachable ();
    return VideoStream::AudioFrameType::Unknown;
}

Size
VideoRecordFrameType::toString_ (Memory const &mem,
                                 Format const & /* fmt */) const
{
    switch (value) {
        case Unknown:
            return toString (mem, "Unknown");
        case KeyFrame:
            return toString (mem, "KeyFrame");
        case InterFrame:
            return toString (mem, "InterFrame");
        case DisposableInterFrame:
            return toString (mem, "DisposableInterFrame");
        case AvcSequenceHeader:
            return toString (mem, "AvcSequenceHeader");
        case AvcEndOfSequence:
            return toString (mem, "AvcEndOfSequence");
    }
    unreachable ();
    return 0;
}

VideoRecordFrameType
toVideoRecordFrameType (VideoStream::VideoFrameType const ftype)
{
    switch (ftype) {
        case VideoStream::VideoFrameType::KeyFrame:
            return  VideoRecordFrameType::KeyFrame;
        case VideoStream::VideoFrameType::InterFrame:
            return  VideoRecordFrameType::InterFrame;
        case VideoStream::VideoFrameType::DisposableInterFrame:
            return  VideoRecordFrameType::DisposableInterFrame;
        case VideoStream::VideoFrameType::AvcSequenceHeader:
            return  VideoRecordFrameType::AvcSequenceHeader;
        case VideoStream::VideoFrameType::AvcEndOfSequence:
            return  VideoRecordFrameType::AvcEndOfSequence;
        default:
            return  VideoRecordFrameType::Unknown;
    }
    unreachable ();
    return VideoRecordFrameType::Unknown;
}

VideoStream::VideoFrameType
toVideoFrameType (unsigned const ftype)
{
    switch (ftype) {
        case VideoRecordFrameType::KeyFrame:
            return VideoStream::VideoFrameType::KeyFrame;
        case VideoRecordFrameType::InterFrame:
            return VideoStream::VideoFrameType::InterFrame;
        case VideoRecordFrameType::DisposableInterFrame:
            return VideoStream::VideoFrameType::DisposableInterFrame;
        case VideoRecordFrameType::AvcSequenceHeader:
            return VideoStream::VideoFrameType::AvcSequenceHeader;
        case VideoRecordFrameType::AvcEndOfSequence:
            return VideoStream::VideoFrameType::AvcEndOfSequence;
        default:
            return VideoStream::VideoFrameType::Unknown;
    }
    unreachable ();
    return VideoStream::VideoFrameType::Unknown;
}

Size
AudioRecordCodecId::toString_ (Memory const &mem,
                               Format const & /* fmt */) const
{
    switch (value) {
        case Unknown:
            return toString (mem, "Unknown");
        case AAC:
            return toString (mem, "AAC");
        case MP3:
            return toString (mem, "MP3");
        case Speex:
            return toString (mem, "Speex");
        case ADPCM:
            return toString (mem, "ADPCM");
        case LinearPcmLittleEndian:
            return toString (mem, "LinearPcmLittleEndian");
    }
    unreachable ();
    return 0;
}

AudioRecordCodecId
toAudioRecordCodecId (VideoStream::AudioCodecId const cid)
{
    switch (cid) {
        case VideoStream::AudioCodecId::AAC:
            return  AudioRecordCodecId::AAC;
        case VideoStream::AudioCodecId::MP3:
            return  AudioRecordCodecId::MP3;
        case VideoStream::AudioCodecId::Speex:
            return  AudioRecordCodecId::Speex;
        case VideoStream::AudioCodecId::ADPCM:
            return  AudioRecordCodecId::ADPCM;
        case VideoStream::AudioCodecId::LinearPcmLittleEndian:
            return  AudioRecordCodecId::LinearPcmLittleEndian;
        default:
            return  AudioRecordCodecId::Unknown;
    }
    unreachable ();
    return AudioRecordCodecId::Unknown;
}

Size
VideoRecordCodecId::toString_ (Memory const &mem,
                               Format const & /* fmt */) const
{
    switch (value) {
        case Unknown:
            return toString (mem, "Unknown");
        case AVC:
            return toString (mem, "AVC");
        case SorensonH263:
            return toString (mem, "SorensonH263");
    }
    unreachable ();
    return 0;
}

VideoRecordCodecId
toVideoRecordCodecId (VideoStream::VideoCodecId const cid)
{
    switch (cid) {
        case VideoStream::VideoCodecId::AVC:
            return  VideoRecordCodecId::AVC;
        case VideoStream::VideoCodecId::SorensonH263:
            return  VideoRecordCodecId::SorensonH263;
        default:
            return  VideoRecordCodecId::Unknown;
    }
    unreachable ();
    return VideoRecordCodecId::Unknown;
}

}

