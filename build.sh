#!/bin/bash

cd out && cmake ../

if test $# -gt 0 && test $1 = "clean"
then
    echo "make clean"
    make clean
else
    echo "make"
    make
fi
