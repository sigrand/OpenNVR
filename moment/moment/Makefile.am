COMMON_CFLAGS =					\
	-ggdb					\
	-Wno-long-long -Wall                    \ 
        -include "local_config.h"               \
	-DLIBMOMENT_PREFIX="\"$(libdir)\""	\
	$(THIS_CFLAGS)

if PLATFORM_WIN32
    COMMON_CFLAGS += -DPLATFORM_WIN32
endif
if PLATFORM_CYGWIN
    COMMON_CFLAGS += -DPLATFORM_CYGWIN -pthread
endif

if PLATFORM_DEFAULT
    COMMON_CFLAGS += -DPLATFORM_DEFAULT -pthread
#    # For backtraces
#    COMMON_CFLAGS += -rdynamic
endif

COMMON_LDFLAGS =

AM_CFLAGS   += -std=gnu99 $(COMMON_CFLAGS)
AM_CXXFLAGS += $(COMMON_CFLAGS)

INCLUDES = -I$(top_srcdir) -I$(top_builddir)

moment_private_headers =
moment_target_headers  =
moment_target_sources  =
include Makefile.inc.am

moment_target_headers +=        \
        libmoment_config.h      \
	libmoment.h		\
        libmoment_gst.h         \
				\
	moment_types.h		\
        channel.h               \
        channel_options.h       \
        channel_set.h           \
        channel_manager.h       \
        media_source.h          \
        slave_media_source.h    \
        media_source_provider.h \
        playback.h              \
        playlist.h              \
        recorder.h              \
                                \
        flv_util.h              \
	amf_encoder.h		\
	amf_decoder.h		\
	rtmp_connection.h	\
        rtmp_client.h           \
	rtmp_server.h		\
	rtmp_video_service.h	\
	rtmp_service.h		\
	rtmpt_service.h		\
				\
	video_stream.h		\
        test_stream_generator.h \
				\
	av_recorder.h		\
	av_muxer.h		\
	flv_muxer.h		\
        mp4_muxer.h             \
				\
	storage.h		\
	local_storage.h		\
				\
        push_protocol.h         \
        rtmp_push_protocol.h    \
        push_agent.h            \
                                \
        fetch_protocol.h        \
        rtmp_fetch_protocol.h   \
        fetch_agent.h           \
                                \
	moment_server.h		\
                                \
        util_moment.h           \
        util_config.h           \
				\
        inc.h                   \
	api.h			\
				\
	module_init.h           \
                                \
        transcoder.h

#	flv_file_source.h

DISTCLEANFILES = libmoment_config.h.in

libmomentincludedir = $(includedir)/moment-1.0/moment
libmomentinclude_HEADERS = $(moment_target_headers)

lib_LTLIBRARIES = libmoment-1.0.la
libmoment_1_0_la_SOURCES =	\
	video_stream.cpp	\
                                \
	libmoment.cpp		\
                                \
        channel.cpp             \
        channel_set.cpp         \
        channel_manager.cpp     \
        slave_media_source.cpp  \
        playback.cpp            \
        playlist.cpp            \
        recorder.cpp            \
                                \
        flv_util.cpp            \
	amf_encoder.cpp		\
	amf_decoder.cpp		\
	rtmp_connection.cpp	\
        rtmp_client.cpp         \
	rtmp_server.cpp		\
	rtmp_service.cpp	\
	rtmpt_service.cpp	\
				\
        test_stream_generator.cpp \
				\
	av_recorder.cpp		\
	flv_muxer.cpp		\
        mp4_muxer.cpp           \
				\
	local_storage.cpp	\
				\
        rtmp_push_protocol.cpp  \
        push_agent.cpp          \
                                \
        rtmp_fetch_protocol.cpp \
        fetch_agent.cpp         \
                                \
	moment_server.cpp	\
                                \
        util_moment.cpp         \
        util_config.cpp         \
				\
	api.cpp			\
	api_amf.cpp		\
				\
	hmac/sha2.c		\
	hmac/hmac_sha2.c        \
                                \
        $(moment_target_sources)

#	flv_file_source.cpp

moment_extra_dist = libmoment_config.h.in

if MOMENT_GSTREAMER
libmoment_1_0_la_SOURCES += libmoment_gst.cpp
else
moment_extra_dist += libmoment_gst.cpp
endif

libmoment_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0" $(COMMON_LDFLAGS)
libmoment_1_0_la_LIBADD = $(THIS_LIBS)
if MOMENT_NETTLE
libmoment_1_0_la_LIBADD += -lgmp
endif
if PLATFORM_WIN32
libmoment_1_0_la_LIBADD += -lws2_32
endif

momentdir = $(libdir)/moment-1.0
moment_LTLIBRARIES = libmoment-file-1.0.la libmoment-rtmp-1.0.la libmoment-test-1.0.la # libmoment-archive-1.0.la

libmoment_file_1_0_la_DEPENDENCIES = libmoment-1.0.la
libmoment_file_1_0_la_SOURCES = \
	mod_file.cpp
libmoment_file_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0" $(COMMON_LDFLAGS)
libmoment_file_1_0_la_LIBADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)

libmoment_rtmp_1_0_la_DEPENDENCIES = libmoment-1.0.la
libmoment_rtmp_1_0_la_SOURCES =	\
	mod_rtmp.cpp
libmoment_rtmp_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0" $(COMMON_LDFLAGS)
libmoment_rtmp_1_0_la_LIBADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)
if PLATFORM_WIN32
libmoment_rtmp_1_0_la_LIBADD += -lws2_32
endif

#libmoment_archive_1_0_la_DEPENDENCIES = libmoment-1.0.la
#libmoment_archive_1_0_la_SOURCES = \
#	mod_archive.cpp
#libmoment_archive_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0" $(COMMON_LDFLAGS)
#libmoment_archive_1_0_la_LIBADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)

libmoment_test_1_0_la_DEPENDENCIES = libmoment-1.0.la
libmoment_test_1_0_la_SOURCES =	\
	mod_test.cpp
libmoment_test_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0" $(COMMON_LDFLAGS)
libmoment_test_1_0_la_LIBADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)

bin_PROGRAMS = moment rtmptool

moment_DEPENDENCIES = libmoment-1.0.la
moment_SOURCES =	\
	moment.cpp
moment_LDADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)
if MOMENT_GPERFTOOLS
moment_LDADD += -lprofiler
endif
moment_LDFLAGS = $(COMMON_LDFLAGS)

rtmptool_DEPENDENCIES = libmoment-1.0.la
rtmptool_SOURCES =	\
	rtmptool.cpp
rtmptool_LDADD = $(top_builddir)/moment/libmoment-1.0.la $(THIS_LIBS)
rtmptool_LDFLAGS = $(COMMON_LDFLAGS)

EXTRA_DIST = $(moment_private_headers) $(moment_extra_dist)

myplayerdir = $(datadir)/moment
myplayer_DATA = moment.example.conf

