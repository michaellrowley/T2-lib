#!/usr/bin/env bash

# Small-scale cleanup for previously unfinished runs
rm -rf ./build-output/
rm -f *.o
rm -f *.a

set -e # Break on first error
set -u # Break if we use an undefined variable

# Clone relevant boost libraries
boostpath="./boost/"
if ! test -e ./boost/; then
    boostpath=$1
    # https://github.com/boostorg/asio # Asio
    # https://github.com/boostorg/endian/ # Endianness
    # https://github.com/boostorg/lexical_cast # Lexicality
fi

clang++ ./*/*.cpp -c -Wall -std=c++20 -I $boostpath -I . # Create object files (.o)
ar rcs ./T2.a ./*.o # Create the library file

rm ./*.o # Remove the object files
mkdir ./build-output/
mv ./*.a ./build-output/
cd ./build-output/ # Leave the user in the output directory

# Undo the above 'set' flags:
set +u
set +e