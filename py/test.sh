#! /bin/sh
export LD_LIBRARY_PATH=../src/.libs:$LD_LIBRARY_PATH

nosetests ./test/test.py --pdb-failure
