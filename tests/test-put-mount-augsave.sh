#! /bin/bash

# Test that we don't follow bind mounts when writing to .herasave.
# This requires that EXDEV or EBUSY is returned from rename(2) to activate the
# code path, so set up a bind mount on Linux.

if [ $UID -ne 0 -o "$(uname -s)" != "Linux" ]; then
    echo "Test can only be run as root on Linux to create bind mounts"
    exit 77
fi

actual() {
    (heratool --nostdinc -I $LENSES -r $ROOT --backup | grep ^/heracles) <<EOF
    set /heracles/save/copy_if_rename_fails 1
    set /files/etc/hosts/1/alias myhost
    save
    print /heracles//error
EOF
}

expected() {
    cat <<EOF
/heracles/files/etc/hosts/error = "clone_unlink_dst_herasave"
/heracles/files/etc/hosts/error/message = "Device or resource busy"
EOF
}

ROOT=$abs_top_builddir/build/test-put-mount-herasave
LENSES=$abs_top_srcdir/lenses

HOSTS=$ROOT/etc/hosts
HOSTS_HERASAVE=${HOSTS}.herasave

ATTACK_FILE=$ROOT/other/attack

rm -rf $ROOT
mkdir -p $(dirname $HOSTS)
mkdir -p $(dirname $ATTACK_FILE)

echo 127.0.0.1 localhost > $HOSTS
touch $ATTACK_FILE $HOSTS_HERASAVE

mount --bind $ATTACK_FILE $HOSTS_HERASAVE
Exit() {
    umount $HOSTS_HERASAVE
    exit $1
}

ACTUAL=$(actual)
EXPECTED=$(expected)
if [ "$ACTUAL" != "$EXPECTED" ]; then
    echo "No error when trying to unlink herasave (a bind mount):"
    echo "$ACTUAL"
    exit 1
fi

if [ -s $ATTACK_FILE ]; then
    echo "/other/attack now contains data, should be blank"
    Exit 1
fi

Exit 0
