COMMON_CFLAGS =				\
	-D_POSIX_C_SOURCE=199309L	\
	-D_XOPEN_SOURCE=600		\
	-ggdb -pedantic			\
	-Wno-long-long -Wall -Wextra	\
	`pkg-config --cflags mycpp-1.0 pargen-1.0`

CFLAGS = -I.. -std=c99 $(COMMON_CFLAGS)
CXXFLAGS = -std=c++98 -ansi $(COMMON_CFLAGS) -fno-default-inline

LDFLAGS = `pkg-config --libs mycpp-1.0 pargen-1.0`

.PHONY: all clean

GENFILES =		\
	test_pargen.h	\
	test_pargen.cpp

TARGETS = test__pargen_simple

all: $(TARGETS)

test__pargen_simple: $(GENFILES) test__pargen_simple.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_pargen.cpp test__pargen_simple.cpp $(LDFLAGS)

test_pargen.cpp: test_pargen.h
test_pargen.h: test.par
	pargen --module-name my_module --header-name test $^

clean:
	rm -f $(GENFILES) $(TARGETS)

