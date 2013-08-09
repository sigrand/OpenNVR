COMMON_CFLAGS =				\
	-Wno-long-long -Wall -Wextra	\
	-ggdb -O3 -rdynamic			\
	`pkg-config --cflags libmary-1.0`

CFLAGS = -std=c99 $(COMMON_CFLAGS)
CXXFLAGS = -std=c++0x $(COMMON_CFLAGS) -fno-default-inline

LDFLAGS = `pkg-config --libs libmary-1.0`

LDFLAGS += -L/opt/moment/lib \
           -Wl,--dynamic-linker=/opt/moment/lib/ld-linux-x86-64.so.2 \
           -Wl,-L/opt/moment/lib \
           -Wl,-rpath=/opt/moment/lib

.PHONY: all clean

TARGETS = test__exc

all: $(TARGETS)

test__exc: test__exc.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS)

