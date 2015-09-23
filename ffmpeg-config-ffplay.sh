
#SDL_CFLAGS=/mingw/SDL-1.2/include
#SDL_LIBS=/mingw/SDL-1.2/lib

./configure \
--enable-debug=3 \
--disable-optimizations \
--disable-stripping \
 \
--arch=i686 \
--disable-asm \
--disable-yasm \
 \
--enable-memalign-hack \
--enable-ffplay
