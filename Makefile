
GIMPTOOL=gimptool-2.0

CC=gcc
CFLAGS=-pipe -g -O2 -Wall $(shell pkg-config --cflags gtk+-2.0 gimp-2.0)

OS=$(shell uname -s)
ifeq ($(OS), Linux)
EXT=
else
EXT=.exe
endif

TARGET=dds$(EXT)

SRCS=color.c dds.c ddsread.c ddswrite.c dxt.c mipmap.c misc.c
OBJS=$(SRCS:.c=.o)

LIBS=$(shell pkg-config --libs gtk+-2.0 gimp-2.0 gimpui-2.0) -lm

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $(TARGET)
		 
clean:
	rm -f *.o $(TARGET)

install: all
	$(GIMPTOOL) --install-bin $(TARGET)

.c.o:
	$(CC) -c $(CFLAGS) $<

color.o: color.c color.h imath.h
dds.o: dds.c ddsplugin.h dds.h misc.h
ddsread.o: ddsread.c ddsplugin.h dds.h dxt.h endian.h
ddswrite.o: ddswrite.c ddsplugin.h dds.h dxt.h endian.h imath.h mipmap.h
dxt.o: dxt.c dxt.h dxt_tables.h dds.h endian.h mipmap.h imath.h
mipmap.o: mipmap.c mipmap.h dds.h imath.h
misc.o: misc.c misc.h
