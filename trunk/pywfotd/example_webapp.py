#! /usr/bin/python2.4
# by pts@fazekas.hu at Sat Mar 27 20:27:36 CET 2010

# Web Framework Of The Day
from wfotd import *

@GET
def _(a=None, b=None):
  if b is None:
    a = 1
    b = 100
    pre = 'Think of a number between %d and %d (inclusive).<br>\n' % (a, b)
  else:
    pre = ''
    a = int(a)
    b = int(b)
  if a >= b:
    return ('The number you have thinked of is %d.<br>\n'
            '<a href="/">Play again</a>' % a)
  m = (a + b + 1) >> 1
  return ('%sIs it less than %d? '
          '<form action="/%d/%d"><input type=submit value="Yes."></form>'
          '<form action="/%d/%d"><input type=submit value="No."></form>'
          % (pre, m, a, m - 1, m, b))

class foo:
  class bar:
    @GET
    def baz():
      return 'Bazinga!'
