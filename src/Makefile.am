EXTRA_DIST = Makefile.old

AM_CFLAGS = -Wall -Werror -Wextra $(GLIB_CFLAGS)

bin_PROGRAMS = armaservermonitor
check_PROGRAMS = test
pkglib_LTLIBRARIES = @ASMDLL_NAME@.la

# TODO: run the test program during "make check"
#TESTS = ...

armaservermonitor_SOURCES = asm.h asm.c asmlog.h asmlog.c client.h client.c \
 gettickcount.h gettickcount.c server.h server.c \
 settings.h settings.c util.h util.c
armaservermonitor_CFLAGS = $(AM_CFLAGS)
armaservermonitor_LDFLAGS = -lrt -lm $(GLIB_LIBS)

@ASMDLL_NAME@_la_SOURCES = = asmdll.h asmdll.c asmlog.h asmlog.c \
 gettickcount.h gettickcount.c settings.h settings.c util.h util.c
@ASMDLL_NAME@_la_LDFLAGS = -avoid-version -module -lrt -lm $(GLIB_LIBS)

test_SOURCES = test.c
test_LDFLAGS = -ldl

