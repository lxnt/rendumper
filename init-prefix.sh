#!/bin/sh -xe

SOURCE=$1
PREFIX=$2

SOURCE=`realpath $1` || true
PREFIX=`realpath $2` || true

if [ -z "${PREFIX}" -o -z "${SOURCE}" ]
then
    echo "Usage: $0 deps/dir dest/prefix "
    exit 1
fi

mkdir -p $PREFIX/lib $PREFIX/bin $PREFIX/include

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

    for GLEW in 1.8 1.7 1.6 
    do
        ln -s /usr/lib/i386-linux-gnu/libGLEW.so.$GLEW $GLEW_TGT && break
    done

    ln -s /usr/lib/i386-linux-gnu/mesa/libGL.so.1.2.0 $PREFIX/lib/libGL.so

    if [ ! -e $GLEW_TGT ]
    then
        echo "Please install libglew-dev and corresponding version of libglew:i386"
        exit
    fi
    
    B32="-DBUILD_32BIT=ON"
    HOST="--host=i686-linux-gnu"
    LDFLAGS="-L$PREFIX/lib -L/usr/lib/i386-linux-gnu -L/usr/lib32"
else
    B32=""
    HOST=""
    LDFLAGS="-L$PREFIX/lib"
fi

if [ ! -d $SOURCE/SDL ]
then
    echo "Cloning SDL2 repository"
    hg clone http://hg.libsdl.org/SDL $SOURCE/SDL
fi

if [ ! -d $SOURCE/SDL_pnglite ]
then
    echo "Cloning SDL_pngline repository"
    git clone git@github.com:lxnt/SDL_pnglite.git $SOURCE/SDL_pnglite
fi

if [ ! -d $SOURCE/SDL_ttf ]
then
    echo "Cloning SDL_ttf repository"
    hg clone http://hg.libsdl.org/SDL_ttf $SOURCE/SDL_ttf
fi

rm -fr sdl2 sdl2ttf sdl2pnglite 
mkdir -p sdl2 sdl2ttf sdl2pnglite

#export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig
export LDFLAGS

( cd sdl2 && $SOURCE/SDL/configure $HOST --prefix=$PREFIX && make -j6 install ) || exit

( cd sdl2ttf && $SOURCE/SDL_ttf/configure $HOST --with-freetype-prefix=$PREFIX --prefix=$PREFIX && make -j4 install ) || exit

( cd sdl2pnglite && cmake -DCMAKE_INSTALL_PREFIX=$PREFIX $B32 $SOURCE/SDL_pnglite && make -j2 install ) || exit




