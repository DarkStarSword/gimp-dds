
GIMPTOOL=gimptool-2.1

CC=gcc
CFLAGS=-O3 -Wall `$(GIMPTOOL) --cflags` -DGETTEXT_PACKAGE
LD=gcc
LDFLAGS=

TARGET=dds

SRCS=dds.c ddsread.c ddswrite.c dxt.c
OBJS=$(SRCS:.c=.o)

LIBS=`$(GIMPTOOL) --libs` -L/usr/X11R6/lib -lGL -lGLU -lglut -lXi -lXext \
-lXmu

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o $(TARGET)
		 
clean:
	rm -f *.o $(TARGET)
	
install: all
	$(GIMPTOOL) --install-bin $(TARGET)
		
.c.o:
	$(CC) -c $(CFLAGS) $<
	  
dds.o: dds.c ddsplugin.h dds.h dxt.h
ddsread.o: ddsread.c ddsplugin.h dds.h dxt.h
ddswrite.o: ddswrite.c ddsplugin.h dds.h dxt.h
dxt.o: dxt.c dds.h
