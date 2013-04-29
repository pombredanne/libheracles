#! /bin/bash

# Test that we don't follow symlinks when writing to .heranew

ROOT=$abs_top_builddir/build/test-put-symlink-heranew
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

HOSTS_SUM=$(sum $HOSTS)

heratool --nostdinc -I $LENSES -r $ROOT --new > /dev/null <<EOF
set /files/etc/hosts/1/alias myhost
save
EOF

if [ ! -f $HOSTS ] ; then
    echo "/etc/hosts is no longer a regular file"
    exit 1
fi
if [ ! "x${HOSTS_SUM}" = "x$(sum $HOSTS)" ]; then
    echo "/etc/hosts has changed"
    exit 1
fi

if [ ! -f $HOSTS_HERANEW ] ; then
    echo "/etc/hosts.heranew is still a symlink, should be unlinked"
    exit 1
fi
if ! grep myhost $HOSTS_HERANEW >/dev/null; then
    echo "/etc/hosts does not contain the modification"
    exit 1
fi

if [ -s $ATTACK_FILE ]; then
    echo "/other/attack now contains data, should be blank"
    exit 1
fi
