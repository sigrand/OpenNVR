#!/bin/bash

set -e
set -x

OUTDIR=`pwd`

###### ffmpeg build begin

rm -rf ./ffmpeg
tar -xzf ./ffmpeg.tar.gz
cd ./ffmpeg

sudo apt-get update
sudo apt-get -y install sed autoconf automake build-essential git libass-dev libgpac-dev libtheora-dev libtool libvorbis-dev \
pkg-config texi2html zlib1g-dev \
libfaac-dev libjack-jackd2-dev \
libmp3lame-dev libopencore-amrnb-dev libopencore-amrwb-dev \
libssl1.0.0 libssl-dev libxvidcore-dev libxvidcore4


# 1. yasm

cd ./yasm-1.2.0
./configure --prefix="$OUTDIR/ffmpeg/ffmpeg_build"
make
make install
make clean
cd ..

# 2. x264

export PATH="$OUTDIR/ffmpeg/ffmpeg_build/bin:$PATH"

cd ./x264
./configure --prefix="$OUTDIR/ffmpeg/ffmpeg_build" --enable-static --enable-pic
make
make install
make clean
cd ..

# 3. RTMPDUMP

cd ./rtmpdump

sed -i "s@prefix=\/usr\/lib@prefix=$OUTDIR/ffmpeg/ffmpeg_build@g" Makefile
cd ./librtmp
sed -i "s@prefix=\/usr\/lib@prefix=$OUTDIR/ffmpeg/ffmpeg_build@g" Makefile
cd ..

make
make install
cd ..

# 4. FFMPEG

cd ./ffmpeg
export PKG_CONFIG_PATH="$OUTDIR/ffmpeg/ffmpeg_build/lib/pkgconfig"
./configure --prefix="$OUTDIR/ffmpeg/ffmpeg_build" \
  --extra-cflags="-I$OUTDIR/ffmpeg/ffmpeg_build/include -fPIC" \
  --extra-ldflags="-L$OUTDIR/ffmpeg/ffmpeg_build/lib -fPIC" \
  --bindir="$OUTDIR/ffmpeg/ffmpeg_build/bin" --extra-libs="-ldl" --enable-gpl \
  --enable-libx264 \
  --enable-librtmp \
  --enable-static \
  --enable-pic \
  --disable-shared 
make
make install
make clean
cd ..

cp ./ffmpeg/config.h ./ffmpeg_build/include/ffmpeg_config.h
cp ./ffmpeg/libavformat/internal.h ./ffmpeg_build/include/libavformat

###### ffmpeg build end
cd ..
echo "FFmpeg is built"