#! /bin/bash --
# by pts@fazekas.hu at Thu May 19 13:11:32 CEST 2011

TOMCAT_ARCHIVE=apache-tomcat-6.0.32.tar.gz
JETTY_ARCHIVE=jetty-distribution-7.4.1.v20110513.tar.gz
BUILD_TIME=$(TZ=GMT LC_ALL=C date)

function do_extract() {
  local ARCHIVE="$1" DSTDIR="$2"
  local XARCHIVE="$ARCHIVE"
  test "$XARCHIVE" = "${XARCHIVE#/}" && XARCHIVE="$PWD/$XARCHIVE"
  (cd "$DSTDIR" && tar xzf "$XARCHIVE")
}

function do_extract_jar() {
  local ARCHIVE="$1" DSTDIR="$2"
  local XARCHIVE="$ARCHIVE"
  test "$XARCHIVE" = "${XARCHIVE#/}" && XARCHIVE="$PWD/$XARCHIVE"
  (cd "$DSTDIR" && unzip -o -- "$XARCHIVE")
}

function do_extract_and_move() {
  local ARCHIVE="$1" DSTPARENTDIR="$2" DIR="$3"
  mkdir -p "$DSTPARENTDIR/$DIR.x"
  do_extract "$ARCHIVE" "$DSTPARENTDIR/$DIR.x"
  local XDIR=
  for F in "$DSTPARENTDIR/$DIR.x"/*"$DIR"*; do
    test -z "$XDIR" || fail "multiple $DIR dirs"
    XDIR="$F"
  done
  test "$XDIR" || fail "missing $DIR dir"
  mv "$XDIR" "$DSTPARENTDIR/$DIR"
  rmdir "$DSTPARENTDIR/$DIR.x"
}

function fail() {
  set +x
  echo "fatal: $*" >&2
  exit 2
}

TMP=buildtmp

set -ex
rm -rf "$TMP"
mkdir "$TMP" "$TMP/jarsrc"

do_extract_and_move "$TOMCAT_ARCHIVE" "$TMP" tomcat
do_extract_and_move "$JETTY_ARCHIVE" "$TMP" jetty

cp "Jtty.java" "$TMP/jarsrc/"

do_extract_jar "$TMP"/jetty/lib/jetty-continuation-*.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/jetty/lib/jetty-http-*.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/jetty/lib/jetty-io-*.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/jetty/lib/jetty-security-*.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/jetty/lib/jetty-server-*.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/jetty/lib/jetty-servlet-*.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/jetty/lib/jetty-util-*.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/jetty/lib/jetty-webapp-*.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/jetty/lib/jetty-xml-*.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/jetty/lib/jsp/ecj-[0-9]*.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/jetty/lib/servlet-api-[0-9]*.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/tomcat/bin/tomcat-juli.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/tomcat/lib/el-api.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/tomcat/lib/jasper-el.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/tomcat/lib/jasper.jar "$TMP/jarsrc"
do_extract_jar "$TMP"/tomcat/lib/jsp-api.jar "$TMP/jarsrc"

(cd "$TMP/jarsrc" && javac Jtty.java)

rm -rf "$TMP/jarsrc/META-INF"
mkdir  "$TMP/jarsrc/META-INF"
echo "Main-Class: Jtty" >"$TMP/jarsrc/META-INF/MANIFEST.MF"
(echo "BUILD_TIME=$BUILD_TIME"
 echo "TOMCAT_ARCHIVE=$TOMCAT_ARCHIVE"
 echo "JETTY_ARCHIVE=$JETTY_ARCHIVE") >"$TMP/jarsrc/version.txt"

rm -rf "$TMP/jarsrc"/{about.html,about_files,plugin.properties,Jtty.java}
test ! -f "$TMP/jarsrc/about.html"
test -f "$TMP/jarsrc/META-INF/MANIFEST.MF"
test -f "$TMP/jarsrc/Jtty.class"

(cd "$TMP/jarsrc" && zip -9r ../jttyp.jar *)

mv "$TMP/jttyp.jar" .
rm -rf "$TMP"

: All OK, jttyp.jar created.
