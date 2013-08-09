
#!/bin/bash
# execute this from the current project dir;
# first arg is  absolute path to the output folder
# second arg is autogen call flag: [nogen/gen]
# thirs arg is rebuild flag: [noclean/clean]
# fourth arg is ffmpeg support flag (if it exists then moment
#    will be built without gstreamer and with ffmpeg): [ffmpeg]

set -e
set -x
OUTDIR=$1

if [ "$2" = "gen" ]
then
	AUTOGEN=1
fi

if [ "$3" = "clean" ]
then
	CLEAN=1
fi

if [ "$4" = "ffmpeg" ]
then
	FFMPEG=1
fi

function autogen()
{
	if [ -n "$1" ]
	then
	sh ./autogen.sh 
	fi 
}

function makeclean()
{
	if [ -n "$1" ]
	then
		make clean
	fi
}

# check required tools 
test -e /usr/bin/java
test -e /opt/flex/bin/mxmlc
# check prerequisites
CHECK_GLIB="`pkg-config --exists glib-2.0 --print-errors`"
CHECK_XML="`pkg-config --exists libxml-2.0 --print-errors`"
CHECK_JSON="`pkg-config --exists jsoncpp --print-errors`"
CHECK_GST="`pkg-config --exists gstreamer-0.10 --print-errors`"

if [ -n "${CHECK_GLIB}" || "${CHECK_XML}" || "${CHECK_JSON}" || "${CHECK_GST}" ]; then
    echo "Not all prerequisites were installed; ${CHECK_GLIB} ${CHECK_XML} ${CHECK_JSON} ${CHECK_GST}"
	exit 1
fi

# Generate projects
cd libmary
autogen "$AUTOGEN"
cd ../pargen
autogen "$AUTOGEN"
cd ../scruffy
autogen "$AUTOGEN"
cd ../mconfig
autogen "$AUTOGEN"
cd ../moment  
autogen "$AUTOGEN"
if [ -z "$FFMPEG" ]
then
cd ../moment-gst
autogen "$AUTOGEN"
else
cd ../moment-ffmpeg
autogen "$AUTOGEN"
fi
cd ../moment-nvr
autogen "$AUTOGEN"
cd ../moment-onvif
autogen "$AUTOGEN"

cd ../ctemplate-2.2
sh ./configure --prefix="${OUTDIR}/ctemplate"
make
make install



I_GLIB=`pkg-config --cflags glib-2.0`
L_GLIB=`pkg-config --libs glib-2.0`
I_XML=`pkg-config --cflags libxml-2.0`
L_XML=`pkg-config --libs libxml-2.0`
I_JSON=`pkg-config --cflags jsoncpp`
L_JSON=`pkg-config --libs jsoncpp`
I_GST=`pkg-config --cflags gstreamer-0.10` 
L_GST=`pkg-config --libs gstreamer-0.10`

export THIS_CFLAGS=""
export THIS_LIBS=""
export CFLAGS="-O2"
export CXXFLAGS="${CFLAGS}"

cd ../libmary
makeclean "$CLEAN"

sh ./configure --prefix="${OUTDIR}"
make
make install

cd ../pargen
makeclean "$CLEAN"

export THIS_CFLAGS="-I${OUTDIR}/include/libmary-1.0 ${I_GLIB}"
export THIS_LIBS="-L${OUTDIR}/lib -lmary-1.0 ${L_GLIB}"
sh ./configure --prefix="${OUTDIR}"
make
make install

cd ../scruffy
makeclean "$CLEAN"

export PATH="${OUTDIR}/bin:$PATH"

export THIS_CFLAGS="${I_GLIB} -I${OUTDIR}/include/pargen-1.0 \
	-I${OUTDIR}/include/libmary-1.0"
export THIS_LIBS="${L_GLIB} \
	-L${OUTDIR}/lib -lpargen-1.0 \
	-L${OUTDIR}/lib -lmary-1.0"
sh ./configure --prefix="${OUTDIR}"
make 
make install

cd ../mconfig
makeclean "$CLEAN"

export THIS_CFLAGS="${I_GLIB} \
	-I${OUTDIR}/include/pargen-1.0 \
	-I${OUTDIR}/include/libmary-1.0 \
	-I${OUTDIR}/include/scruffy-1.0/"
export THIS_LIBS="${L_GLIB} \
	-L${OUTDIR}/lib -lpargen-1.0 \
	-L${OUTDIR}/lib -lmary-1.0 \
	-L${OUTDIR}/lib -lscruffy-1.0"
sh ./configure --prefix="${OUTDIR}"
make
make install

cd ../moment
makeclean "$CLEAN"


export THIS_CFLAGS="-I${OUTDIR}/ctemplate/include \
	${I_GLIB} ${I_XML} ${I_JSON} \
	-I${OUTDIR}/include/pargen-1.0 \
	-I${OUTDIR}/include/libmary-1.0 \
	-I${OUTDIR}/include/scruffy-1.0 \
	-I${OUTDIR}/include/mconfig-1.0"
export THIS_LIBS="${L_GLIB} ${L_XML} ${L_JSON} \
	-L${OUTDIR}/lib -lpargen-1.0 \
	-L${OUTDIR}/lib -lmary-1.0 \
	-L${OUTDIR}/lib -lscruffy-1.0 \
	-L${OUTDIR}/lib -lmconfig-1.0 \
	-L${OUTDIR}/ctemplate/lib -lctemplate"

if [ -n "$FFMPEG" ]
then
./configure --prefix="${OUTDIR}" --disable-gstreamer
else
export THIS_CFLAGS+=" ${I_GST}"
export THIS_LIBS+=" ${L_GST}"
./configure --prefix="${OUTDIR}"
fi

make
make install

cd ./web
make
make install
cd ..

export THIS_CFLAGS="${I_GLIB} ${I_XML} ${I_JSON} \
	-I${OUTDIR}/include/moment-1.0 \
	-I${OUTDIR}/include/libmary-1.0 \
	-I${OUTDIR}/include/mconfig-1.0 \
	-I${OUTDIR}/include/pargen-1.0"
export THIS_LIBS="${L_GLIB} ${L_XML} ${L_JSON} \
	-L${OUTDIR}/lib -lmoment-1.0 \
	-L${OUTDIR}/lib -lmary-1.0 \
	-L${OUTDIR}/lib -lmconfig-1.0 \
	-L${OUTDIR}/lib -lpargen-1.0"

if [ -z "$FFMPEG" ]
then
cd ../moment-gst
export THIS_CFLAGS+=" ${I_GST}"
export THIS_LIBS+=" ${L_GST}"
else
cd ../moment-ffmpeg
export THIS_CFLAGS+=" -I../ffmpeg-sdk/include"
export THIS_LIBS+=" -L../ffmpeg-sdk/lib -lavformat -lavcodec -lz -lm -lswscale"
fi
makeclean "$CLEAN"

./configure --prefix="${OUTDIR}"
make
make install

cd ../moment-nvr
makeclean "$CLEAN"

export THIS_CFLAGS="${I_GLIB} ${I_XML} \
	-I${OUTDIR}/include/moment-1.0 \
	-I${OUTDIR}/include/libmary-1.0 \
	-I${OUTDIR}/include/mconfig-1.0 \
	-I${OUTDIR}/include/pargen-1.0"
export THIS_LIBS="${L_GLIB} ${L_XML} \
	-L${OUTDIR}/lib -lmoment-1.0 \
	-L${OUTDIR}/lib -lmary-1.0 \
	-L${OUTDIR}/lib -lmconfig-1.0 \
	-L${OUTDIR}/lib -lpargen-1.0"

if [ -z "$FFMPEG" ]
then
export THIS_CFLAGS+=" ${I_GST} -I${OUTDIR}/include/moment-gst-1.0"
export THIS_LIBS+=" ${L_GST} -L${OUTDIR}/lib/moment-1.0 -lmoment-gst-1.0"
fi

./configure --prefix="${OUTDIR}"
make
make install

cd ../moment-onvif
makeclean "$CLEAN"

export THIS_CFLAGS="${I_GLIB} ${I_XML} \
	-I${OUTDIR}/include/moment-1.0 \
	-I${OUTDIR}/include/libmary-1.0 \
	-I${OUTDIR}/include/mconfig-1.0 \
	-I${OUTDIR}/include/pargen-1.0 \
	-I../onvif-sdk/include"
export THIS_LIBS="${L_GLIB} ${L_XML} \
	-L${OUTDIR}/lib -lmoment-1.0 \
	-L${OUTDIR}/lib -lmary-1.0 \
	-L${OUTDIR}/lib -lmconfig-1.0 \
	-L${OUTDIR}/lib -lpargen-1.0"
	

if [ $(uname -m) == "x86_64" ]
then
	export THIS_LIBS+=" -L../onvif-sdk/lib/x64 -lWsDiscovery -lOnvifSDK"
else
	export THIS_LIBS+=" -L../onvif-sdk/lib/x86 -lWsDiscovery -lOnvifSDK"
fi

if [ -z "$FFMPEG" ]
then
export THIS_CFLAGS+=" ${I_GST} -I${OUTDIR}/include/moment-gst-1.0"
export THIS_LIBS+=" ${L_GST} -L${OUTDIR}/lib/moment-1.0 -lmoment-gst-1.0"
fi

./configure --prefix="${OUTDIR}"
make
make install
