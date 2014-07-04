#!/bin/bash
# BuildScript name
bs="buildLinuxMoment" 

COPY_WEB="no"

SCRIPT=`realpath $0` || { echo 'You have to install realpath.'; exit 1; }

# BuildScript working directory
SC_DIR=`dirname $SCRIPT`
source ${SC_DIR}/helperFuncs.sh

# destination directory
DST=`realpath $1 2> /dev/null`

if [ "${DST}" == "" ]; then
    checkErr "Erroneous value for build destination [${DST}]"
fi

# check prerequisites
CHECK_GLIB="`pkg-config --cflags glib-2.0`"
if [ -z "${CHECK_GLIB}" ]; then
    checkErr "glib-2.0 isn't installed. Please install glib-2.0"
fi

CHECK_XML="`pkg-config --cflags libxml-2.0`"
if [ -z "${CHECK_XML}" ]; then
    checkErr "libxml-2.0 isn't installed. Please install libxml-2.0"
fi

CHECK_JSON="`pkg-config --cflags jsoncpp`"
if [ -z "${CHECK_JSON}" ]; then
    checkErr "jsoncpp isn't installed. Please install jsoncpp"    
fi

yasmOutput=`yasm --version 2> /dev/null`
if [[ "${yasmOutput}" != *yasm\ 1.2* ]]; then
    checkErr "yasm 1.2 or above required"
fi


# install some prerequisites:
# sudo apt-get update
sudo apt-get -y install sed autoconf automake build-essential git libass-dev \
    libgpac-dev libtheora-dev libtool libvorbis-dev pkg-config texi2html \
    zlib1g-dev libfaac-dev libjack-jackd2-dev libmp3lame-dev \
    libopencore-amrnb-dev libopencore-amrwb-dev \
    libssl1.0.0 libssl-dev libxvidcore-dev libxvidcore4


# build some prerequisites:
# ctemplate
ctemplatePath="${DST}/ctemplate"
    ProjectBuildStart "ctemplate" "${SC_DIR}/external/ctemplate-2.2.tar.gz" \
        $ctemplatePath "$ctemplatePath/lib/libctemplate.so"
    if [ $CurrentProjectStatus_ -eq 0 ]; then
	patch -p0 < "${SC_DIR}/external/ctemplate.r129.patch"
        ./configure --prefix="${ctemplatePath}" \
                --enable-static=no --enable-shared=yes || checkErr "ctemplate config failed"
    fi
    ProjectBuildFinish
    ctemplateIncludeDir="-I${ctemplatePath}/include"
    ctemplateLibs="-L${ctemplatePath}/lib -lctemplate"


# POCO:
pocoPath="${DST}/poco"
    ProjectBuildStart "poco" "${SC_DIR}/external/poco-1.4.6p2.tar.gz" \
        $pocoPath "$pocoPath/lib/libPocoFoundation.a"
    if [ $CurrentProjectStatus_ -eq 0 ]; then        
        ./configure --prefix="${pocoPath}" --static \
            --cflags="-fPIC" --no-samples --no-tests || checkErr "poco config failed"
    fi
    ProjectBuildFinish
    pocoIncludeDir="-I${pocoPath}/include"
    pocoLibs="-L${pocoPath}/lib -Wl,--whole-archive \
      -Wl,${pocoPath}/lib/libPocoFoundation.a \
      -Wl,${pocoPath}/lib/libPocoNet.a \
      -Wl,${pocoPath}/lib/libPocoUtil.a \
      -Wl,${pocoPath}/lib/libPocoXML.a \
      -Wl,--no-whole-archive -Wl,-Bsymbolic -lz -lm -lpthread -ldl -lrt"

# x264:
x264Path="${DST}/x264"
    ProjectBuildStart "x264" \
    "${SC_DIR}/external/x264-snapshot-20131008-2245-stable.tar.bz2" \
    $x264Path "$x264Path/lib/libx264.a"
    if [ $CurrentProjectStatus_ -eq 0 ]; then
            ./configure --enable-static --disable-shared --enable-pic \
                --prefix="${x264Path}" || checkErr "x264 configure failed";
    fi
    ProjectBuildFinish
    x264IncludeDir="-I${x264Path}/include"
    x264Libs="-L${x264Path}/lib" #-Wl,--whole-archive,${x264Path}/lib/libx264.a,--no-whole-archive"


# rtmpdump:
rtmpdumpPath="${DST}/rtmpdump"
    ProjectBuildStart "rtmpdump" \
    "${SC_DIR}/external/rtmpdump-2.3.tgz" \
    $rtmpdumpPath "$rtmpdumpPath/lib/librtmp.a"
    if [ $CurrentProjectStatus_ -eq 0 ]; then
        sed -i "s@prefix=\/usr\/local@prefix=$rtmpdumpPath/@g" ./Makefile
        sed -i "s@prefix=\/usr\/local@prefix=$rtmpdumpPath/@g" ./librtmp/Makefile            
    fi
    ProjectBuildFinish
    rtmpdumpIncludeDir="-I${rtmpdumpPath}/include"
    rtmpdumpLibs="-L${rtmpdumpPath}/lib" # -Wl,--whole-archive,${rtmpdumpPath}/lib/librtmp.a,--no-whole-archive"


# ffmpeg itself
ffmpegPath="${DST}/ffmpeg"
    ProjectBuildStart "ffmpeg" \
    "${SC_DIR}/external/ffmpeg-2.0.1.tar.bz2" \
    $ffmpegPath \
    "$ffmpegPath/lib/libavcodec.a"
    if [ $CurrentProjectStatus_ -eq 0 ]; then
        export PKG_CONFIG_PATH="${rtmpdumpPath}/lib/pkgconfig:${x264Path}/lib/pkgconfig"
        ./configure --prefix="${ffmpegPath}" \
            --extra-cflags="${x264IncludeDir} ${rtmpdumpIncludeDir} -fPIC" \
            --extra-ldflags="${x264Libs} ${rtmpdumpLibs}" \
            --bindir="$ffmpegPath/bin" --extra-libs="-ldl" --enable-gpl \
            --enable-libx264 \
            --enable-librtmp \
            --enable-static \
            --enable-pic \
            --disable-shared  || checkErr "ffmpeg configure failed";
        # some headers have to be installed manually
        mkdir -p "${ffmpegPath}/include/libavformat"
        cp config.h "${ffmpegPath}/include/ffmpeg_config.h" || checkErr "ffmpeg headers delivery failed"
        cp libavformat/internal.h "${ffmpegPath}/include/libavformat/internal.h" || checkErr "ffmpeg headers delivery failed"
    fi
    ProjectBuildFinish
    ffmpegIncludeDir="-I${ffmpegPath}/include"  
    ffmpegLibs="-Wl,--whole-archive \
        -Wl,${ffmpegPath}/lib/libavutil.a \
        -Wl,${ffmpegPath}/lib/libavformat.a \
        -Wl,${ffmpegPath}/lib/libavcodec.a \
        -Wl,${ffmpegPath}/lib/libswscale.a \
        -Wl,${x264Path}/lib/libx264.a \
        -Wl,${rtmpdumpPath}/lib/librtmp.a \
        -Wl,--no-whole-archive"



# set external Includes/Libraries
glibIncludeDir=`pkg-config --cflags glib-2.0`
glibLibs=`pkg-config --libs glib-2.0`
xmlIncludeDir=`pkg-config --cflags libxml-2.0`
xmlLibs=`pkg-config --libs libxml-2.0`
jsonIncludeDir=`pkg-config --cflags jsoncpp`
jsonLibs=`pkg-config --libs jsoncpp`


# libmary:
libmaryPath="${DST}/libmary"
    ProjectBuildStart "libmary" "${SC_DIR}/libmary" $libmaryPath "${libmaryPath}/lib/libmary-1.0.so"
        if [ $CurrentProjectStatus_ -eq 0 ]; then            
            ./configure --prefix="${libmaryPath}" || checkErr "libmary config failed"
        fi
    ProjectBuildFinish
    libmaryInclideDir="-I${libmaryPath}/include/libmary-1.0"
    libmaryLibs="-L${libmaryPath}/lib -lmary-1.0"


# pargen:
pargenPath="${DST}/pargen"  
    ProjectBuildStart "pargen" "${SC_DIR}/pargen" $pargenPath "$pargenPath/lib/libpargen-1.0.so"
        if [ $CurrentProjectStatus_ -eq 0 ]; then
            THIS_CFLAGS="${glibIncludeDir} ${libmaryInclideDir}" \
            THIS_LIBS="${glibLibs} ${libmaryLibs}" \
            ./configure --prefix="${pargenPath}" || checkErr "pargen config failed"
        fi
    ProjectBuildFinish  
    pargenInclideDir="-I${pargenPath}/include/pargen-1.0"
    pargenLibs="-L${pargenPath}/lib -lpargen-1.0"
    export PATH="${pargenPath}/bin:$PATH"


# scruffy:
scruffyPath="${DST}/scruffy"    
    ProjectBuildStart "scruffy" "${SC_DIR}/scruffy" $scruffyPath "$scruffyPath/lib/libscruffy-1.0.so"
        if [ $CurrentProjectStatus_ -eq 0 ]; then
            THIS_CFLAGS="${libmaryInclideDir} ${glibIncludeDir} ${pargenInclideDir}" \
            THIS_LIBS="${libmaryLibs} ${glibLibs} ${pargenLibs}" \
            ./configure --prefix="${scruffyPath}" || checkErr "scruffy config failed"
        fi
    ProjectBuildFinish
    scruffyIncludeDir="-I${scruffyPath}/include/scruffy-1.0"
    scruffyLibs="-L${scruffyPath}/lib -lscruffy-1.0"


# mconfig:
mconfigPath="${DST}/mconfig"
    ProjectBuildStart "mconfig" "${SC_DIR}/mconfig" $mconfigPath "${mconfigPath}/lib/libmconfig-1.0.so"
        if [ $CurrentProjectStatus_ -eq 0 ]; then
            THIS_CFLAGS="${libmaryInclideDir} ${glibIncludeDir} ${pargenInclideDir} ${scruffyIncludeDir}" \
            THIS_LIBS="${libmaryLibs} ${glibLibs} ${pargenLibs} ${scruffyLibs}" \
            ./configure --prefix="${mconfigPath}" || checkErr "mconfig config failed"
        fi
    ProjectBuildFinish
    mconfigIncludeDir="-I${mconfigPath}/include/mconfig-1.0"
    mconfigLibs="-L${mconfigPath}/lib -lmconfig-1.0"


# moment:
momentPath="${DST}/moment"
    ProjectBuildStart "moment" "${SC_DIR}/moment" $momentPath "${momentPath}/lib/libmoment-1.0.so"
        if [ $CurrentProjectStatus_ -eq 0 ]; then
            THIS_CFLAGS="${libmaryInclideDir} ${glibIncludeDir} ${pargenInclideDir} ${scruffyIncludeDir} ${mconfigIncludeDir} ${ctemplateIncludeDir} ${xmlIncludeDir} ${jsonIncludeDir} ${pocoIncludeDir}" \
            THIS_LIBS="${libmaryLibs} ${glibLibs} ${pargenLibs} ${scruffyLibs} ${mconfigLibs} ${ctemplateLibs} ${xmlLibs} ${jsonLibs} ${pocoLibs}" \
            ./configure --prefix="${momentPath}" \
                --disable-gstreamer || checkErr "moment config failed"
        fi  
    ProjectBuildFinish

    if [ "${COPY_WEB}" == "yes" ]; then
        # moment has extra folder to perform install:
        cd "${SC_DIR}/moment/web"
        make || checkErr "moment web make failed"
        make install || checkErr "moment web make install failed"
    fi

    # deliver some extra files
    if [ ! -f "${momentPath}/bin/run_moment.sh" ]; then
        cp "${SC_DIR}/run_moment.sh" "${momentPath}/bin/run_moment.sh"
        sed -i "s@../moment.conf@$momentPath/moment.conf@g" "${momentPath}/bin/run_moment.sh" || checkErr "run_moment edit failed"
        sed -i "s@./moment @$momentPath/bin/moment @g" "${momentPath}/bin/run_moment.sh" || checkErr "run_moment edit failed"
        sed -i "s@./log.txt@$momentPath/bin/log.txt@g" "${momentPath}/bin/run_moment.sh" || checkErr "run_moment edit failed"
        sed -i "s@./out.txt@$momentPath/bin/out.txt@g" "${momentPath}/bin/run_moment.sh" || checkErr "run_moment edit failed"
        sed -i "s@./err.txt@$momentPath/bin/err.txt@g" "${momentPath}/bin/run_moment.sh" || checkErr "run_moment edit failed"
    fi

    mkdir "${momentPath}/records"
    mkdir "${momentPath}/conf.d"
    if [ ! -f "${momentPath}/moment.conf" ]; then
        cp "${SC_DIR}/moment.conf" "${momentPath}"
        cp "${SC_DIR}/recpath.conf" "${momentPath}"
        cp "${SC_DIR}/watchdog.py" "${momentPath}/bin"
        sed -i "s@/opt/nvr@$momentPath@g" "${momentPath}/moment.conf" || checkErr "Config edit failed"
        sed -i "s@/opt/nvr@$momentPath@g" "${momentPath}/recpath.conf" || checkErr "RecpathConfig edit failed"
        sed -i "s@/opt/nvr/moment@$momentPath@g" "${momentPath}/bin/watchdog.py" || checkErr "RecpathConfig edit failed"
    fi

    # export
    momentIncludeDir="-I${momentPath}/include/moment-1.0"
    momentLibs="-L${momentPath}/lib -lmoment-1.0"

# hdd statistic module
cd "${SC_DIR}/moment-ffmpeg/iostat"
make
cp "${SC_DIR}/moment-ffmpeg/iostat/libiostat.so" "${momentPath}/lib/libiostat.so"
momentLibs+=" -liostat"

# moment-ffmpeg:
momentffmpegPath="${DST}/moment"
    ProjectBuildStart "moment-ffmpeg" "${SC_DIR}/moment-ffmpeg" $momentffmpegPath \
     "${momentffmpegPath}/lib/moment-1.0/libmoment-ffmpeg-1.0.so"
        if [ $CurrentProjectStatus_ -eq 0 ]; then
            THIS_CFLAGS="${libmaryInclideDir} ${glibIncludeDir} ${pargenInclideDir} ${scruffyIncludeDir} ${mconfigIncludeDir} ${ctemplateIncludeDir} ${xmlIncludeDir} ${jsonIncludeDir} ${ffmpegIncludeDir} ${momentIncludeDir} ${pocoIncludeDir}" \
            THIS_LIBS="${ctemplateLibs} ${xmlLibs} ${jsonLibs} ${glibLibs} ${libmaryLibs} ${pargenLibs} ${scruffyLibs} ${mconfigLibs} ${momentLibs} ${ffmpegLibs} -Wl,-Bsymbolic -lz -lm" \
            ./configure --prefix="${momentffmpegPath}" \
                --enable-shared --disable-static  || checkErr "moment-ffmpeg config failed"            
        fi  
    ProjectBuildFinish


# moment-hls:
momenthlsPath="${DST}/moment"
    ProjectBuildStart "moment-hls" "${SC_DIR}/moment-hls" $momenthlsPath \
     "${momenthlsPath}/lib/moment-1.0/libmoment-hls-1.0.so"
        if [ $CurrentProjectStatus_ -eq 0 ]; then
            THIS_CFLAGS="${libmaryInclideDir} ${glibIncludeDir} ${pargenInclideDir} ${scruffyIncludeDir} ${mconfigIncludeDir} ${ctemplateIncludeDir} ${xmlIncludeDir} ${jsonIncludeDir} ${momentIncludeDir} ${pocoIncludeDir}" \
            THIS_LIBS="${ctemplateLibs} ${xmlLibs} ${jsonLibs} ${glibLibs} ${libmaryLibs} ${pargenLibs} ${scruffyLibs} ${mconfigLibs} ${momentLibs}" \
            ./configure --prefix="${momenthlsPath}" \
                --enable-shared --disable-static  || checkErr "moment-ffmpeg config failed"            
        fi  
    ProjectBuildFinish


succMessage "project has been built and installed to ${momentPath}"
