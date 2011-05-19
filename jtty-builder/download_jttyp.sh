#! /bin/bash
# by pts@fazekas.hu at Thu May 19 13:57:09 CEST 2011

set -ex

# Landing page: https://github.com/stephenh/jtty
wget -O Jtty.java --no-check-certificate \
    https://github.com/stephenh/jtty/raw/master/src/main/java/Jtty.java

# Landing page: http://tomcat.apache.org/download-60.cgi
wget -O apache-tomcat-6.0.32.tar.gz \
    http://apache.osuosl.org/tomcat/tomcat-6/v6.0.32/bin/apache-tomcat-6.0.32.tar.gz

# Landing page: http://download.eclipse.org/jetty/
wget -O jetty-distribution-7.4.1.v20110513.tar.gz \
    http://download.eclipse.org/jetty/7.4.1.v20110513/dist/jetty-distribution-7.4.1.v20110513.tar.gz

: All downloads OK.
