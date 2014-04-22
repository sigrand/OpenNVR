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
        ffmpeg_stream.cpp               \
        segment_muxer.c                 \
        naming_scheme.cpp               \
        naming_scheme.h                 \
        nvr_cleaner.h                   \
        nvr_cleaner.cpp                 \
        nvr_file_iterator.h             \
        nvr_file_iterator.cpp           \
        channel_checker.h               \
        channel_checker.cpp             \
        media_reader.h                  \
        media_reader.cpp                \
        media_viewer.h                  \
        media_viewer.cpp                \
        types.h                         \
        types.cpp                       \
        time_checker.cpp                \
        time_checker.h                  \
        stat_measurer.cpp               \
        stat_measurer.h                 \
        video_part_maker.cpp            \
        video_part_maker.h              \
        memory_dispatcher.cpp           \
        memory_dispatcher.h             \
        rec_path_config.cpp             \
        rec_path_config.h               \
        ffmpeg_common.h                 \
        iostat.h                        \
        inc.h

moment_ffmpeg_win_sources =                                         \
        ../../../win_moment_ffmpeg/windows_helper_funcs.h           \
        ../../../win_moment_ffmpeg/windows_helper_funcs.cpp

moment_ffmpeg_extra_dist =

libmoment_ffmpeg_1_0_la_LIBADD = $(THIS_LIBS)
if PLATFORM_WIN32
libmoment_ffmpeg_1_0_la_LIBADD += -lws2_32
libmoment_ffmpeg_1_0_la_LDFLAGS = -no-undefined
libmoment_ffmpeg_1_0_la_SOURCES += $(moment_ffmpeg_win_sources)
endif

EXTRA_DIST = $(moment_ffmpeg_private_headers) $(moment_ffmpeg_extra_dist)

