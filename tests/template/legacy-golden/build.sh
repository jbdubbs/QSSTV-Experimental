#!/bin/bash
# Builds gen.cpp against the object files of an existing build-cmake tree (the legacy editor classes are linked from
# there). Usage: build.sh [build-dir]   -> writes ./gen
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B=${1:-$HERE/../../../build-cmake}
cd "$B"
FLAGS=$(grep '^CXX_DEFINES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
INCS=$(grep '^CXX_INCLUDES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
CXXF=$(grep '^CXX_FLAGS' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
eval /usr/bin/c++ $CXXF $FLAGS $INCS -fPIC -c "$HERE/gen.cpp" -o "$HERE/gen.o"
LINK=$(sed "s# CMakeFiles/qsstv.dir/main.cpp.o # $HERE/gen.o #; s#-o qsstv-experimental#-o $HERE/gen#; s#-Wl,--dependency-file=CMakeFiles/qsstv.dir/link.d##" CMakeFiles/qsstv.dir/link.txt)
eval $LINK
rm -f "$HERE/gen.o"
