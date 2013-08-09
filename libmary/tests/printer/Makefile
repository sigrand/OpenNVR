COMMON_CFLAGS =				\
	-D_POSIX_C_SOURCE=199309L	\
	-D_XOPEN_SOURCE=600		\
	-ggdb -pedantic			\
	-Wno-long-long -Wall -Wextra	\
	-rdynamic			\
	`pkg-config --cflags libmary-1.0`

#COMMON_CFLAGS += #-O2

CFLAGS = -std=c99 $(COMMON_CFLAGS)
CXXFLAGS = -std=c++0x $(COMMON_CFLAGS) -fno-default-inline

LDFLAGS = `pkg-config --libs libmary-1.0`

.PHONY: all clean

TARGETS = test__printer

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

