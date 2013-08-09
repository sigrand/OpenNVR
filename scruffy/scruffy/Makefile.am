COMMON_CFLAGS =			\
        -ggdb                   \
	-Wno-long-long -Wall    \
	$(THIS_CFLAGS)

if !PLATFORM_WIN32
#    COMMON_CFLAGS += -rdynamic
endif

AM_CFLAGS   += -std=gnu99 $(COMMON_CFLAGS)
AM_CXXFLAGS += $(COMMON_CFLAGS)

INCLUDES = -I$(top_srcdir) -I$(top_builddir)

PARGEN_GENFILES =		\
	cpp_pargen.h		\
	cpp_pargen.cpp		\
	cpp_cond_pargen.h	\
	cpp_cond_pargen.cpp
CLEANFILES = $(PARGEN_GENFILES)

BUILT_SOURCES = cpp_pargen.h	\
		cpp_cond_pargen.h

cpp_pargen.h: cpp.par
	pargen --module-name scruffy --header-name cpp $(srcdir)/cpp.par

cpp_cond_pargen.h: cpp_cond.par
	pargen --module-name scruffy --header-name cpp_cond $(srcdir)/cpp_cond.par

scruffy_private_headers =		\
	cpp.h				\
	cpp_util.h			\
	dump_context.h			\
	cpp_parser.h			\
	cpp_dispatcher.h		\
	test_dispatcher.h		\
					\
	util.h                          \
                                        \
        unichar_stream.h

scruffy_target_headers =		\
	parsing_exception.h		\
					\
        unichar_stream.h                \
        utf8_unichar_stream.h           \
                                        \
	pp_item_stream.h		\
	unichar_pp_item_stream.h	\
	list_pp_item_stream.h		\
	phase3_item_stream.h		\
	list_token_stream.h		\
					\
	pp_item_stream_token_stream.h	\
					\
	macro_definition.h		\
	preprocessor_types.h		\
	preprocessor.h			\
					\
	checkpoint_tracker.h

#bin_PROGRAMS = scruffy
#scruffy_DEPENDENCIES = libscruffy-1.0.la
#scruffy_SOURCES =
#	cpp.cpp
#	cpp_util.cpp
#	dump_context.cpp
#	cpp_parser.cpp
#
#	main.cpp
#
#	cpp_pargen.cpp

##	cpp_type_conversions.cpp

#scruffy_LDADD = $(top_builddir)/scruffy/libscruffy-1.0.la	\
#		$(THIS_LIBS)
## Adding -rpath breaks testing changes in the library without doing make install.
##scruffy_LDFLAGS = -Wl,-rpath,'$(libdir)'

lib_LTLIBRARIES = libscruffy-1.0.la
libscruffy_1_0_la_SOURCES =		\
	pp_item_stream.cpp              \
	unichar_pp_item_stream.cpp      \
	list_pp_item_stream.cpp         \
	phase3_item_stream.cpp          \
	pp_item_stream_token_stream.cpp \
	util.cpp                        \
	checkpoint_tracker.cpp          \
	cpp_cond_pargen.cpp             \
        file_byte_stream.cpp            \
        utf8_unichar_stream.cpp         \
	preprocessor_util.cpp           \
	preprocessor.cpp

libscruffy_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0"
libscruffy_1_0_la_LIBADD = $(THIS_LIBS)

scruffy_includedir = $(includedir)/scruffy-1.0/scruffy
scruffy_include_HEADERS = $(scruffy_target_headers)

EXTRA_DIST = $(scruffy_private_headers)

