#! /bin/bash
set -ex
PREFIX=/usr/local/google/more-compiler-i686
CMDPREFIX="$PREFIX/bin/i686-"
export CC="${CMDPREFIX}gcc -static"

if ! test -f php-5.3.3.tar.bz2.downloaded; then
  wget -c -O php-5.3.3.tar.bz2 http://us.php.net/get/php-5.3.3.tar.bz2/from/us2.php.net/mirror
  touch php-5.3.3.tar.bz2.downloaded
fi

rm -rf mkphp.tmp
mkdir mkphp.tmp
(cd mkphp.tmp && tar xjf ../php-5.3.3.tar.bz2)
mv mkphp.tmp/php-* mkphp.tmp/phpsrc
(cd mkphp.tmp/phpsrc && patch -p1 <../../pts-php-5.3.3-static.patch)

# --enable-mbstring would increase the binary size by 1881K
(cd mkphp.tmp/phpsrc && ./configure --disable-cgi --enable-cli --enable-embed=static --disable-shared --with-libxml-dir="$PREFIX" --without-iconv --enable-bcmath --with-zlib --with-bz2="$PREFIX" --enable-calendar --enable-exif --enable-ftp --enable-zip --enable-pcntl --with-readline="$PREFIX" --enable-soap --enable-sockets --enable-sqlite-utf8 --enable-wddx)
(make -C mkphp.tmp/phpsrc sapi/cli/php EXTRA_LDFLAGS_PROGRAM="-all-static")
${CMDPREFIX}strip mkphp.tmp/phpsrc/sapi/cli/php
cp mkphp.tmp/phpsrc/sapi/cli/php php-5.3.3
ls -l php-5.3.3

