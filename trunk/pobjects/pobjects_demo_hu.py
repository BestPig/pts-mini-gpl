Szabó Péter <pts@fazekas.hu>       2009-05-27            budapest.py

# --- célok

class Foo(object):
  @abstract
  def F(self):
    pass  # dobjon AbstractMethodError-t híváskor

  @override  # dobjon kivételt definiáláskor
  def G(self, x):
    print x

  @final
  def H(self, x):
    print x

class Bar(Foo):
  def H(self, x):  # dobjon kivételt definiáláskor
    print x

# Egyeb megvalósítandó szolgáltatások:
# @nosuper: @override ellentéte
# @finalim: alosztályban csak @classmethod vagy @staticmethod lehet


# --- eszköz: dekorátor (decorator):

def Change(f, a, b):
  return a * b

@Change(6, 7)
def F(x, y, z):
  assert 0, (x, y, z)

print F           #: 42
print F(2, 3, 4)  #: hiba: F nem függvény

@Change  #: SyntaxError: csak def-et lehet dekorálni
class Foo(object):
  pass









# --- hasznos beépített dekorátorok (1/2)

class Rectangle(object):
  def __init__(self, width, height):
    self._width = self.CheckPositiveDimension(width)
    self._height = self.CheckPositiveDimension(height)

  @staticmethod
  def CheckPositiveDimension(x):
    x = float(x)
    assert x > 0
    return x

  @property
  def width(self): return self._width

  @property
  def height(self): return self._height

print Rectangle.CheckPositiveDimension(42)  #: 42.0
print Rectangle(8, 9).width  #: 8



# --- hasznos beépített dekorátorok (2/2)

class LandscapeRectangle(Rectangle):
  def __init__(self, width, height):
	self.CheckLandscape(width, height)
    Rectangle.__init__(self, width, height)

  @classmethod
  def CheckLandscape(cls, width, height):
    width = cls.CheckPositiveDimension(width)
    height = cls.CheckPositiveDimension(height)
    assert width >= height

LandscapeRectangle.CheckLandscape(6, 7)  # AssertionError

# * A @property egy ún. descriptor-t hoz létre, lásd még
#   http://users.rcn.com/python/download/Descriptor.htm
# * A def mindig függvényt hoz létre, ami osztály definiálásakor
#   instancemethod-dá alakul. Más típusúak nem alakulnak át.
# * Amit a @classmethod és a @staticmethod visszaad, az már nem
#   függvény (hanem classmethod ill. staticmethod típusú), ezért
#   nem is alakul át osztály definiálásakor.


# --- @abstract megvalósítása dekorátorral

class AbstractMethodError(Exception): pass

def abstract(func):
  def AbstractFunction(self):
    raise AbstractMethodError('%s.%s.%s' %
	    (self.__module__, self.__name__, func.func_name))
  return AbstractFunction















# --- @override megvalósítási ötlete

def override(func)
  import sys
  assert [s for s in GetSuperClasses(sys._getframe().f_back)
          if hasattr(s, func.func_name)], '@override mismatch'
  return func

def GetSuperClasses(f)  # f: frame objektum
  while f:
    print (f.f_code.co_name, f.f_code.co_filename, f.f_lineno,
	       sorted(f.f_locals), sorted(f.f_globals),
		   sorted(f.f_builtins))
    f = f.f_back
  return ...









# --- GetSuperClasses megvalósítása

def GetSuperClasses(f)  # f: frame objektum
  import linecache
  # az f a metódus def-je
  # az f.f_back az osztálydefiníció (class)
  file_name = f.f_back.f_code.co_filename
  line_number = f.f_back.f_lineno
  linecache.checkcache(file_name)
  line = linecache.getline(file_name, line_number)
  assert line
  import re
  match = re.match(r'\s*class\s+...', line):
  assert match
  return [...]









# --- a @final problémája

class Foo(object):
  @final
  def H(self, x):
    print x

class Bar(Foo):
  def H(self, x):  # dobjon kivételt
    print x

# a Bar.H-ban a Foo.H @final dekorátorának kéne a kivételt dobnia












# --- metaclass

class MyMeta(type):
  def __new__(cls, class_name, bases, dict_obj):
    print (class_name, bases, sorted(dict_obj))
    return type.__new__(cls, class_name, bases, dict_obj)

class A(object):
  def M(self, x): print x
	
class B(object):
  __metaclass__ = MyMeta
  def M(self, x): print x
class C(B):
  def M(self, x): print x
  def L(self, x): print x

#: ('B', (<type 'object'>,), ['M', '__metaclass__', '__module__'])
#: ('C', (<class '__main__.B'>,), ['L', 'M', '__module__'])
print type(A)  #: <type 'type'>
print type(B)  #: <class '__main__.MyMeta'>
print type(C)  #: <class '__main__.MyMeta'>


# --- @final megvalósítása metaclass-szal

def final(func):
  func._is_final = True
  return func

class ClassCheckMeta(type):
  def __new__(cls, class_name, bases, dict_obj):
    for s in bases:
      assert not [a for a in dir(s) and name in dict_obj and
                  getattr(s, '_is_final', None)]
    return type.__new__(cls, class_name, bases, dict_obj)

class Foo(object):
  __metaclass__ = ClassCheckMeta
  @final
  def H(self, x): print x

class Bar(Foo):  # kivételt dob
  def H(self, x): print x




# --- ha a felhasználó elfelejti a __metaclass__-t

class Foo(object):
  @final
  def H(self, x): print x

class Bar(Foo):  # sajnos átcsúszik kivétel nélkül
  def H(self, x): print x

# Jó megoldási ötlet:

def final(func):
  import sys
  f = sys._getframe().f_back
  f.f_locals['__metaclass__'] = ClassCheckMeta  # általánosítandó
  func._is_final = True
  return func

# Ugyan a metaclass kiváltja az @override-hoz szükséges
# linecache-t, viszont linecache a final általánosításához
# kelleni fog.



# --- hogy ne kelljen @pobjects.override-ot írni

# pobjects.py-ban:
__builtins__['final'] = final
__builtins__['finalim'] = finalim
__builtins__['nosuper'] = nosuper
__builtins__['abstract'] = abstract
__builtins__['override'] = override

# example.py-ban:
import pobjects
class Foo:
  @override  # a közös __builtins__-ből véve
  def G(self, x):
    print x







"""Vége."""









