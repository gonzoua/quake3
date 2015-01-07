#!/bin/bash
# this script builds q3 for raspberry pi
# invoke with ./
# or ./build.sh clean to clean before build

# directory containing the ARM shared libraries (rootfs, lib/ of SD card)
# specifically libEGL.so and libGLESv2.so
ARM_ROOT=/src/FreeBSD/tftproot/rpi/
ARM_LIBS=${ARM_ROOT}/opt/vc/lib

# directory containing baseq3/ containing .pk3 files - baseq3 on CD
BASEQ3_DIR="/baseq3/"

# directory to find khronos linux make files (with include/ containing
# headers! Make needs them.)
INCLUDES="-I${ARM_ROOT}/opt/vc/include -I${ARM_ROOT}/opt/vc/include/interface/vcos/pthreads"

# prefix of arm cross compiler installed
# CROSS_COMPILE=armv6-freebsd-

# clean
if [ $# -ge 1 ] && [ $1 = clean ]; then
   echo "clean build"
   rm -rf build/*
fi

/usr/local/bin/gmake -j4 -f Makefile COPYDIR="$BASEQ3_DIR" ARCH=arm \
	CC="${CROSS_COMPILE}cc" USE_SVN=0 USE_CURL=0 USE_OPENAL=0 \
	CFLAGS="-g -DVCMODS_MISC -DVCMODS_NOSDL -DVCMODS_OPENGLES -DVCMODS_DEPTH -DVCMODS_REPLACETRIG $INCLUDES" \
	LDFLAGS="-L"$ARM_LIBS"  -lvchostif -lvcfiled_check -lbcm_host -lkhrn_static -lvchiq_arm -lopenmaxil -lEGL -lGLESv2 -lvcos -lrt"

# copy the required pak3 files over
# cp "$BASEQ3_DIR"/baseq3/*.pk3 "build/release-linux-arm/baseq3/"
# cp -a lib build/release-linux-arm/baseq3/
exit 0

