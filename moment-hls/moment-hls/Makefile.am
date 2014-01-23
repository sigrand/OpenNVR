COMMON_CFLAGS =			\
	-ggdb			\
	-Wno-long-long -Wall    \
        -Wno-unused-parameter   \
	$(THIS_CFLAGS)

if PLATFORM_WIN32
    COMMON_CFLAGS += -DPLATFORM_WIN32
else
    COMMON_CFLAGS += -pthread # -rdynamic
endif

if MOMENT_HLS_DEMO
    COMMON_CFLAGS += -DMOMENT_HLS_DEMO
endif

AM_CFLAGS   += -std=gnu99 $(COMMON_CFLAGS)
AM_CXXFLAGS += $(COMMON_CFLAGS)

INCLUDES = -I$(top_srcdir)

moment_hls_private_headers =
moment_hls_target_headers =
moment_hls_target_sources =
include Makefile.inc.am

moment_hlsdir = $(libdir)/moment-1.0
moment_hls_LTLIBRARIES = libmoment-hls-1.0.la
libmoment_hls_1_0_la_SOURCES =	\
        mod_hls.cpp             \
        tsmux/tsmux.c           \
        tsmux/tsmuxstream.c     \
        $(moment_hls_target_sources)

moment_hls_extra_dist =

libmoment_hls_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0"
libmoment_hls_1_0_la_LIBADD = $(THIS_LIBS)
if PLATFORM_WIN32
libmoment_hls_1_0_la_LIBADD += -lws2_32
endif

EXTRA_DIST = $(moment_hls_private_headers) $(moment_hls_extra_dist)

