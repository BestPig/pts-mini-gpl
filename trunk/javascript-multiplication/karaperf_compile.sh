#! /bin/bash --
# by pts@fazekas.hu at Sun Oct  4 19:37:43 CEST 2009
set -ex
../bin/mxmlc -target-player 9 -default-size 800 200 KaraPerf9.as
../bin/mxmlc -target-player 10 -default-size 800 200 KaraPerf.as
: All OK
