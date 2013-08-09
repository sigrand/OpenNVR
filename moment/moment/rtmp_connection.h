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


#ifndef LIBMOMENT__RTMP_CONNECTION__H__
#define LIBMOMENT__RTMP_CONNECTION__H__


#include <libmary/libmary.h>
#include <moment/amf_encoder.h>
#include <moment/amf_decoder.h>
#include <moment/video_stream.h>


struct iovec;

namespace Moment {

using namespace M;

class RtmpConnection_OutputQueue_name;

class RtmpConnection : public DependentCodeReferenced,
		       public IntrusiveListElement<RtmpConnection_OutputQueue_name>
{
private:
    // Protects part of receiving state which may be accessed from
    // ~RtmpConnection() destructor.
    // A simple release fence would be enough to synchronize with the destructor.
    // Using a mutex is an overkill. We can switch to using a fence after
    // an implementation of C++0x atomics comes out.
    //
    // It'd be optimal to use a fence just once for each invocation of doProcessInput()
    // and only if there was a change to the part of the state which the destructor
    // needs.
#warning TODO get rid of in_destr_mutex by malloc'ing' part of RtmpConnection which is accessed
#warning      from receiver's' sync domain and using DeferredProcessor to schedule its deletion.
    Mutex in_destr_mutex;

    // Protects sending state.
    Mutex send_mutex;

public:
    enum {
	DefaultDataChunkStreamId  = 3,
	DefaultAudioChunkStreamId = 4,
	DefaultVideoChunkStreamId = 5
    };

    enum {
	MinChunkSize = 128,
	MaxChunkSize = 65536,
	PrechunkSize = 65536,
//	PrechunkSize = 128,
	DefaultChunkSize = 128
    };

    struct ConnectionInfo
    {
        bool momentrtmp_proto;
    };

    struct Frontend
    {
	Result (*handshakeComplete) (void *cb_data);

	Result (*commandMessage) (VideoStream::Message * mt_nonnull msg,
				  Uint32                msg_stream_id,
                                  AmfEncoding           amf_encoding,
                                  ConnectionInfo       * mt_nonnull conn_info,
				  void                 *cb_data);

	Result (*audioMessage) (VideoStream::AudioMessage * mt_nonnull msg,
				void                      *cb_data);

	Result (*videoMessage) (VideoStream::VideoMessage * mt_nonnull msg,
				void                      *cb_data);

	void (*sendStateChanged) (Sender::SendState send_state,
				  void *cb_data);

	void (*closed) (Exception *exc_ mt_exc_kind ((IoException, InternalException)),
			void *cb_data);
    };

    struct Backend
    {
	void (*close) (void *cb_data);
    };

private:
    typedef IntrusiveList<RtmpConnection, RtmpConnection_OutputQueue_name> OutputQueue;

public: // Temporally public
    enum {
	//  3 bytes - chunk basic header;
	// 11 bytes - chunk message header (type 0);
	//  4 bytes - extended timestamp;
	//  3 bytes - fix chunk basic header;
	//  7 bytes - fix chunk message header (type 1).
        //  5 bytes - FLV VIDEODATA packet header
        MaxHeaderLen = 33
    };
private:

    class ReceiveState
    {
    public:
	enum Value {
	    Invalid,

	    ClientWaitS0,
	    ClientWaitS1,
	    ClientWaitS2,

	    ServerWaitC0,
	    ServerWaitC1,
	    ServerWaitC2,

	    BasicHeader,

	    ChunkHeader_Type0,
	    ChunkHeader_Type1,
	    ChunkHeader_Type2,
	    ChunkHeader_Type3,

	    ExtendedTimestamp,
	    ChunkData
	};
	operator Value () const { return value; }
	ReceiveState (Value const value) : value (value) {}
	ReceiveState () {}
    private:
	Value value;
    };

    class CsIdFormat
    {
    public:
	enum Value {
	    Unknown,
	    OneByte,
	    TwoBytes_First,
	    TwoBytes_Second
	};
	operator Value () const { return value; }
	CsIdFormat (Value const value) : value (value) {}
	CsIdFormat () {}
    private:
	Value value;
    };

public:
    class RtmpMessageType
    {
    public:
	enum Value {
	    SetChunkSize      =  1,
	    Abort             =  2,
	    Ack               =  3,
	    UserControl       =  4,
	    WindowAckSize     =  5,
	    SetPeerBandwidth  =  6,
	    AudioMessage      =  8,
	    VideoMessage      =  9,
	    Data_AMF3         = 15,
	    Data_AMF0         = 18,
	    SharedObject_AMF3 = 16,
	    SharedObject_AMF0 = 19,
	    Command_AMF3      = 17,
	    Command_AMF0      = 20,
	    Aggregate         = 22
	};
	operator Uint32 () const { return value; }
	RtmpMessageType (Uint32 const value) : value (value) {}
	RtmpMessageType () {}
	Size toString_ (Memory const &mem, Format const &fmt) const;
    private:
	Uint32 value;
    };

    enum {
	CommandMessageStreamId = 0
    };

    enum {
	DefaultMessageStreamId = 1
    };

private:
    class UserControlMessageType
    {
    public:
	enum Value {
	    StreamBegin      = 0,
	    StreamEof        = 1,
	    StreamDry        = 2,
	    SetBufferLength  = 3,
	    StreamIsRecorded = 4,
	    PingRequest      = 6,
	    PingResponse     = 7,
            BufferEmpty      = 31,
            BufferReady      = 32
	};
	operator Value () const { return value; }
	UserControlMessageType (Value const value) : value (value) {}
	UserControlMessageType () {}
    private:
	Value value;
    };

    enum {
	Type0_HeaderLen = 11,
	Type1_HeaderLen =  7,
	Type2_HeaderLen =  3,
	Type3_HeaderLen =  0
    };

public:
    class PrechunkContext
    {
	friend class RtmpConnection;

    private:
	Size prechunk_offset;

    public:
	void reset () {  prechunk_offset = 0; }

	PrechunkContext (Size const initial_offset = 0)
	    : prechunk_offset (initial_offset)
	{}
    };

    class ChunkStream : public BasicReferenced
    {
	friend class RtmpConnection;

    private:
	Uint32 chunk_stream_id;

	// Incoming message accumulator.
	mt_mutex (RtmpConnection::in_destr_mutex) PagePool::PageListHead page_list;

	PrechunkContext in_prechunk_ctx;

	Size in_msg_offset;

        Uint64 in_msg_timestamp_low;
	Uint64 in_msg_timestamp;
	Uint64 in_msg_timestamp_delta;
	Uint32 in_msg_len;
	Uint32 in_msg_type_id;
	Uint32 in_msg_stream_id;
	bool   in_header_valid;

	mt_mutex (RtmpConnection::send_mutex)
	mt_begin
	  Uint64 out_msg_timestamp;
	  Uint64 out_msg_timestamp_delta;
	  Uint32 out_msg_len;
	  Uint32 out_msg_type_id;
	  Uint32 out_msg_stream_id;
	  bool   out_header_valid;
	mt_end

    public:
	Uint32 getChunkStreamId () const { return chunk_stream_id; }
    };

private:
    mt_const DataDepRef<Timers>   timers;
    mt_const DataDepRef<PagePool> page_pool;
    mt_const DataDepRef<Sender>   sender;

    mt_const bool prechunking_enabled;
    mt_const Time send_delay_millisec;
    mt_const Time ping_timeout_millisec;

    mt_const Cb<Frontend> frontend;
    mt_const Cb<Backend> backend;

    mt_mutex (in_destr_mutex) Timers::TimerKey ping_send_timer;
    AtomicInt ping_reply_received;
    AtomicInt ping_timeout_expired_once;

    mt_sync_domain (receiver) Size in_chunk_size;
    mt_mutex (send_mutex) Size out_chunk_size;

    mt_mutex (send_mutex) Size out_got_first_timestamp;
    mt_mutex (send_mutex) Uint64 out_first_timestamp;
    mt_mutex (send_mutex) Count out_first_frames_counter;

    mt_mutex (send_mutex) Time out_last_flush_time;

    mt_sync_domain (receiver) bool extended_timestamp_is_delta;
    mt_sync_domain (receiver) bool ignore_extended_timestamp;

    mt_sync_domain (receiver) bool processing_input;

    typedef AvlTree< Ref<ChunkStream>,
		     MemberExtractor< ChunkStream,
				      Uint32 const,
				      &ChunkStream::chunk_stream_id >,
		     DirectComparator<Uint32> >
	    ChunkStreamTree;

    mt_mutex (in_destr_mutex) ChunkStreamTree chunk_stream_tree;

  // Receiving state

    mt_sync_domain (receiver)
    mt_begin
      Uint32 remote_wack_size;

      Size total_received;
      Size last_ack;

      Uint16 cs_id;
      CsIdFormat cs_id__fmt;
      Size chunk_offset;

      ChunkStream *recv_chunk_stream;

      Byte fmt;

      ReceiveState conn_state;

      // Can be set from 'receiver' when holding 'send_mutex'.
      // For sending; can be set as mt_const at init.
      mt_mutex (send_mutex) bool momentrtmp_proto;
    mt_end

  // Sending state

    mt_const Uint32 local_wack_size;

    static bool timestampGreater (Uint32 const left,
				  Uint32 const right)
    {
	Uint32 delta;
	if (right >= left)
	    delta = right - left;
	else
	    delta = (0xffffffff - left) + right + 1;

	return delta < 0x80000000 ? 1 : 0;
    }

public:
    mt_sync_domain (receiver) Receiver::ProcessInputResult doProcessInput (ConstMemory  mem,
									   Size        * mt_nonnull ret_accepted);

    class MessageDesc;

private:
    mt_mutex (send_mutex) Uint32 mangleOutTimestamp (MessageDesc const * mt_nonnull mdesc);

    mt_mutex (send_mutex) Size fillMessageHeader (MessageDesc const * mt_nonnull mdesc,
                                                  Size               msg_len,
						  ChunkStream       * mt_nonnull chunk_stream,
						  Byte              * mt_nonnull header_buf,
						  Uint64             timestamp,
						  Uint32             prechunk_size);

    mt_sync_domain (receiver) void resetChunkRecvState ();
    mt_sync_domain (receiver) void resetMessageRecvState (ChunkStream * mt_nonnull chunk_stream);

    mt_mutex (in_destr_mutex) void releaseChunkStream (ChunkStream * mt_nonnull chunk_stream);

public:
    mt_const ChunkStream *control_chunk_stream;
    mt_const ChunkStream *data_chunk_stream;
    mt_const ChunkStream *audio_chunk_stream;
    mt_const ChunkStream *video_chunk_stream;

    mt_one_of(( mt_const, mt_sync_domain (receiver) ))
	ChunkStream* getChunkStream (Uint32 chunk_stream_id,
				     bool create);

    static void fillPrechunkedPages (PrechunkContext        *prechunk_ctx,
				     ConstMemory const      &mem,
				     PagePool               *page_pool,
				     PagePool::PageListHead *page_list,
				     Uint32                  chunk_stream_id,
				     Uint64                  msg_timestamp,
				     bool                    first_chunk);

  // Send methods.

    // Message description for sending.
    class MessageDesc
    {
    public:
	Uint64 timestamp;
	Uint32 msg_type_id;
	Uint32 msg_stream_id;
	Size msg_len;
	// Chunk stream header compression.
	bool cs_hdr_comp;

        bool adjustable_timestamp;

        MessageDesc ()
            : adjustable_timestamp (false)
        {
        }

	// TODO ChunkStream *chunk_stream;
    };

    // @prechunk_size - If 0, then message data is not prechunked.
    void sendMessage (MessageDesc  const * mt_nonnull mdesc,
		      ChunkStream        * mt_nonnull chunk_stream,
		      ConstMemory  const &mem,
		      Uint32              prechunk_size,
		      bool                unlocked = false);

    // TODO 'page_pool' parameter is needed.
    // @prechunk_size - If 0, then message data is not prechunked.
    void sendMessagePages (MessageDesc const      * mt_nonnull mdesc,
			   ChunkStream            * mt_nonnull chunk_stream,
			   PagePool::PageListHead * mt_nonnull page_list,
			   Size                    msg_offset,
			   Uint32                  prechunk_size,
			   bool                    take_ownership,
			   bool                    unlocked,
                           Byte const             *extra_header_buf,
                           unsigned                extra_header_len);

    void sendRawPages (PagePool::Page *first_page);

  // Send utility methods.

    void sendSetChunkSize (Uint32 chunk_size);

private:
    void sendSetChunkSize_unlocked (Uint32 chunk_size);

    void doSendSetChunkSize (Uint32 chunk_size,
			     bool   unlocked);

public:
    void sendAck (Uint32 seq);

    void sendWindowAckSize (Uint32 wack_size);

    void sendSetPeerBandwidth (Uint32 wack_size,
			       Byte   limit_type);

    void sendUserControl_StreamBegin (Uint32 msg_stream_id);

    void sendUserControl_SetBufferLength (Uint32 msg_stream_id,
					  Uint32 buffer_len);

    void sendUserControl_StreamIsRecorded (Uint32 msg_stream_id);

    void sendUserControl_PingRequest ();

    void sendUserControl_PingResponse (Uint32 timestamp);

    void sendDataMessage_AMF0 (Uint32 msg_stream_id,
                               ConstMemory mem);

    void sendDataMessage_AMF0_Pages (Uint32                  msg_stream_id,
                                     PagePool::PageListHead * mt_nonnull page_list,
                                     Size                    msg_offset,
                                     Size                    msg_len,
                                     Uint32                  prechunk_size);

    void sendCommandMessage_AMF0 (Uint32 msg_stream_id,
				  ConstMemory const &mem);

    void sendCommandMessage_AMF0_Pages (Uint32                  msg_stream_id,
					PagePool::PageListHead * mt_nonnull page_list,
					Size                    msg_offset,
					Size                    msg_len,
					Uint32                  prechunk_size);

  // Extra send utility methods.

    void sendVideoMessage (VideoStream::VideoMessage * mt_nonnull msg);

    void sendAudioMessage (VideoStream::AudioMessage * mt_nonnull msg);

    void sendConnect (ConstMemory const &app_name);

    void sendCreateStream ();

    void sendPlay (ConstMemory const &stream_name);

    void sendPublish (ConstMemory const &stream_name);

  // ______

    void closeAfterFlush ();

    void close ();

    // Useful for controlling RtmpConnection's state from the backend.
    void close_noBackendCb ();

private:
  // Ping timer

    mt_const void beginPings ();

    static void pingTimerTick (void *_self);

  // ___

    mt_sync_domain (receiver) Result processMessage (ChunkStream *chunk_stream);

    mt_sync_domain (receiver) Result callCommandMessage (ChunkStream *chunk_stream,
							 AmfEncoding amf_encoding);

    mt_sync_domain (receiver) Result processUserControlMessage (ChunkStream *chunk_stream);

    mt_iface (Sender::Frontend)
      static Sender::Frontend const sender_frontend;

      static void senderStateChanged (Sender::SendState  send_state,
				      void              *_self);

      static void senderClosed (Exception *exc_,
				void      *_self);
    mt_iface_end

    mt_iface (Receiver::Frontend)
      static Receiver::Frontend const receiver_frontend;

      mt_sync_domain (receiver)
      mt_begin
	static Receiver::ProcessInputResult processInput (Memory const &mem,
							  Size * mt_nonnull ret_accepted,
							  void *_self);

	static void processEof (void *_self);

	static void processError (Exception *exc_,
				  void      *_self);
      mt_end
    mt_iface_end

    void doError (Exception *exc_);

public:
    // TODO setReceiver()
    CbDesc<Receiver::Frontend> getReceiverFrontend ()
        { return CbDesc<Receiver::Frontend> (&receiver_frontend, this, getCoderefContainer()); }

    mt_const void setFrontend (CbDesc<Frontend> const &frontend)
        { this->frontend = frontend; }

    mt_const void setBackend (CbDesc<Backend> const &backend)
        { this->backend = backend; }

    mt_const void setSender (Sender * const sender)
    {
	this->sender = sender;
	sender->setFrontend (CbDesc<Sender::Frontend> (&sender_frontend, this, getCoderefContainer()));
    }

    Sender* getSender () const { return sender; }

    mt_const void startClient ();
    mt_const void startServer ();

    // Should be called from frontend->commandMessage() callback only.
    Uint32 getLocalWackSize () const { return local_wack_size; }

    // Should be called from frontend->commandMessage() callback only.
    Uint32 getRemoteWackSize () const { return remote_wack_size; }

  // TODO doConnect(), doCreateStream(), etc. belong to RtmpServer.

    Result doCreateStream (Uint32      msg_stream_id,
			   AmfDecoder * mt_nonnull amf_decoder);

    Result doReleaseStream (Uint32      msg_stream_id,
			    AmfDecoder * mt_nonnull amf_decoder);

    Result doCloseStream (Uint32      msg_steam_id,
			  AmfDecoder * mt_nonnull amf_decoder);

    Result doDeleteStream (Uint32      msg_stream_id,
			   AmfDecoder * mt_nonnull amf_decoder);

    // Parses transaction_id from amf_decoder and sends a basic reply.
    Result doBasicMessage (Uint32      msg_stream_id,
	    		   AmfDecoder * mt_nonnull amf_decoder);

    Result fireVideoMessage (VideoStream::VideoMessage * mt_nonnull video_msg);

    void reportError ();

    mt_const void init (Timers   * mt_nonnull timers,
			PagePool * mt_nonnull page_pool,
			Time      send_delay_millisec,
                        Time      ping_timeout_millisec,
                        bool      prechunking_enabled,
                        bool      momentrtmp_proto);

    RtmpConnection (Object *coderef_container);

    ~RtmpConnection ();

  // Utility functions

    static Size normalizePrechunkedData (VideoStream::Message    * mt_nonnull msg,
					 PagePool                * mt_nonnull page_pool,
					 PagePool               ** mt_nonnull ret_page_pool,
					 PagePool::PageListHead  * mt_nonnull ret_page_list,
					 Size                    * mt_nonnull ret_msg_offs);
};

}


#endif /* LIBMOMENT__RTMP_CONNECTION__H__ */

