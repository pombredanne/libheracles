GNULIB= ../gnulib/lib/libgnu.la
GNULIB_CFLAGS= -I $(top_builddir)/gnulib/lib -I $(top_srcdir)/gnulib/lib

AM_CFLAGS = @HERACLES_CFLAGS@ $(GNULIB_CFLAGS) 

AM_YFLAGS=-d -p spec_

EXTRA_DIST = try heracles_sym.version fa_sym.version

BUILT_SOURCES = datadir.h

DISTCLEANFILES = datadir.h

lib_LTLIBRARIES = libheracles.la
noinst_LTLIBRARIES = liblexer.la

include_HEADERS = heracles.h 

libheracles_la_SOURCES = heracles.h heracles.c pathx.c \
	internal.h internal.c \
	memory.h memory.c ref.h ref.c \
    syntax.c syntax.h parser.y builtin.c lens.c lens.h regexp.c regexp.h \
	transform.h transform.c ast.c get.c put.c list.h \
    info.c info.h errcode.c errcode.h jmt.h jmt.c \
	fa.c fa.h hash.c hash.h \
    tree.c tree.h

libheracles_la_LDFLAGS = $(HERACLES_VERSION_SCRIPT) \
    -version-info $(LIBHERACLES_VERSION_INFO)
libheracles_la_LIBADD = liblexer.la $(LIB_SELINUX) $(GNULIB)

liblexer_la_SOURCES = lexer.l
liblexer_la_CFLAGS = $(AM_CFLAGS) -Wno-error

FAILMALLOC_START ?= 1
FAILMALLOC_REP   ?= 20
FAILMALLOC_PROG ?= ./heratool

# Generate datadir.h. HERACLES_LENS_DIR in internal.h depends on
# the value of DATADIR
internal.h: datadir.h

FORCE-datadir.h: Makefile
	echo '#define DATADIR "$(datadir)"' > datadir.h1
	$(top_srcdir)/build/aux/move-if-change datadir.h1 datadir.h

datadir.h: FORCE-datadir.h
