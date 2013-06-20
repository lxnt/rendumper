#!/bin/sh -xe


SOURCE=`dirname $0` || true
SOURCE=`realpath $SOURCE` || true
DEPSOURCE=$1
BUILD=$2
PREFIX=$3

DEPSOURCE=`realpath $1` || true
BUILD=`realpath $2` || true
PREFIX=`realpath $3` || true

if [ -z "${PREFIX}" -o -z "${SOURCE}" ]
then
    echo "Usage: $0 sources/dir build/dir dest/prefix "
    exit 1
fi

mkdir -p $PREFIX/lib $PREFIX/bin $PREFIX/include $BUILD

if [ "x86_64" = `uname -m` ]
then
    if [ ! -e $PREFIX/lib/libfreetype.so ]
    then
        echo "Unpacking libfreetype into $PREFIX"
        mkdir -p ftpkg
        cd ftpkg
        apt-get download libfreetype6:i386 libfreetype6-dev:i386
        dpkg -x libfreetype6_* .
        dpkg -x libfreetype6-dev_* .
        mv usr/bin/* $PREFIX/bin/
        mv usr/lib/i386-linux-gnu/* $PREFIX/lib/
        mv usr/include/* $PREFIX/include/
        cd ..
        rm -r ftpkg
    fi

    GLEW_TGT=$PREFIX/lib/libGLEW.so
    if [ ! -e $GLEW_TGT ]
    then
        for GLEW in 1.8 1.7 1.6
        do
            ln -s /usr/lib/i386-linux-gnu/libGLEW.so.$GLEW $GLEW_TGT && break
        done

        if [ ! -e $GLEW_TGT ]
        then
            echo "Please install libglew-dev and corresponding version of libglew:i386"
            exit
        fi
    fi

    ln -sf /usr/lib/i386-linux-gnu/mesa/libGL.so.1.2.0 $PREFIX/lib/libGL.so

    B32="-DBUILD_32BIT=ON"
    HOST="--host=i686-linux-gnu"
    LDFLAGS="-L$PREFIX/lib -L/usr/lib/i386-linux-gnu -L/usr/lib32"
else
    B32=""
    HOST=""
    LDFLAGS="-L$PREFIX/lib"
fi

if [ ! -d $DEPSOURCE/SDL2-2.0.0 ]
then
    echo "Downloading SDL2 source"
    cd $DEPSOURCE
    wget http://www.libsdl.org/tmp/release/SDL2-2.0.0.tar.gz
    tar zxf SDL2-2.0.0.tar.gz
fi

if [ ! -d $DEPSOURCE/SDL_pnglite ]
then
    echo "Cloning SDL_pngline repository"
    git clone git://github.com/lxnt/SDL_pnglite.git $DEPSOURCE/SDL_pnglite
fi

if [ ! -d $DEPSOURCE/harfbuzz-0.9.18 ]
then
    echo "Downloading HarfBuzz source"
    cd $DEPSOURCE
    wget http://www.freedesktop.org/software/harfbuzz/release/harfbuzz-0.9.18.tar.bz2
    tar jxf harfbuzz-0.9.18.tar.bz2
fi

if [ ! -d $DEPSOURCE/zhban ]
then
    echo "Cloning zhban repository"
    git clone git://github.com/lxnt/zhban.git $DEPSOURCE/zhban
fi

cd $BUILD
#rm -fr sdl2 harfbuzz zhban sdl2pnglite rdx
mkdir -p sdl2 harfbuzz zhban sdl2pnglite rdx

export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig
export LDFLAGS

( cd $BUILD/sdl2 && $DEPSOURCE/SDL2-2.0.0/configure --prefix=$PREFIX && make -j4 install ) || exit

( cd $BUILD/sdl2pnglite && cmake -DCMAKE_INSTALL_PREFIX=$PREFIX -DCMAKE_BUILD_TYPE=Release $DEPSOURCE/SDL_pnglite && make -j2 install ) || exit

( cd $BUILD/harfbuzz && $DEPSOURCE/harfbuzz-0.9.18/configure --prefix=$PREFIX --without-glib --without-cairo && make -j4 install ) || exit

( cd $BUILD/zhban && cmake -DCMAKE_INSTALL_PREFIX=$PREFIX -DCMAKE_BUILD_TYPE=Debug $DEPSOURCE/zhban && make -j2 install ) || exit

cp /usr/lib/i386-linux-gnu/libGLEW.so.1.6 /usr/lib/i386-linux-gnu/libGLEW.so.1.6.0 $PREFIX/lib/ # they dropped it from 12.10 or smth

( cd $BUILD/rdx && cmake -DCMAKE_INSTALL_PREFIX=$PREFIX \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_MODULES=on \
    -DBUILD_LIBGRAPHICS=on \
    -DRENDERER_SDL2GL2=off \
    -DRENDERER_NCURSES=off \
    -DPLATFORM_NCURSES=off \
    $SOURCE \
    && make -j2 install ) || exit



