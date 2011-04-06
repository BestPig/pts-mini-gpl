README for wnckkeys
~~~~~~~~~~~~~~~~~~~
by pts@fazekas.hu at Wed Apr  6 14:38:30 CEST 2011

wnckkeys is a fork of xbindkeys 1.8.3 which adds libwnck support for
controlling the window manager, and adds some special commands to activate
windows up, down, left or right to the current window.

Example compilation on Ubuntu Lucid:

  $ sudo apt-get install libwnck-dev
  $ ./configure
  $ make
  $ sudo make install

Example run:

  $ ./wnckkeys -n -f example.config.txt
  # Press Ctrl-Windows-<Left>, Ctrl-Windows-<Right>, Ctrl-Windows-<Up> or
  # Ctrl-Windows-<Down> to activate a window relative the the currently
  # active window.

__EOF__
