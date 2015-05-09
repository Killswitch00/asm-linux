EXTRA_DIST = Makefile.old

AM_CFLAGS = $(GLIB_CFLAGS)

bin_PROGRAMS = asm
check_PROGRAMS = test
pkglib_LTLIBRARIES = asmdll.la

# TODO: run the test program during "make check"
#TESTS = ...

asm_SOURCES = asm.h asm.c client.h client.c gettickcount.h gettickcount.c \
 server.h server.c settings.h settings.c util.h util.c
asm_CFLAGS = $(AM_CFLAGS)
asm_LDFLAGS = -lrt -lm $(GLIB_LIBS)

asmdll_la_SOURCES = asmdll.h asmdll.c gettickcount.h gettickcount.c \
 settings.h settings.c util.h util.c
asmdll_la_LDFLAGS = -avoid-version -module -lrt -lm $(GLIB_LIBS)

test_SOURCES = test.c
test_LDFLAGS = -ldl

