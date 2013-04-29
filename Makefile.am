SUBDIRS=gnulib/lib src 

ACLOCAL_AMFLAGS = -I gnulib/m4

lensdir=$(datadir)/heracles/lenses/dist
lenstestdir=$(datadir)/heracles/lenses/dist/tests

dist_lens_DATA=$(wildcard lenses/*.hera)
dist_lenstest_DATA=$(wildcard lenses/tests/*.hera)


EXTRA_DIST=build/aux/move-if-change Makefile.am HACKING

distclean-local:
	-find $(top_builddir)/build/* -maxdepth 0 -not -name aux | xargs rm -rf

ChangeLog:
	if test -d $(top_srcdir)/.git; then                             \
	  $(top_srcdir)/build/aux/gitlog-to-changelog > $@;             \
	fi

dist: ChangeLog

.PHONY: ChangeLog
