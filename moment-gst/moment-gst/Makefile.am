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

moment_gst_private_headers =

moment_gst_target_headers =	\
	moment_gst_module.h	\
	gst_stream.h

moment_gst_includedir = $(includedir)/moment-gst-1.0/moment-gst
moment_gst_include_HEADERS = $(moment_gst_target_headers)

moment_gstdir = $(libdir)/moment-1.0
moment_gst_LTLIBRARIES = libmoment-gst-1.0.la
libmoment_gst_1_0_la_SOURCES =	\
	moment_gst_module.cpp	\
	mod_gst.cpp		\
	gst_stream.cpp

moment_gst_extra_dist =

libmoment_gst_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0"
libmoment_gst_1_0_la_LIBADD = $(THIS_LIBS)
if PLATFORM_WIN32
libmoment_gst_1_0_la_LIBADD += -lws2_32
endif

EXTRA_DIST = $(moment_gst_private_headers) $(moment_gst_extra_dist)

