OBJS=settings_database.o ui.o applet_start.o
SOURCES=settings_database.c ui.c applet_start.c
BUILDDIR=../build
LIB=$(BUILDDIR)/libcpdevicelock.so
PKG_FLAGS=$(shell pkg-config glib-2.0 gconf-2.0 hildon-1 libhildondesktop-1 codelockui libdevlock1 --libs --cflags)
CCFLAGS=-shared -fPIC -Wall -Werror
CC=gcc

all:$(LIB)

$(LIB):$(OBJS)
	$(CC) $(CCFLAGS) $(PKG_FLAGS) $(OBJS) -o $(LIB)

.c.o:
	$(CC) $(CCFLAGS) $(PKG_FLAGS) -c $< -o $@

.PHONE: clean all

clean:
	rm -f $(OBJS)
