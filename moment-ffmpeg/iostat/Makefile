# Compiler to use
CC = gcc

# Other commands
AR = ar

# Compiler flags
CFLAGS = -fPIC -g -O2 -Wall -Wstrict-prototypes -pipe -O2
DFLAGS = 
LFLAGS = -s

%.o: %.c
	$(CC) -o $@ -c $(CFLAGS) $(DFLAGS) $<

% : %.o
	$(CC) -o $@ $(CFLAGS) $^ $(LFLAGS)

all: libiostat.so

common.o: common.c version.h common.h ioconf.h sysconfig.h
	$(CC) -o $@ -c $(CFLAGS) $(DFLAGS) $<

ioconf.o: ioconf.c ioconf.h common.h sysconfig.h
	$(CC) -o $@ -c $(CFLAGS) $(DFLAGS) $<

rd_stats_light.o: rd_stats.c common.h rd_stats.h ioconf.h sysconfig.h
	$(CC) -o $@ -c $(CFLAGS) $(DFLAGS) $<

count_light.o: count.c common.h rd_stats.h
	$(CC) -o $@ -c $(CFLAGS) $(DFLAGS) $<

format.o: format.c sadf.h
	$(CC) -o $@ -c $(CFLAGS) $(DFLAGS) $<

# Explicit rules needed to prevent possible file corruption
# when using parallel execution.
libsyscom.a: common.o ioconf.o
	$(AR) rvs $@ $?

librdstats_light.a: rd_stats_light.o count_light.o
	$(AR) rvs $@ $?

iostat.o: iostat.c iostat.h version.h common.h ioconf.h sysconfig.h rd_stats.h count.h
	$(CC) -o $@ -c $(CFLAGS) $(DFLAGS) $<

libiostat.so: iostat.o librdstats_light.a libsyscom.a
	$(CC) -shared iostat.o -o $@ -Wl,-whole-archive -L. librdstats_light.a libsyscom.a -Wl,-no-whole-archive

clean:
	rm -f *.o *.so *.a
