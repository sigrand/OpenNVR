/*  Moment Video Server - High performance media server
    Copyright (C) 2011-2013 Dmitry Shatrov
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


#include <moment/amf_encoder.h>
#include <moment/amf_decoder.h>
#include <moment/inc.h>

#include <moment/rtmp_server.h>


using namespace M;

namespace Moment {

static LogGroup libMary_logGroup_rtmp_server ("rtmp_server", LogLevel::I);

MOMENT__RTMP_SERVER

void
RtmpServer::sendRtmpSampleAccess (Uint32 const msg_stream_id,
                                  bool   const allow_a,
                                  bool   const allow_b)
{
  // Sending |RtmpSampleAccess

//    logD_ (_this_func, " (", msg_stream_id, ", ", allow_a, ", ", allow_b, ")");

    AmfAtom atoms [3];
    AmfEncoder encoder (atoms);

    encoder.addString ("|RtmpSampleAccess");
    encoder.addBoolean (allow_a);
    encoder.addBoolean (allow_b);

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
        logE_ (_func, "could not encode |RtmpSampleAccess message");
        return;
    }

    rtmp_conn->sendDataMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
}

Result
RtmpServer::doConnect (Uint32       const msg_stream_id,
		       AmfDecoder * const mt_nonnull decoder)
{
    double transaction_id;
    if (!decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

#if 0
    if (!decoder->skipObject ()) {
	logE_ (_func, "could not skip command object");
	return Result::Failure;
    }
#endif

    if (!decoder->beginObject ()) {
	logE_ (_func, "no command object");
	return Result::Failure;
    }

    Byte app_name_buf [1024];
    Size app_name_len = 0;
    double object_encoding = 0.0;
    while (!decoder->isObjectEnd()) {
	Byte field_name_buf [512];
	Size field_name_len;
	Size field_name_full_len;
	if (!decoder->decodeFieldName (Memory::forObject (field_name_buf), &field_name_len, &field_name_full_len)) {
	    logE_ (_func, "no \"app\" field in the command object");
	    return Result::Failure;
	}

	if (equal (ConstMemory (field_name_buf, field_name_len), "app")) {
	    Size app_name_full_len;
	    if (!decoder->decodeString (Memory::forObject (app_name_buf), &app_name_len, &app_name_full_len)) {
		logE_ (_func, "could not decode app name");
		return Result::Failure;
	    }
	    if (app_name_full_len > app_name_len) {
		logW_ (_func, "app name length exceeds limit "
		       "(length ", app_name_full_len, " bytes, limit ", sizeof (app_name_buf), " bytes)");
	    }
	} else
        if (equal (ConstMemory (field_name_buf, field_name_len), "objectEncoding")) {
            double number;
            if (!decoder->decodeNumber (&number)) {
                logE_ (_func, "could not decode objectEncoding");
                return Result::Failure;
            }
            object_encoding = number;
        } else
	if (!decoder->skipValue ()) {
	    logE_ (_func, "could not skip field value");
	    return Result::Failure;
	}
    }

    rtmp_conn->sendWindowAckSize (rtmp_conn->getLocalWackSize());
    rtmp_conn->sendSetPeerBandwidth (rtmp_conn->getRemoteWackSize(), 2 /* dynamic limit */);
    rtmp_conn->sendUserControl_StreamBegin (0 /* msg_stream_id */);

    sendRtmpSampleAccess (RtmpConnection::DefaultMessageStreamId, true, true);

    {
	AmfAtom atoms [25];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);

	{
	    encoder.beginObject ();

	    encoder.addFieldName ("fmsVer");
            // TODO FMLE doesn't allow DVR if we don't pretend to be FMS>=3.5.
            //      Make this configurable and default to 'FMS' when rtmp recording
            //      is enabled.
	    encoder.addString ("MMNT/0,1,0,0");
//	    encoder.addString ("FMS/3,5,7,7009");

	    encoder.addFieldName ("capabilities");
	    // TODO Define capabilities. Docs?
	    encoder.addNumber (31.0);

	    encoder.addFieldName ("mode");
	    encoder.addNumber (1.0);

	    encoder.endObject ();
	}

	{
	    encoder.beginObject ();

	    encoder.addFieldName ("level");
	    encoder.addString ("status");

	    encoder.addFieldName ("code");
	    encoder.addString ("NetConnection.Connect.Success");

	    encoder.addFieldName ("description");
	    encoder.addString ("Connection succeeded.");

	    encoder.addFieldName ("objectEncoding");
	    encoder.addNumber (object_encoding);

#if 0
// This seems to be unnecessary

	    encoder.addFieldName ("data");
	    encoder.beginEcmaArray (0);

	    encoder.addFieldName ("version");
	    encoder.addString ("3,5,7,7009");

	    encoder.endEcmaArray ();
#endif

	    encoder.endObject ();
	}

	Byte msg_buf [1024];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "encode() failed");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));

#if 0
	logD (proto_out, _func, "msg_len: ", msg_len);
	if (logLevelOn (proto_out, LogLevel::Debug))
	    hexdump (ConstMemory (msg_buf, msg_len));
#endif
    }

    if (frontend && frontend->connect) {
	Result res;
	if (!frontend.call_ret<Result> (&res, frontend->connect, /*(*/ ConstMemory (app_name_buf, app_name_len) /*)*/)) {
	    logE_ (_func, "frontend gone");
	    return Result::Failure;
	}

	if (!res) {
	    logE_ (_func, "frontend->connect() failed");
	    return Result::Failure;
	}
    }

    return Result::Success;
}

namespace {
class StartRtmpWatchingCallback_Data : public Referenced
{
public:
    RtmpServer *rtmp_server;
    Uint32 msg_stream_id;
};
}

void
RtmpServer::startRtmpWatchingCallback (Result   const res,
                                       void   * const _data)
{
    StartRtmpWatchingCallback_Data * const data = static_cast <StartRtmpWatchingCallback_Data*> (_data);
    RtmpServer * const self = data->rtmp_server;

    if (!res) {
        self->rtmp_conn->reportError ();
        return;
    }

    self->completePlay (data->msg_stream_id);
}

void
RtmpServer::completePlay (Uint32 const /* msg_stream_id */)
{
    // sendRtmpSampleAccess (msg_stream_id, true, true);
}

Result
RtmpServer::doPlay (Uint32       const msg_stream_id,
		    AmfDecoder * const mt_nonnull decoder)
{
    logD (rtmp_server, _func_);

    if (playing.get()) {
	logW_ (_func, "already playing");
	return Result::Success;
    }
    playing.set (1);

    double transaction_id;
    if (!decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    if (!decoder->skipObject ()) {
	logE_ (_func, "could not skip command object");
	return Result::Failure;
    }

    Byte vs_name_buf [512];
    Size vs_name_len;
    Size vs_name_full_len;
    if (!decoder->decodeString (Memory::forObject (vs_name_buf), &vs_name_len, &vs_name_full_len)) {
	logE_ (_func, "could not decode video stream name");
	return Result::Failure;
    }
    if (vs_name_full_len > vs_name_len) {
	logW_ (_func, "video stream name length exceeds limit "
	       "(length ", vs_name_full_len, " bytes, limit ", sizeof (vs_name_buf), " bytes)");
    }

    {
	AmfAtom atoms [4];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);
	encoder.addNullObject ();
	encoder.addNullObject ();

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode reply");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    {
      // Sending onStatus reply "Reset".

	AmfAtom atoms [15];
	AmfEncoder encoder (atoms);

	encoder.addString ("onStatus");
	encoder.addNumber (0.0 /* transaction_id */);
	encoder.addNullObject ();

	encoder.beginObject ();

	encoder.addFieldName ("level");
	encoder.addString ("status");

	encoder.addFieldName ("code");
	encoder.addString ("NetStream.Play.Reset");

	Ref<String> description_str = makeString ("Playing and resetting ", ConstMemory (vs_name_buf, vs_name_len), ".");
	encoder.addFieldName ("description");
	encoder.addString (description_str->mem());

	encoder.addFieldName ("details");
	encoder.addString (ConstMemory (vs_name_buf, vs_name_len));

	encoder.addFieldName ("clientid");
	encoder.addNumber (1.0);

	encoder.endObject ();

	Byte msg_buf [4096];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode onStatus message");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    // TODO 

//    if (!playing) {
	// TODO Send StreamIsRecorded for non-live streams.
        //      From the docs, it appears to affect flash player's buffering strategy.
//        rtmp_conn->sendUserControl_StreamIsRecorded (msg_stream_id);
	rtmp_conn->sendUserControl_StreamBegin (msg_stream_id);
//    }

    sendRtmpSampleAccess (msg_stream_id, true, true);

    {
      // Sending onStatus reply "Start".

	AmfAtom atoms [15];
	AmfEncoder encoder (atoms);

	encoder.addString ("onStatus");
	encoder.addNumber (0.0 /* transaction_id */);
	encoder.addNullObject ();

	encoder.beginObject ();

	encoder.addFieldName ("level");
	encoder.addString ("status");

	encoder.addFieldName ("code");
	encoder.addString ("NetStream.Play.Start");

	Ref<String> description_str = makeString ("Started playing ", ConstMemory (vs_name_buf, vs_name_len), ".");
	encoder.addFieldName ("description");
	encoder.addString (description_str->mem());

	encoder.addFieldName ("details");
	encoder.addString (ConstMemory (vs_name_buf, vs_name_len));

	encoder.addFieldName ("clientid");
	encoder.addNumber (1.0);

	encoder.endObject ();

	Byte msg_buf [4096];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode onStatus message");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    {
        Ref<StartRtmpWatchingCallback_Data> const data = grab (new StartRtmpWatchingCallback_Data);
        data->rtmp_server = this;
        data->msg_stream_id = msg_stream_id;

	Result res = Result::Failure;
        bool complete = false;
	if (!frontend.call_ret<bool> (
                    &complete,
                    frontend->startRtmpWatching,
                    /*(*/
                        ConstMemory (vs_name_buf, vs_name_len),
                        CbDesc<StartRtmpWatchingCallback> (startRtmpWatchingCallback,
                                                           data,
                                                           getCoderefContainer(),
                                                           data),
                        &res
                    /*)*/))
        {
	    logE_ (_func, "frontend gone");
	    return Result::Failure;
	}

        if (!complete)
            return Result::Success;

	if (!res) {
//	    logD_ (_func, "frontend->startWatching() failed");
	    return Result::Failure;
	}
    }

    completePlay (msg_stream_id);

    return Result::Success;
}

Result
RtmpServer::doPause (Uint32       const msg_stream_id,
		     AmfDecoder * const mt_nonnull decoder)
{
    logD (rtmp_server, _func_);

    if (!playing.get()) {
	logW_ (_func, "not playing");
	return Result::Success;
    }

    double transaction_id;
    if (!decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    if (!decoder->skipObject ()) {
	logE_ (_func, "could not skip command object");
	return Result::Failure;
    }

    bool is_pause;
    if (!decoder->decodeBoolean (&is_pause)) {
	logE_ (_func, "could not decode boolean");
	return Result::Failure;
    }

    {
	AmfAtom atoms [4];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);
	encoder.addNullObject ();

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode reply");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    {
	Result res;
	if (!frontend.call_ret<Result> (&res, (is_pause ? frontend->pause : frontend->resume) /*(*/ /*)*/)) {
	    logE_ (_func, "frontend gone");
	    return Result::Failure;
	}

	if (!res)
	    return Result::Failure;
    }

    return Result::Success;
}

namespace {
class StartRtmpStreamingCallback_Data : public Referenced
{
public:
    RtmpServer *rtmp_server;

    Uint32 msg_stream_id;
    double transaction_id;
    Ref<String> vs_name;
};
}

void
RtmpServer::startRtmpStreamingCallback (Result   const res,
                                        void   * const _data)
{
    StartRtmpStreamingCallback_Data * const data = static_cast <StartRtmpStreamingCallback_Data*> (_data);
    RtmpServer * const self = data->rtmp_server;

    if (!res) {
        self->rtmp_conn->reportError ();
        return;
    }

    if (!self->completePublish (data->msg_stream_id,
                                data->transaction_id,
                                data->vs_name->mem()))
    {
        self->rtmp_conn->reportError ();
        return;
    }
}

Result
RtmpServer::completePublish (Uint32      const msg_stream_id,
                             double      const transaction_id,
                             ConstMemory const vs_name)
{
    // TODO Subscribe for translation stop? (probably not here)

    // TODO The following reply sends are probably unnecessary - needs checking.

    rtmp_conn->sendUserControl_StreamBegin (msg_stream_id);

    {
      // TODO sendSimpleResult

	AmfAtom atoms [4];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);
	encoder.addNullObject ();
	encoder.addNullObject ();

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode reply");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

#if 0
    {
      // Sending onStatus reply.

	AmfAtom atoms [5];
	AmfEncoder encoder (atoms);

	encoder.addString ("onStatus");
	encoder.addNumber (0.0 /* transaction_id */);
	encoder.addNullObject ();
	encoder.addString (vs_name);
	encoder.addString ("live");

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode onStatus message");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }
#endif

    {
      // Sending onStatus reply "Reset".

	AmfAtom atoms [15];
	AmfEncoder encoder (atoms);

	encoder.addString ("onStatus");
	encoder.addNumber (0.0 /* transaction_id */);
	encoder.addNullObject ();

	encoder.beginObject ();

	encoder.addFieldName ("level");
	encoder.addString ("status");

	encoder.addFieldName ("code");
	encoder.addString ("NetStream.Play.Reset");

	Ref<String> description_str = makeString ("Playing and resetting ", vs_name, ".");
	encoder.addFieldName ("description");
	encoder.addString (description_str->mem());

	encoder.addFieldName ("details");
	encoder.addString (vs_name);

	encoder.addFieldName ("clientid");
	encoder.addNumber (1.0);

	encoder.endObject ();

	Byte msg_buf [4096];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode onStatus message");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    {
      // Sending onStatus reply "Start".

	AmfAtom atoms [15];
	AmfEncoder encoder (atoms);

	encoder.addString ("onStatus");
	encoder.addNumber (0.0 /* transaction_id */);
	encoder.addNullObject ();

	encoder.beginObject ();

	encoder.addFieldName ("level");
	encoder.addString ("status");

	encoder.addFieldName ("code");
	encoder.addString ("NetStream.Play.Start");

	Ref<String> description_str = makeString ("Started playing ", vs_name, ".");
	encoder.addFieldName ("description");
	encoder.addString (description_str->mem());

	encoder.addFieldName ("details");
	encoder.addString (vs_name);

	encoder.addFieldName ("clientid");
	encoder.addNumber (1.0);

	encoder.endObject ();

	Byte msg_buf [4096];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode onStatus message");
	    return Result::Failure;
	}

	rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    return Result::Success;
}

Result
RtmpServer::doPublish (Uint32       const msg_stream_id,
		       AmfDecoder * const mt_nonnull decoder,
                       RtmpConnection::ConnectionInfo * const mt_nonnull conn_info)
{
    logD (rtmp_server, _func_);

    double transaction_id;
    if (!decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    if (!decoder->skipObject ()) {
	logE_ (_func, "could not skip command object");
	return Result::Failure;
    }

    Byte vs_name_buf [512];
    Size vs_name_len;
    Size vs_name_full_len;
    if (!decoder->decodeString (Memory::forObject (vs_name_buf), &vs_name_len, &vs_name_full_len)) {
	logE_ (_func, "could not decode video stream name");
	return Result::Failure;
    }
    if (vs_name_full_len > vs_name_len) {
	logW_ (_func, "video stream name length exceeds limit "
	       "(length ", vs_name_full_len, " bytes, limit ", sizeof (vs_name_buf), " bytes)");
    }

    RecordingMode rec_mode = RecordingMode::NoRecording;
    {
	Byte rec_mode_buf [512];
	Size rec_mode_len;
	Size rec_mode_full_len;
	if (decoder->decodeString (Memory::forObject (rec_mode_buf), &rec_mode_len, &rec_mode_full_len)) {
	    if (rec_mode_full_len > rec_mode_len) {
		logW_ (_func, "recording mode length exceeds limit "
		       "(length ", rec_mode_full_len, " bytes, limit ", sizeof (rec_mode_buf), " bytes)");
	    } else {
		ConstMemory const rec_mode_mem (rec_mode_buf, rec_mode_len);
		if (equal (rec_mode_mem, "live"))
		    rec_mode = RecordingMode::NoRecording;
		else
		if (equal (rec_mode_mem, "record"))
		    rec_mode = RecordingMode::Replace;
		else
		if (equal (rec_mode_mem, "append"))
		    rec_mode = RecordingMode::Append;
		else
		    logW_ (_func, "unknown recording mode: ", rec_mode_mem);
	    }
	} else {
	    logD_ (_func, "could not decode recording mode");
	}
    }

    if (frontend && frontend->startRtmpStreaming) {
        Ref<StartRtmpStreamingCallback_Data> const data = grab (new StartRtmpStreamingCallback_Data);
        data->rtmp_server = this;
        data->msg_stream_id = msg_stream_id;
        data->transaction_id = transaction_id;
        data->vs_name = grab (new String (ConstMemory (vs_name_buf, vs_name_len)));

	Result res = Result::Failure;
        bool complete = false;
	if (!frontend.call_ret<bool> (
                    &complete,
                    frontend->startRtmpStreaming,
		    /*(*/
                        ConstMemory (vs_name_buf, vs_name_len),
                        rec_mode,
                        conn_info->momentrtmp_proto,
                        CbDesc<StartRtmpStreamingCallback> (startRtmpStreamingCallback,
                                                            data,
                                                            getCoderefContainer(),
                                                            data),
                        &res
                    /*)*/))
	{
	    logE_ (_func, "frontend gone");
	    return Result::Failure;
	}

        if (!complete)
            return Result::Success;

	if (!res) {
	    logE_ (_func, "frontend->startStreaming() failed");
	    return Result::Failure;
	}
    }

    return completePublish (msg_stream_id,
                            transaction_id,
                            ConstMemory (vs_name_buf, vs_name_len));
}

VideoStream::FrameSaver::FrameHandler const RtmpServer::saved_frame_handler = {
    savedAudioFrame,
    savedVideoFrame
};

Result
RtmpServer::savedAudioFrame (VideoStream::AudioMessage * const mt_nonnull audio_msg,
                             void                      * const _self)
{
    RtmpServer * const self = static_cast <RtmpServer*> (_self);
//    VideoStream::AudioMessage tmp_audio_msg = *audio_msg;
//    tmp_audio_msg.timestamp = 0;
//    self->rtmp_conn->sendAudioMessage (&tmp_audio_msg);
    self->rtmp_conn->sendAudioMessage (audio_msg);
    return Result::Success;
}

Result
RtmpServer::savedVideoFrame (VideoStream::VideoMessage * const mt_nonnull video_msg,
                             void                      * const _self)
{
    RtmpServer * const self = static_cast <RtmpServer*> (_self);
//    VideoStream::VideoMessage tmp_video_msg = *video_msg;
//    tmp_video_msg.timestamp = 0;
//    self->rtmp_conn->sendVideoMessage (&tmp_video_msg);
    self->rtmp_conn->sendVideoMessage (video_msg);
    return Result::Success;
}

void
RtmpServer::sendInitialMessages_unlocked (VideoStream::FrameSaver * const mt_nonnull frame_saver)
{
    frame_saver->reportSavedFrames (&saved_frame_handler, this);
}

Result
RtmpServer::commandMessage (VideoStream::Message * const mt_nonnull msg,
			    Uint32                 const msg_stream_id,
			    AmfEncoding            const amf_encoding,
                            RtmpConnection::ConnectionInfo * const mt_nonnull conn_info)
{
    logD (rtmp_server, _func_);

    if (msg->msg_len == 0)
	return Result::Success;

    if (amf_encoding == AmfEncoding::AMF3) {
        // AMF3 command messages have an extra leading (dummy?) byte.
        if (msg->msg_len < 1) {
            logW_ (_func, "AMF3 message is too short (no leading byte)");
            return Result::Success;
        }

        // empty message with a leading byte
        if (msg->msg_len == 1)
            return Result::Success;
    }

    Size const decoder_offset = (amf_encoding == AmfEncoding::AMF3 ? 1 : 0);
    PagePool::PageListArray pl_array (msg->page_list.first, decoder_offset, msg->msg_len - decoder_offset);
    AmfDecoder decoder (AmfEncoding::AMF0, &pl_array, msg->msg_len - decoder_offset);
//    // DEBUG
//    logD_ (_func, "msg dump (", amf_encoding, "):");
//    decoder.dump ();

    Byte method_name [256];
    Size method_name_len;
    if (!decoder.decodeString (Memory::forObject (method_name),
			       &method_name_len,
			       NULL /* ret_full_len */))
    {
	logE_ (_func, "could not decode method name");
	return Result::Failure;
    }

    logD (rtmp_server, _func, "method: ", ConstMemory (method_name, method_name_len));

    ConstMemory method_mem (method_name, method_name_len);
    if (equal (method_mem, "connect")) {
	return doConnect (msg_stream_id, &decoder);
    } else
    if (equal (method_mem, "createStream")) {
	return rtmp_conn->doCreateStream (msg_stream_id, &decoder);
    } else
    if (equal (method_mem, "FCPublish")) {
      // TEMPORAL TEST
	return rtmp_conn->doReleaseStream (msg_stream_id, &decoder);
    } else
    if (equal (method_mem, "releaseStream")) {
	return rtmp_conn->doReleaseStream (msg_stream_id, &decoder);
    } else
    if (equal (method_mem, "closeStream")) {
	return rtmp_conn->doCloseStream (msg_stream_id, &decoder);
    } else
    if (equal (method_mem, "deleteStream")) {
	return rtmp_conn->doDeleteStream (msg_stream_id, &decoder);
    } else
    if (equal (method_mem, "receiveVideo")) {
      // TODO
    } else
    if (equal (method_mem, "receiveAudio")) {
      // TODO
    } else
    if (equal (method_mem, "play")) {
	return doPlay (msg_stream_id, &decoder);
    } else
    if (equal (method_mem, "pause")) {
	return doPause (msg_stream_id, &decoder);
    } else
    if (equal (method_mem, "publish")) {
	return doPublish (msg_stream_id, &decoder, conn_info);
    } else
    if (equal (method_mem, "@setDataFrame")) {
	Size const msg_offset = decoder.getCurOffset () + decoder_offset;
	assert (msg_offset <= msg->msg_len);

	VideoStream::VideoMessage video_msg;
	video_msg.timestamp_nanosec = msg->timestamp_nanosec;
	video_msg.prechunk_size = msg->prechunk_size;
	video_msg.frame_type = VideoStream::VideoFrameType::RtmpSetMetaData;
	video_msg.codec_id = VideoStream::VideoCodecId::Unknown;

	video_msg.page_pool = msg->page_pool;
	video_msg.page_list = msg->page_list;
	video_msg.msg_len = msg->msg_len - msg_offset;
	video_msg.msg_offset = msg_offset;

	return rtmp_conn->fireVideoMessage (&video_msg);
    } else
    if (equal (method_mem, "@clearDataFrame")) {
	Size const msg_offset = decoder.getCurOffset () + decoder_offset;
	assert (msg_offset <= msg->msg_len);

	VideoStream::VideoMessage video_msg;
	video_msg.timestamp_nanosec = msg->timestamp_nanosec;
	video_msg.prechunk_size = msg->prechunk_size;
	video_msg.frame_type = VideoStream::VideoFrameType::RtmpClearMetaData;
	video_msg.codec_id = VideoStream::VideoCodecId::Unknown;

	video_msg.page_pool = msg->page_pool;
	video_msg.page_list = msg->page_list;
	video_msg.msg_len = msg->msg_len - msg_offset;
	video_msg.msg_offset = msg_offset;

	return rtmp_conn->fireVideoMessage (&video_msg);
    } else {
	if (frontend && frontend->commandMessage) {
	    CommandResult res;
	    if (!frontend.call_ret<CommandResult> (&res, frontend->commandMessage,
			 /*(*/ rtmp_conn, msg_stream_id, method_mem, msg, &decoder /*)*/))
	    {
		logE_ (_func, "frontend gone");
		return Result::Failure;
	    }

	    if (res == CommandResult::Failure) {
		logE_ (_func, "frontend->cmmandMessage() failed (", method_mem, ")");
		return Result::Failure;
	    }

	    if (res == CommandResult::UnknownCommand)
		logW_ (_func, "unknown method: ", method_mem);
	    else
		assert (res == CommandResult::Success);
	} else {
	    logW_ (_func, "unknown method: ", method_mem);
	}
    }

    return Result::Success;
}

Result
RtmpServer::encodeMetaData (MetaData                  * const mt_nonnull metadata,
			    PagePool                  * const mt_nonnull page_pool,
			    VideoStream::VideoMessage * const ret_msg)
{
    PagePool::PageListHead page_list;

    AmfAtom atoms [128];
    AmfEncoder encoder (atoms);

    encoder.addString ("onMetaData");

    encoder.beginEcmaArray (0 /* num_entries */);
    AmfAtom * const toplevel_array_atom = encoder.getLastAtom ();
    Uint32 num_entries = 0;

    if (metadata->got_flags & MetaData::VideoCodecId) {
	encoder.addFieldName ("videocodecid");
	encoder.addNumber (metadata->video_codec_id);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AudioCodecId) {
	encoder.addFieldName ("audiocodecid");
	encoder.addNumber (metadata->audio_codec_id);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::VideoWidth) {
	encoder.addFieldName ("width");
	encoder.addNumber (metadata->video_width);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::VideoHeight) {
	encoder.addFieldName ("height");
	encoder.addNumber (metadata->video_height);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AspectRatioX) {
	encoder.addFieldName ("AspectRatioX");
	encoder.addNumber (metadata->aspect_ratio_x);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AspectRatioY) {
	encoder.addFieldName ("AspectRatioY");
	encoder.addNumber (metadata->aspect_ratio_y);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::VideoDataRate) {
	encoder.addFieldName ("videodatarate");
	encoder.addNumber (metadata->video_data_rate);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AudioDataRate) {
	encoder.addFieldName ("audiodatarate");
	encoder.addNumber (metadata->audio_data_rate);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::VideoFrameRate) {
	encoder.addFieldName ("framerate");
	encoder.addNumber (metadata->video_frame_rate);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AudioSampleRate) {
	encoder.addFieldName ("audiosamplerate");
	encoder.addNumber (metadata->audio_sample_rate);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AudioSampleSize) {
	encoder.addFieldName ("audiosamplesize");
	encoder.addNumber (metadata->audio_sample_size);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::NumChannels) {
	encoder.addFieldName ("stereo");
	if (metadata->num_channels >= 2) {
	    encoder.addBoolean (true);
	} else {
	    encoder.addBoolean (false);
	}
	++num_entries;
    }

    encoder.endObject ();

    toplevel_array_atom->setEcmaArraySize (num_entries);

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	logE_ (_func, "encode() failed");
	return Result::Failure;
    }

    {
	RtmpConnection::PrechunkContext prechunk_ctx;
	RtmpConnection::fillPrechunkedPages (&prechunk_ctx,
					     ConstMemory (msg_buf, msg_len),
					     page_pool,
					     &page_list,
					     RtmpConnection::DefaultDataChunkStreamId,
					     0    /* timestamp */,
					     true /* first_chunk */);
    }

    if (ret_msg) {
	ret_msg->timestamp_nanosec = 0;
	ret_msg->prechunk_size = RtmpConnection::PrechunkSize;
	ret_msg->frame_type = VideoStream::VideoFrameType::RtmpSetMetaData;
	ret_msg->codec_id = VideoStream::VideoCodecId::Unknown;

	ret_msg->page_pool = page_pool;
	ret_msg->page_list = page_list;
	ret_msg->msg_len = msg_len;
	ret_msg->msg_offset = 0;
    }

    return Result::Success;
}

}

