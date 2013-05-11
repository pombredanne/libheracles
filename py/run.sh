#! /bin/sh
export LD_LIBRARY_PATH=../src/.libs:$LD_LIBRARY_PATH
gdb --args python load.py
