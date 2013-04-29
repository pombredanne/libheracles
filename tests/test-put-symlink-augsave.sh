#! /bin/bash

# Test that we don't follow .herasave symlinks

ROOT=$abs_top_builddir/build/test-put-symlink-herasave
LENSES=$abs_top_srcdir/lenses

HOSTS=$ROOT/etc/hosts
HOSTS_HERASAVE=${HOSTS}.herasave

ATTACK_FILE=$ROOT/other/attack

rm -rf $ROOT
mkdir -p $(dirname $HOSTS)
mkdir -p $(dirname $ATTACK_FILE)

cat <<EOF > $HOSTS
127.0.0.1 localhost
EOF
HOSTS_SUM=$(sum $HOSTS)

touch $ATTACK_FILE
(cd $(dirname $HOSTS) && ln -s ../other/attack $(basename $HOSTS).herasave)

# Now ask for the original to be saved in .herasave
heratool --nostdinc -I $LENSES -r $ROOT --backup > /dev/null <<EOF
set /files/etc/hosts/1/alias myhost
save
EOF

if [ ! -f $HOSTS ] ; then
    echo "/etc/hosts is no longer a regular file"
    exit 1
fi
if [ ! -f $HOSTS_HERANEW ] ; then
    echo "/etc/hosts.herasave is still a symlink, should be unlinked"
    exit 1
fi

if [ ! "x${HOSTS_SUM}" = "x$(sum $HOSTS_HERASAVE)" ]; then
    echo "/etc/hosts.herasave has changed from the original /etc/hosts"
    exit 1
fi
if ! grep myhost $HOSTS >/dev/null; then
    echo "/etc/hosts does not contain the modification"
    exit 1
fi

if [ -s $ATTACK_FILE ]; then
    echo "/other/attack now contains data, should be blank"
    exit 1
fi
