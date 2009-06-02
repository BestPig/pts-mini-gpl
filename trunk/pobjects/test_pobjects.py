#! /usr/bin/python2.4
# by pts@fazekas.hu at Tue May 26 16:47:13 CEST 2009

import pobjects

class A:
  #__metaclass__ = type
  @final
  def Foo():
    pass

class B(A):
  def Foo():
    pass

#print type(A)
