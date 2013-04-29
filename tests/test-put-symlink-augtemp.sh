#! /bin/bash

# Test that we don't follow .heranew symlinks (regression test)

ROOT=$abs_top_builddir/build/test-put-symlink-heratemp
LENSES=$abs_top_srcdir/lenses

HOSTS=$ROOT/etc/hosts
HOSTS_HERANEW=${HOSTS}.heranew

ATTACK_FILE=$ROOT/other/attack

rm -rf $ROOT
mkdir -p $(dirname $HOSTS)
mkdir -p $(dirname $ATTACK_FILE)

cat <<EOF > $HOSTS
127.0.0.1 localhost
EOF
touch $ATTACK_FILE

(cd $(dirname $HOSTS) && ln -s ../other/attack $(basename $HOSTS).heranew)

# Test the normal save code path which would use a temp heranew file
heratool --nostdinc -I $LENSES -r $ROOT > /dev/null <<EOF
set /files/etc/hosts/1/alias myhost1
save
EOF

if [ -h $HOSTS ] ; then
    echo "/etc/hosts is now a symlink, pointing to" $(readlink $HOSTS)
    exit 1
fi
if ! grep myhost1 $HOSTS >/dev/null; then
    echo "/etc/hosts does not contain the modification"
    exit 1
fi

if [ ! -h $HOSTS_HERANEW ] ; then
    echo "/etc/hosts.heranew is not a symbolic link"
    exit 1
fi
LINK=$(readlink $HOSTS_HERANEW)
if [ "x$LINK" != "x../other/attack" ] ; then
    echo "/etc/hosts.heranew no longer links to ../other/attack"
    exit 1
fi

if [ -s $ATTACK_FILE ]; then
    echo "/other/attack now contains data, should be blank"
    exit 1
fi
