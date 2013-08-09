/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov
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


#ifndef MOMENT__API__H__
#define MOMENT__API__H__


// Stable C API for external modules.


#include <stdlib.h>
#include <stdarg.h>


#ifdef __cplusplus
extern "C" {
#endif


enum MomentRecordingMode {
    MomentRecordingMode_NoRecording,
    MomentRecordingMode_Replace,
    MomentRecordingMode_Append
};

enum MomentResult {
    MomentResult_Success = 0,
    MomentResult_Failure
};


// _________________________________ Messages __________________________________

typedef struct MomentMessage MomentMessage;

void moment_message_get_data (MomentMessage *msg,
			      size_t         offset,
			      unsigned char *buf,
			      size_t         len);


// _______________________________ Stream events _______________________________

typedef struct MomentStreamHandler MomentStreamHandler;

// TODO This is a temporal form. It depends on VideoStream::AudioMessage.
typedef void (*MomentAudioMessageCallback) (void *_audio_msg,
					    void *user_data);

// TODO This is a temporal form. It depends on VideoStream::VideoMessage.
typedef void (*MomentVideoMessageCallback) (void *_video_msg,
					    void *user_data);

typedef void (*MomentRtmpDataMessageCallback) (MomentMessage *msg,
					       // TODO AMF decoder
					       void          *user_data);

typedef void (*MomentStreamClosedCallback) (void *user_data);

typedef void (*MomentNumWatchersChangedCallback) (unsigned long  num_watchers,
                                                  void          *user_data);

// TODO Add dtor_cb/cb_data pair to MomentStreamHandler for WeakRef race prevention.
MomentStreamHandler* moment_stream_handler_new (void);

void moment_stream_handler_delete (MomentStreamHandler *stream_handler);

void moment_stream_handler_set_audio_message (MomentStreamHandler        *stream_handler,
					      MomentAudioMessageCallback  cb,
					      void                       *user_data);

void moment_stream_handler_set_video_message (MomentStreamHandler        *stream_handler,
					      MomentVideoMessageCallback  cb,
					      void                       *user_data);

void moment_stream_handler_set_rtmp_command_message (MomentStreamHandler           *stream_handler,
						     MomentRtmpDataMessageCallback  cb,
						     void                          *user_data);

void moment_stream_handler_set_closed (MomentStreamHandler        *stream_handler,
				       MomentStreamClosedCallback  cb,
				       void                       *user_data);

void moment_stream_handler_set_num_watchers_changed (MomentStreamHandler              *stream_handler,
                                                     MomentNumWatchersChangedCallback  cb,
                                                     void                             *user_data);


// ___________________________ Video stream control ____________________________

typedef void MomentStream;

typedef void *MomentStreamKey;

typedef void *MomentStreamHandlerKey;

MomentStream* moment_create_stream (void);

MomentStream* moment_get_stream (char const      *name_buf,
				 size_t           name_len,
				 MomentStreamKey *ret_stream_key,
				 unsigned         create);

void moment_stream_ref (MomentStream *stream);

void moment_stream_unref (MomentStream *stream);

void moment_remove_stream (MomentStreamKey stream_key);

MomentStreamHandlerKey moment_stream_add_handler (MomentStream        *stream,
						  MomentStreamHandler *stream_handler);

void moment_stream_remove_handler (MomentStream           *stream,
				   MomentStreamHandlerKey  stream_handler_key);

void moment_stream_plus_one_watcher (MomentStream *stream);

void moment_stream_minus_one_watcher (MomentStream *stream);

void moment_stream_bind_to_stream (MomentStream *stream,
                                   MomentStream *bind_audio_stream,
                                   MomentStream *bind_video_stream,
                                   int           bind_audio,
                                   int           bind_video);


// _______________________________ Client events _______________________________

typedef struct MomentClientSession MomentClientSession;

void moment_client_session_ref (MomentClientSession *client_session);

void moment_client_session_unref (MomentClientSession *client_session);

void moment_client_session_disconnect (MomentClientSession *client_session);

typedef void (*MomentClientConnectedCallback) (MomentClientSession  *client_session,
					       char const           *app_name_buf,
					       size_t                app_name_len,
					       char const           *full_app_name_buf,
					       size_t                full_app_name_len,
					       void                **ret_client_user_data,
					       void                 *user_data);

typedef void (*MomentClientDisconnectedCallback) (void *client_user_data,
						  void *user_data);

typedef void (*MomentStartWatchingResultCallback) (MomentStream *ext_stream,
                                                   void         *cb_data);

typedef int (*MomentStartWatchingCallback) (char const    *stream_name_buf,
                                            size_t         stream_name_len,
                                            void          *client_user_data,
                                            void          *user_data,
                                            MomentStartWatchingResultCallback cb,
                                            void          *cb_data,
                                            MomentStream **ret_ext_stream);

typedef void (*MomentStartStreamingResultCallback) (MomentResult  res,
                                                    void         *cb_data);

typedef int (*MomentStartStreamingCallback) (char const          *stream_name_buf,
                                             size_t               stream_name_len,
                                             MomentStream        *stream,
                                             MomentRecordingMode  rec_mode,
                                             void                *client_user_data,
                                             void                *user_data,
                                             MomentStartStreamingResultCallback cb,
                                             void                *cb_data,
                                             MomentResult        *ret_res);

typedef void (*MomentRtmpCommandMessageCallback) (MomentMessage *msg,
						  void          *client_user_data,
						  void          *user_data);

typedef struct MomentClientHandler MomentClientHandler;

typedef void* MomentClientHandlerKey;

MomentClientHandler* moment_client_handler_new (void);

void moment_client_handler_delete (MomentClientHandler *client_handler);

void moment_client_handler_set_connected (MomentClientHandler           *client_handler,
					  MomentClientConnectedCallback  cb,
					  void                          *user_data);

void moment_client_handler_set_disconnected (MomentClientHandler              *client_handler,
					     MomentClientDisconnectedCallback  cb,
					     void                             *user_data);

void moment_client_handler_set_start_watching (MomentClientHandler         *client_handler,
					       MomentStartWatchingCallback  cb,
					       void                        *user_data);

void moment_client_handler_set_start_streaming (MomentClientHandler          *client_handler,
						MomentStartStreamingCallback  cb,
						void                         *user_data);

void moment_client_handler_set_rtmp_command_message (MomentClientHandler              *client_handler,
						     MomentRtmpCommandMessageCallback  cb,
						     void                             *user_data);

MomentClientHandlerKey moment_add_client_handler (MomentClientHandler *client_handler,
						  char const          *prefix_bux,
						  size_t               prefix_len);

void moment_remove_client_handler (MomentClientHandlerKey client_handler_key);

void moment_client_send_rtmp_command_message (MomentClientSession *client_session,
					      unsigned char const *msg_buf,
					      size_t               msg_len);

void moment_client_send_rtmp_command_message_passthrough (MomentClientSession *client_session,
							  MomentMessage       *msg);


// _____________________________ Config file access ____________________________

typedef void MomentConfigSectionEntry;

typedef void MomentConfigSection;

typedef void MomentConfigOption;

typedef void *MomentConfigIterator;

// @section  Section to iterate through. Pass NULL to iterate through the root section.
MomentConfigIterator moment_config_section_iter_begin (MomentConfigSection *section);

MomentConfigSectionEntry* moment_config_section_iter_next (MomentConfigSection  *section,
							   MomentConfigIterator *iter);

MomentConfigSection* moment_config_section_entry_is_section (MomentConfigSectionEntry *section_entry);

MomentConfigOption* moment_config_section_entry_is_option (MomentConfigSectionEntry *section_entry);

size_t moment_config_option_get_value (MomentConfigOption *option,
				       char               *buf,
				       size_t              len);

size_t moment_config_get_option (char   *opt_path,
				 size_t  opt_path_len,
				 char   *buf,
				 size_t  len,
				 int    *ret_is_set);


// __________________________________ Logging __________________________________

// The values are the same as for M::LogLevel.
typedef enum MomentLogLevel {
    MomentLogLevel_All      =  1000,
    MomentLogLevel_Debug    =  2000,
    MomentLogLevel_Info     =  3000,
    MomentLogLevel_Warning  =  4000,
    MomentLogLevel_Error    =  5000,
    MomentLogLevel_High     =  6000,
    MomentLogLevel_Failure  =  7000,
    MomentLogLevel_None     = 10000,
    MomentLogLevel_A        = MomentLogLevel_All,
    MomentLogLevel_D        = MomentLogLevel_Debug,
    MomentLogLevel_I        = MomentLogLevel_Info,
    MomentLogLevel_W        = MomentLogLevel_Warning,
    MomentLogLevel_E        = MomentLogLevel_Error,
    MomentLogLevel_H        = MomentLogLevel_High,
    MomentLogLevel_F        = MomentLogLevel_Failure,
    MomentLogLevel_N        = MomentLogLevel_None
} MomentLogLevel;

void moment_log  (MomentLogLevel log_level, char const *fmt, ...);
void moment_vlog (MomentLogLevel log_level, char const *fmt, va_list ap);

void moment_log_d (char const *fmt, ...);
void moment_log_i (char const *fmt, ...);
void moment_log_w (char const *fmt, ...);
void moment_log_e (char const *fmt, ...);
void moment_log_h (char const *fmt, ...);
void moment_log_f (char const *fmt, ...);

void moment_log_dump_stream_list ();


// ________________________________ AMF Decoder ________________________________

typedef struct MomentAmfDecoder MomentAmfDecoder;

MomentAmfDecoder* moment_amf_decoder_new_AMF0 (MomentMessage *msg);

void moment_amf_decoder_delete (MomentAmfDecoder *decoder);

void moment_amf_decoder_reset (MomentAmfDecoder *decoder,
			       MomentMessage    *msg);

int moment_amf_decode_number (MomentAmfDecoder *decoder,
			      double           *ret_number);

int moment_amf_decode_boolean (MomentAmfDecoder *decoder,
			       int              *ret_boolean);

int moment_amf_decode_string (MomentAmfDecoder *decoder,
			      char             *buf,
			      size_t            buf_len,
			      size_t           *ret_len,
			      size_t           *ret_full_len);

int moment_amf_decode_field_name (MomentAmfDecoder *decoder,
				  char             *buf,
				  size_t            buf_len,
				  size_t           *ret_len,
				  size_t           *ret_full_len);

int moment_amf_decoder_begin_object (MomentAmfDecoder *decoder);

int moment_amf_decoder_skip_value (MomentAmfDecoder *decoder);

int moment_amf_decoder_skip_object (MomentAmfDecoder *decoder);


// ________________________________ AMF Encoder ________________________________

typedef struct MomentAmfEncoder MomentAmfEncoder;

MomentAmfEncoder* moment_amf_encoder_new_AMF0 (void);

void moment_amf_encoder_delete (MomentAmfEncoder *encoder);

void moment_amf_encoder_reset (MomentAmfEncoder *encoder);

void moment_amf_encoder_add_number (MomentAmfEncoder *encoder,
				    double            number);

void moment_amf_encoder_add_boolean (MomentAmfEncoder *encoder,
				     int               boolean);

void moment_amf_encoder_add_string (MomentAmfEncoder *encoder,
				    char const       *str,
				    size_t            str_len);

void moment_amf_encoder_add_null_object (MomentAmfEncoder *encoder);

void moment_amf_encoder_begin_object (MomentAmfEncoder *encoder);

void moment_amf_encoder_end_object (MomentAmfEncoder *encoder);

void moment_amf_encoder_begin_ecma_array (MomentAmfEncoder *encoder,
					  unsigned long     num_entries);

void moment_amf_encoder_end_ecma_array (MomentAmfEncoder *encoder);

void moment_amf_encoder_add_field_name (MomentAmfEncoder *encoder,
					char const       *name,
					size_t            name_len);

void moment_amf_encoder_add_null (MomentAmfEncoder *encoder);

int moment_amf_encoder_encode (MomentAmfEncoder *encoder,
			       unsigned char    *buf,
			       size_t            buf_len,
			       size_t           *ret_len);


#ifdef __cplusplus
}
#endif


#endif /* MOMENT__API__H__ */

