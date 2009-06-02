#! /usr/bin/python2.4
# by pts@fazekas.hu at Tue May 26 16:47:13 CEST 2009

import pobjects

class M:
  def F():
    pass

class N(M):
  @override
  def F():
    pass

assert type(N) is type

class E(Exception):
  @final
  def Eoo():
    pass

class A:
  #__metaclass__ = type
  @final
  def Foo():
    pass

assert type(A) is not type

class B(A):
  def Foo():
    pass

#print type(A)
