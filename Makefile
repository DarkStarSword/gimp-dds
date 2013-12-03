
GIMPTOOL=/opt/gimp-2.9/bin/gimptool-2.0

CC=gcc
CFLAGS+=-pipe -O2 -g -Wall -fopenmp $(shell PKG_CONFIG_PATH=/opt/gimp-2.9/lib/pkgconfig pkg-config --cflags gtk+-2.0 gimp-2.0)
LDFLAGS=-fopenmp

OS=$(shell uname -s)
ifeq (,$(findstring Windows,$(OS)))
EXT=
else
EXT=.exe
endif

TARGET=dds$(EXT)

SRCS=color.c dds.c ddsread.c ddswrite.c dxt.c mipmap.c misc.c
OBJS=$(SRCS:.c=.o)

LIBS=$(shell PKG_CONFIG_PATH=/opt/gimp-2.9/lib/pkgconfig pkg-config --libs gtk+-2.0 gimp-2.0 gimpui-2.0) -lm

ifdef VERBOSE
Q=
else
Q=@
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(Q)echo "[LD]\t$@"
	$(Q)$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $(TARGET)
		 
clean:
	rm -f *.o $(TARGET)

install: all
	$(GIMPTOOL) --install-bin $(TARGET)

.c.o:
	$(Q)echo "[CC]\t$<"
	$(Q)$(CC) -c $(CFLAGS) -o $@ $<

color.o: color.c color.h imath.h
dds.o: dds.c ddsplugin.h dds.h misc.h
ddsread.o: ddsread.c ddsplugin.h dds.h dxt.h endian.h
ddswrite.o: ddswrite.c ddsplugin.h dds.h dxt.h endian.h imath.h mipmap.h color.h
dxt.o: dxt.c dxt.h dxt_tables.h dds.h endian.h mipmap.h imath.h vec.h
mipmap.o: mipmap.c mipmap.h dds.h imath.h
misc.o: misc.c misc.h

ifdef WIN32
-include Makefile.mingw32
else ifdef WIN64
-include Makefile.mingw64
endif
