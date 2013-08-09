COMMON_CFLAGS =			\
	-ggdb			\
	-Wno-long-long -Wall    \
	$(THIS_CFLAGS)

if PLATFORM_WIN32
    COMMON_CFLAGS += -DPLATFORM_WIN32
else
    COMMON_CFLAGS += -pthread # -rdynamic
endif

AM_CFLAGS   += -std=gnu99 $(COMMON_CFLAGS)
AM_CXXFLAGS += $(COMMON_CFLAGS)

INCLUDES = -I$(top_srcdir)

moment_ffmpeg_private_headers =

moment_ffmpeg_target_headers =	\
        moment_ffmpeg_module.h  \
        ffmpeg_stream.h


moment_ffmpeg_includedir = $(includedir)/moment-ffmpeg-1.0/moment-ffmpeg
moment_ffmpeg_include_HEADERS = $(moment_ffmpeg_target_headers)

moment_ffmpegdir = $(libdir)/moment-1.0
moment_ffmpeg_LTLIBRARIES = libmoment-ffmpeg-1.0.la
libmoment_ffmpeg_1_0_la_SOURCES =	\
	moment_ffmpeg_module.cpp	\
        moment_ffmpeg_module.h          \
        ffmpeg_stream.h                 \
        mod_ffmpeg.cpp                  \
        ffmpeg_stream.cpp

moment_ffmpeg_DATA= ../ffmpeg-sdk/lib/libavcodec.so ../ffmpeg-sdk/lib/libavdevice.so \
                   ../ffmpeg-sdk/lib/libavfilter.so ../ffmpeg-sdk/lib/libavformat.so \
                   ../ffmpeg-sdk/lib/libavutil.so ../ffmpeg-sdk/lib/libpostproc.so \
                   ../ffmpeg-sdk/lib/librtmp.so ../ffmpeg-sdk/lib/libswscale.so \
                   ../ffmpeg-sdk/lib/libswresample.so ../ffmpeg-sdk/lib/libx264.so

moment_ffmpeg_extra_dist =

libmoment_ffmpeg_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0" -Wl,-rpath,.
libmoment_ffmpeg_1_0_la_LIBADD = $(THIS_LIBS)
if PLATFORM_WIN32
libmoment_ffmpeg_1_0_la_LIBADD += -lws2_32
endif

EXTRA_DIST = $(moment_ffmpeg_private_headers) $(moment_ffmpeg_extra_dist)

