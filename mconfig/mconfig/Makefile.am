COMMON_CFLAGS =			\
	-ggdb			\
	-Wno-long-long -Wall    \
	$(THIS_CFLAGS)

if !PLATFORM_WIN32
    COMMON_CFLAGS += -pthread
#    # For backtraces
#    COMMON_CFLAGS += -rdynamic
endif

AM_CFLAGS   += -std=gnu99 $(COMMON_CFLAGS)
AM_CXXFLAGS += $(COMMON_CFLAGS)

INCLUDES = -I$(top_srcdir) -I$(top_builddir)

mconfig_private_headers =

mconfig_target_headers =	\
	mconfig.h		\
        util.h                  \
	config.h		\
	config_parser.h         \
        varlist.h               \
        varlist_parser.h

MCONFIG_GENFILES =		\
	mconfig_pargen.h        \
	mconfig_pargen.cpp      \
        varlist_pargen.h        \
        varlist_pargen.cpp
CLEANFILES = $(MCONFIG_GENFILES)

BUILT_SOURCES = mconfig_pargen.h        \
                varlist_pargen.h

mconfig_pargen.h mconfig_pargen.cpp: mconfig.par
	pargen --extmode --module-name MConfig --header-name mconfig $(srcdir)/mconfig.par

varlist_pargen.h varlist_pargen.cpp: varlist.par
	pargen --extmode --module-name VarList --namespace MConfig --header-name varlist $(srcdir)/varlist.par

mconfigincludedir = $(includedir)/mconfig-1.0/mconfig
mconfiginclude_HEADERS =	\
	$(mconfig_target_headers)

lib_LTLIBRARIES = libmconfig-1.0.la
libmconfig_1_0_la_SOURCES =		\
	mconfig.cpp			\
        util.cpp                        \
	config.cpp			\
        varlist.cpp                     \
	config_parser.cpp		\
        varlist_parser.cpp              \
	mconfig_pargen.cpp              \
        varlist_pargen.cpp

mconfig_extra_dist =                    \
	mconfig_pargen.h                \
        varlist_pargen.h

libmconfig_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0"
libmconfig_1_0_la_LIBADD = $(THIS_LIBS)

EXTRA_DIST = $(mconfig_private_headers) $(mconfig_extra_dist)

