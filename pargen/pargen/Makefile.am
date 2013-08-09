COMMON_CFLAGS =			\
        -ggdb                   \
	-Wno-long-long -Wall	\
	$(THIS_CFLAGS)

AM_CFLAGS   += -std=gnu99 $(COMMON_CFLAGS)
AM_CXXFLAGS += $(COMMON_CFLAGS)

INCLUDES = -I$(top_srcdir)

pargen_private_headers =	\
	util.h			\
	declarations.h		\
	pargen_task_parser.h	\
	compile.h		\
	header_compiler.h	\
	source_compiler.h

pargen_target_headers =		\
        file_position.h         \
        token_stream.h          \
        file_token_stream.h     \
        memory_token_stream.h   \
	parser_element.h	\
	acceptor.h		\
	grammar.h		\
	parsing_exception.h	\
	lookup_data.h		\
	parser.h

bin_PROGRAMS = pargen
pargen_DEPENDENCIES = libpargen-1.0.la
pargen_SOURCES =                \
	util.cpp                \
        declarations.cpp        \
	pargen_task_parser.cpp  \
        header_compiler.cpp     \
        source_compiler.cpp     \
	main.cpp

pargen_LDADD = $(top_builddir)/pargen/libpargen-1.0.la	\
	       $(THIS_LIBS)

lib_LTLIBRARIES = libpargen-1.0.la
libpargen_1_0_la_SOURCES =      \
        file_token_stream.cpp   \
        memory_token_stream.cpp \
	grammar.cpp             \
	parser.cpp
libpargen_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0"
libpargen_1_0_la_LIBADD = $(THIS_LIBS)

pargen_includedir = $(includedir)/pargen-1.0/pargen
pargen_include_HEADERS = $(pargen_target_headers)

EXTRA_DIST = $(pargen_private_headers)

