#! /bin/bash

# Check that reading the files in tests/root/ with heratool does not lead to
# any errors

TOPDIR=$(cd $(dirname $0)/.. && pwd)
[ -n "$abs_top_srcdir" ] || abs_top_srcdir=$TOPDIR

export HERACLES_LENS_LIB=$abs_top_srcdir/lenses
export HERACLES_ROOT=$abs_top_srcdir/tests/root

errors=$(heratool --nostdinc match '/heracles//error/descendant-or-self::*')

if [ "x$errors" != "x  (no matches)" ] ; then
    printf "get /heracles//error reported errors:\n%s\n" "$errors"
    exit 1
fi
