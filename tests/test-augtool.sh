#! /bin/bash

TOP_DIR=$(cd $(dirname $0)/.. && pwd)
TOP_BUILDDIR="$abs_top_builddir"
[ -z "$TOP_BUILDDIR" ] && TOP_BUILDDIR="$TOP_DIR"
TOP_SRCDIR="$abs_top_srcdir"
[ -z "$TOP_SRCDIR" ] && TOP_SRCDIR="$TOP_DIR"

TEST_DIR="$TOP_SRCDIR/tests"

export PATH="$TOP_BUILDDIR/src:${PATH}"

export HERACLES_ROOT="$TOP_BUILDDIR/build/test-heratool"
export HERACLES_LENS_LIB="$TOP_SRCDIR/lenses"

GSED=sed
type gsed >/dev/null 2>&1 && GSED=gsed

fail() {
    [ -z "$failed" ] && echo FAIL
    failed=yes
    echo "$@"
    result=1
}

# Without args, run all tests
if [ $# -eq 0 ] ; then
    args="$TEST_DIR/test-heratool/*.sh"
else
    args="$@"
fi

result=0

for tst in $args; do
    unset failed

    printf "%-40s ... " $(basename $tst .sh)

    # Read in test variables. The variables we understand are
    # echo              - echo heratool commands if set to some value
    # commands          - the commands to send to heratool
    # lens              - the lens to use
    # file              - the file that should be changed
    # diff              - the expected diff
    # refresh           - print diff in a form suitable for cut and paste
    #                     into the test file if set to some value

    unset echo commands lens file diff refresh
    . $tst

    # Setup test root from root/
    [ -d "$HERACLES_ROOT" ] && rm -rf "$HERACLES_ROOT"
    dest_dir="$HERACLES_ROOT"$(dirname $file)
    mkdir -p $dest_dir
    cp -p "$TEST_DIR"/root/$file $dest_dir

    [ -n "$echo" ] && echo="-e"

    commands="set /heracles/load/Test/lens $lens
set /heracles/load/Test/incl $file
load
$commands
save
quit"
    echo "$commands" | heratool $echo --nostdinc --noautoload -n || fail "heratool failed"

    abs_file="$HERACLES_ROOT$file"
    if [ ! -f "${abs_file}.heranew" ]; then
        fail "Expected file $file.heranew"
    else
        safe_heracles_root=$(printf "%s\n" "$HERACLES_ROOT" | sed 's/[][\.*^$(){}?+|/]/\\&/g')
        act=$(diff -u "$abs_file" "${abs_file}.heranew" \
            | $GSED -r -e "s/^ $//;s!^(---|\+\+\+) ${safe_heracles_root}($file(\.heranew)?)(.*)\$!\1 \2!;s/\\t/\\\\t/g")

        if [ "$act" != "$diff" ] ; then
            fail "$act"
        fi
    fi
    other_files=$(find "$HERACLES_ROOT" -name \*.heranew | grep -v "$abs_file.heranew")
    [ -n "$other_files" ] && fail "Unexpected file(s) $other_files"
    [ -z "$failed" ] && echo OK
done

exit $result
