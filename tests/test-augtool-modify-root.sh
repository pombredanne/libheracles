#! /bin/bash

# Make sure changing the value of root works

exp="/ = root"

act=$(heratool --noautoload 2>&1 <<EOF
set / root
get /
quit
EOF
)
result=$?

if [ $result -ne 0 ]; then
    echo "heratool failed"
    exit 1
fi

if [ "$act" != "$exp" ]; then
    echo "heratool produced unexpected output:"
    echo "Expected:"
    echo "$exp"
    echo "Actual:"
    echo "$act"
    exit 1
fi
