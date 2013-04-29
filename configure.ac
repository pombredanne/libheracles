AC_INIT(rheracles, 1.0.0)
AC_CONFIG_SRCDIR([src/heracles.c])
AC_CONFIG_AUX_DIR([build/aux])
AM_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([-Wno-portability 1.11 color-tests parallel-tests])
AM_SILENT_RULES([yes]) # make --enable-silent-rules the default.


dnl Version info in libtool's notation
AC_SUBST([LIBHERACLES_VERSION_INFO], [16:0:16])
AC_SUBST([LIBFA_VERSION_INFO], [5:0:4])

AC_GNU_SOURCE

AC_PROG_CC
gl_EARLY
AC_SYS_LARGEFILE
AC_PROG_LIBTOOL
AC_PROG_YACC
AC_PROG_LEX

## Compiler flags to be used everywhere
HERACLES_CFLAGS=-std=gnu99
AC_SUBST(HERACLES_CFLAGS)

AC_CHECK_FUNCS([open_memstream uselocale])

AC_MSG_CHECKING([how to pass version script to the linker ($LD)])
VERSION_SCRIPT_FLAGS=none
if $LD --help 2>&1 | grep "version-script" >/dev/null 2>/dev/null; then
    VERSION_SCRIPT_FLAGS=-Wl,--version-script=
elif $LD --help 2>&1 | grep "M mapfile" >/dev/null 2>/dev/null; then
    VERSION_SCRIPT_FLAGS="-Wl,-M -Wl,"
fi
AC_MSG_RESULT([$VERSION_SCRIPT_FLAGS])
AC_SUBST(VERSION_SCRIPT_FLAGS)
AM_CONDITIONAL([USE_VERSION_SCRIPT], [test "$VERSION_SCRIPT_FLAGS" != none])
gl_INIT
AC_CHECK_FUNCS([strerror_r fsync])

AC_OUTPUT(Makefile \
          gnulib/lib/Makefile \
          gnulib/tests/Makefile \
          src/Makefile \
          examples/Makefile )
