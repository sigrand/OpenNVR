#!/bin/bash

set -e
set -x

OUTDIR=`pwd`

###### Poco build begin

rm -rf ./poco
mkdir ./poco
tar -xzf ./poco-1.4.6p2.tar.gz -C ./poco
cd ./poco/poco-1.4.6p2

./configure --prefix="$OUTDIR/poco/poco_build" --static --cflags="-fPIC"
make
make install
make clean
cd ../..

echo "Poco is built"