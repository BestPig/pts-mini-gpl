#! /usr/local/bin/stackless2.6
# by pts@fazekas.hu at Sun Apr 18 18:16:11 CEST 2010

import sys
import os

from distutils.core import Extension
from distutils.core import setup
from distutils.util import get_platform

# Compile fooo.c to fooo.so
if len(sys.argv) == 1:
  sys.argv = [sys.argv[0], 'build']
  os.system('rm -rf build')
  setup(ext_modules=[Extension('fooo', ['fooo.c'])])

# Load fooo.so
sys.path[:0] = ['build/lib.%s-%s' % (get_platform(), sys.version[0 : 3])]
import fooo

import stackless
import sys

# No segfault if we call SoftMainLoop instead of fooo.SoftMainLoop.
#stackless.enable_softswitch(0)
#def SoftMainLoop():
#  stackless.schedule()
#  stackless.schedule()
#  raise AssertionError

stackless.tasklet(fooo.SoftMainLoop)()
def Work():
  print 'exiting'
  sys.exit()
stackless.tasklet(Work)()
print 'last breath'
stackless.schedule_remove()
