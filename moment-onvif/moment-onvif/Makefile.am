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

moment_onvif_private_headers =

moment_onvif_target_headers =	\
	moment_onvif_module.h

moment_onvif_includedir = $(includedir)/moment-onvif-1.0/moment-onvif
moment_onvif_include_HEADERS = $(moment_onvif_target_headers)

moment_onvifdir = $(libdir)/moment-1.0
moment_onvif_LTLIBRARIES = libmoment-onvif-1.0.la
libmoment_onvif_1_0_la_SOURCES =	\
	moment_onvif_module.cpp	\
	mod_onvif.cpp

if PLATFORM_64
   moment_onvif_DATA= ../onvif-sdk/lib/x64/libWsDiscovery.so ../onvif-sdk/lib/x64/libOnvifSDK.so
else
   moment_onvif_DATA= ../onvif-sdk/lib/x86/libWsDiscovery.so ../onvif-sdk/lib/x86/libOnvifSDK.so
endif


moment_onvif_extra_dist =

libmoment_onvif_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0" -Wl,-rpath,.
libmoment_onvif_1_0_la_LIBADD = $(THIS_LIBS)
if PLATFORM_WIN32
libmoment_onvif_1_0_la_LIBADD += -lws2_32
endif

EXTRA_DIST = $(moment_onvif_private_headers) $(moment_onvif_extra_dist)

