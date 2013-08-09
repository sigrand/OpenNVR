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


#include <moment/flv_util.h>


namespace Moment {

unsigned fillFlvAudioHeader (VideoStream::AudioMessage * const mt_nonnull audio_msg,
                             Memory const mem)
{
    assert (mem.len() >= 2);

    Byte audio_hdr = 0;
    Byte audio_hdr_ext = 0;
    unsigned audio_hdr_len = 1;

#if 0
    switch (audio_msg->frame_type) {
        case Unknown:
            assert (0 && "Unknown audio frame type");
            break;
        case RawData:
        case AacSequenceHeader:
        case SpeexHeader:
          // Fall-through
            break;
    }
#endif

    switch (audio_msg->codec_id) {
        case VideoStream::AudioCodecId::Unknown:
            logD_ (_func, "unknown audio codec id");
            return 0;
        case VideoStream::AudioCodecId::LinearPcmPlatformEndian:
            audio_hdr = 0x00;
            break;
        case VideoStream::AudioCodecId::ADPCM:
	    audio_hdr = 0x10;
            break;
        case VideoStream::AudioCodecId::MP3:
            audio_hdr = 0x20;
            break;
        case VideoStream::AudioCodecId::LinearPcmLittleEndian:
	    audio_hdr = 0x30;
            break;
        case VideoStream::AudioCodecId::Nellymoser_16kHz_mono:
            audio_hdr = 0x40;
            break;
        case VideoStream::AudioCodecId::Nellymoser_8kHz_mono:
            audio_hdr = 0x50;
            break;
        case VideoStream::AudioCodecId::Nellymoser:
	    audio_hdr = 0x60;
            break;
        case VideoStream::AudioCodecId::G711ALaw:
	    audio_hdr = 0x70;
            break;
        case VideoStream::AudioCodecId::G711MuLaw:
	    audio_hdr = 0x80;
            break;
        case VideoStream::AudioCodecId::AAC:
            audio_hdr = 0xa0;

            if (audio_msg->frame_type == VideoStream::AudioFrameType::AacSequenceHeader)
                audio_hdr_ext = 0; // AAC sequence header
            else
                audio_hdr_ext = 1; // AAC raw

            audio_hdr_len = 2;
            break;
        case VideoStream::AudioCodecId::Speex:
	    audio_hdr = 0xb0;
            break;
        case VideoStream::AudioCodecId::MP3_8kHz:
            audio_hdr = 0xe0;
            break;
        case VideoStream::AudioCodecId::DeviceSpecific:
            audio_hdr = 0xf0;
            break;
    }

    unsigned rate = audio_msg->rate;
    unsigned channels = audio_msg->channels;

    if (audio_msg->codec_id == VideoStream::AudioCodecId::AAC) {
      // FLV spec forces these values.
        rate = 44100;
        channels = 2;
    } else
    if (audio_msg->codec_id == VideoStream::AudioCodecId::Speex) {
      // FLV spec forces these values.
        rate = 5512;
        channels = 1;
    }

//    if (audio_msg->codec_id != VideoStream::AudioCodecId::G711ALaw &&
//        audio_msg->codec_id != VideoStream::AudioCodecId::G711MuLaw)
    {
        audio_hdr |= 0x02; // 16-bit samples
    }

    if (channels > 1)
        audio_hdr |= 1; // stereo

    switch (rate) {
        case 8000:
            if (audio_msg->codec_id == VideoStream::AudioCodecId::MP3) {
                audio_hdr &= 0x0f;
                audio_hdr |= 0xe0; // MP3 8 kHz
            }

            audio_hdr |= 0x04; // 11 kHz
            break;
        case 5512:
        case 5513:
            audio_hdr |= 0x00; // 5.5 kHz
            break;
        case 11025:
        case 16000:
            audio_hdr |= 0x04; // 11 kHz
            break;
        case 22050:
            audio_hdr |= 0x08; // 22 kHz
            break;
        case 44100:
            audio_hdr |= 0x0c; // 44 kHz
            break;
        default:
            audio_hdr |= 0x0c; // 44 kHz
            break;
    }

    mem.mem() [0] = audio_hdr;
    mem.mem() [1] = audio_hdr_ext;

//    logD_ (_func, "audio_hdr: 0x", fmt_hex, audio_hdr, ", audio_hdr_len: ", audio_hdr_len);

    return audio_hdr_len;
}

unsigned fillFlvVideoHeader (VideoStream::VideoMessage * const mt_nonnull video_msg,
                             Memory const mem)
{
//    logD_ (_func, "codec_id: ", video_msg->codec_id, ", frame_type: ", video_msg->frame_type);

    assert (mem.len() >= 5);

    Byte video_hdr [5] = { 0, 0, 0, 0, 0 };
    Size video_hdr_len = 1;
    switch (video_msg->codec_id) {
        case VideoStream::VideoCodecId::Unknown:
            logW_ (_func, "unknown video codec id");
            return 0;
        case VideoStream::VideoCodecId::SorensonH263:
            video_hdr [0] = 0x02;
            break;
        case VideoStream::VideoCodecId::ScreenVideo:
            video_hdr [0] = 0x03;
            break;
        case VideoStream::VideoCodecId::ScreenVideoV2:
            video_hdr [0] = 0x06;
            break;
        case VideoStream::VideoCodecId::VP6:
            video_hdr [0] = 0x04;
            break;
        case VideoStream::VideoCodecId::VP6Alpha:
            video_hdr [0] = 0x05;
            break;
        case VideoStream::VideoCodecId::AVC:
            video_hdr [0] = 0x07;

            video_hdr [1] = 1; // AVC NALU

            // Composition time offset
            video_hdr [2] = 0;
            video_hdr [3] = 0;
            video_hdr [4] = 0;

            video_hdr_len = 5;
            break;
    }

    switch (video_msg->frame_type) {
        case VideoStream::VideoFrameType::Unknown:
            logW_ (_func, "unknown video frame type");
            break;
        case VideoStream::VideoFrameType::KeyFrame:
            video_hdr [0] |= 0x10;
            break;
        case VideoStream::VideoFrameType::InterFrame:
            video_hdr [0] |= 0x20;
            break;
        case VideoStream::VideoFrameType::DisposableInterFrame:
            video_hdr [0] |= 0x30;
            break;
        case VideoStream::VideoFrameType::GeneratedKeyFrame:
            video_hdr [0] |= 0x40;
            break;
        case VideoStream::VideoFrameType::CommandFrame:
            video_hdr [0] |= 0x50;
            break;
        case VideoStream::VideoFrameType::AvcSequenceHeader:
            video_hdr [0] |= 0x10; // AVC seekable frame
            video_hdr [1] = 0;     // AVC sequence header
            break;
        case VideoStream::VideoFrameType::AvcEndOfSequence:
            video_hdr [0] |= 0x10; // AVC seekable frame
            video_hdr [1] = 2;     // AVC end of sequence
            break;
        case VideoStream::VideoFrameType::RtmpSetMetaData:
        case VideoStream::VideoFrameType::RtmpClearMetaData:
            assert (0 && "unexpected frame type");
    }

    memcpy (mem.mem(), video_hdr, video_hdr_len);
    return video_hdr_len;
}

}

