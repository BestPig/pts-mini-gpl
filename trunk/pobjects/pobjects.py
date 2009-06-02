#! /usr/bin/python2.4
#
# pobject.py: @override, @final etc. decorator support in Python2.4
# by pts@fazekas.hu at Thu Mar 26 14:32:12 CET 2009
#
# pobjects works in python2.4 and python2.5.
#

import linecache
import re
import sys
import traceback
import types

class AbstractMethodError(Exception):
  """Raised when an abstract method is called."""


class BadMethod(Exception):
  """Raised when a bad method definition is encountered."""

class BadDecoration(Exception):
  """Raised when a decorator cannot be applied."""


def _ParseClassNameList(spec, f_globals, f_builtins):
  if not isinstance(spec, str): raise TypeError
  classes = []
  for spec_item in spec.split(','):
    path = spec_item.strip().split('.')
    # TODO(pts): More detailed error reporting.
    if '' in path: return None
    path_start = path.pop(0)
    if path_start in f_globals:
      value = f_globals[path_start]
    elif path_start in f_builtins:
      value = f_builtins[path_start]
    else:
      # TODO(pts): More detailed error reporting.
      return None
    for path_item in path:
      # TODO(pts): More detailed error reporting.
      if not hasattr(value, path_item): return None
      value = getattr(value, path_item)
    classes.append(value)
  # TODO(pts): More detailed error reporting.
  if not classes: return None
  return classes


def _Self(value):
  """Helper identity function."""
  return value


def _UnwrapFunction(fwrap):
  """Unwarp function from function, classmethod or staticmethod.
  
  Args:
    fwrap: A function, classmethod or staticmethod.
  Returns:
    (function, wrapper), where `function' is the function unwrapped from
    fwrap, and wrapper is a callable which can be called on function to
    yield something equivalent to fwrap. The value (None, None) is returned
    if fwrap is is not of the right type.
  """
  if isinstance(fwrap, types.FunctionType):
    return fwrap, _Self
  elif isinstance(fwrap, classmethod):
    return fwrap.__get__(0).im_func, classmethod
  elif isinstance(fwrap, staticmethod):
    return fwrap.__get__(0), staticmethod
  else:
    return None, None


def _GetCurrentClassInfo(f, error_prefix=''):
  """Return (metaclass, bases) pair for a class being defined in frame f."""
  # !! cache the result, reuse for subsequent methods.
  file_name = f.f_back.f_code.co_filename
  line_number = f.f_back.f_lineno
  linecache.checkcache(file_name)
  line = linecache.getline(file_name, line_number)
  if not line:
    raise BadDecoration(
        '%sline %d of file %r not found'
        (error_prefix, line_number, file_name))
  #print 'DDD', decorator.func_name, full_method_name, repr(line)
  # TODO(pts): Accept multiline class definitions, with comments.
  # TODO(pts): Special error message for old-style class definition.
  match = re.match(r'[ \t]*class[ \t]+([_A-Za-z]\w*)[ \t]*'
                   r'\(([,. \t\w]+)\)[ \t]*:[ \t]*(?:#.*)?$', line)
  if not match:
    if not re.match(r'[ \t]*class[ \t]+([_A-Za-z]\w*)[ \t]*:'
                r'[ \t]*(?:#.*)?$', line):
      raise BadDecoration(
          '%sline %d of file %r (%r) does not contain a valid class def' %
          (error_prefix, line_number, file_name, line))
    bases = []
  else:
    if match.group(1) != f.f_code.co_name:
      raise BadDecoration(
          '%sline %d of file %r (%r) does not define class %s' %
          (error_prefix, line_number, file_name, line, f.f_code.co_name))
    bases = _ParseClassNameList(
        spec=match.group(2), f_globals=f.f_back.f_globals,
        f_builtins=f.f_back.f_builtins)
    if bases is None:
      raise BadDecoration(
          '%sline %d of file %r (%r) has unparsable superclass list %r' %
          (error_prefix, line_number, file_name, line, match.group(2)))
  if '__metaclass__' in f.f_locals:
    # This doesn't work if the __metaclass__ assignment is later than us.
    metaclass = f.f_locals['__metaclass__']
  elif bases and issubclass(type(bases[0]), type):  # new-style class
    metaclass = type(bases[0])
  else:
    metaclass = None  # old-style class
  return metaclass, bases, line_number, file_name, line


def pobject_decorator(decorator):
  #print 'MMM', decorator.func_name
  def ApplyDecorator(fwrap):
    """Applies decorator to function in f (function or method)."""
    function, wrapper = _UnwrapFunction(fwrap)
    if not isinstance(function, types.FunctionType):
      raise BadDecoration(
          'decorator @%s cannot be applied to non-function %r' %
          (decorator.func_name, function))
    f = sys._getframe().f_back
    if '__module__' not in f.f_locals:
      raise BadDecoration(
          'decorator @%s cannot be applied to function %s in %s, '
          'because the latter is not a class definition' %
          (decorator.func_name, function.func_name, f.f_code.co_name))
    module_name = f.f_locals['__module__']
    full_method_name = '%s.%s.%s' % (
        module_name, f.f_code.co_name, function.func_name)
    error_prefix = 'decorator @%s cannot be applied to %s, because ' % (
        decorator.func_name, full_method_name)
    metaclass, bases, line_number, file_name, line = _GetCurrentClassInfo(
        f, error_prefix=error_prefix)
    if not metaclass or not issubclass(metaclass, _PObjectMeta):
      # This for loop is to give a better error message than:
      # TypeError: Error when calling the metaclass bases:: metaclass
      # conflict: the metaclass of a derived class must be a (non-strict)
      # subclass of the metaclasses of all its bases
      for base in bases:
        if (issubclass(type(base), type) and
            not issubclass(_PObjectMeta, type(base))):
          raise BadDecoration(
              '%sline %d of file %r (%r) specifies superclass %s, which '
              'has conflicting metaclass %s.'
              (error_prefix, line_number, file_name, line, base, type(base)))
      # This silently upgrades an old-style class to a new-style class.
      f.f_locals['__metaclass__'] = metaclass = _PObjectMeta
    decorated_function = decorator(
        function=function, full_method_name=full_method_name)
    #print (decorator.func_name, decorated_function, function, wrapper)
    if decorated_function is function:
      return fwrap  # The wrapped function, classmethod or staticmethod.
    else:
      return wrapper(decorated_function)
  return ApplyDecorator


def _GenerateBothAbstractAndFinal(full_method_name):
  return 'decorators @abstract and @final cannot be both applied to %s' % (
      full_method_name)

def _GenerateBothAbstractAndFinalim(full_method_name):
  return 'decorators @abstract and @finalim cannot be both applied to %s' % (
      full_method_name)

@pobject_decorator
def abstract(function, full_method_name):
  assert type(function) == types.FunctionType

  if getattr(function, '_is_final', None):
    raise BadDecoration(_GenerateBothAbstractAndFinal(full_method_name))

  # We'd lose ._is_finalim on AbstractFunction anyway.
  if getattr(function, '_is_finalim', None):
    raise BadDecoration(_GenerateBothAbstractAndFinalim(full_method_name))

  def AbstractFunction(self, *args, **kwargs):
    if not isinstance(self, type):
      self = type(self)
    # TODO(pts): Extract class name from self.
    raise AbstractMethodError(
        'abstract method %s.%s.%s called' %
        (self.__module__, self.__name__, function.func_name))

  AbstractFunction._is_abstract = True

  return AbstractFunction


@pobject_decorator
def nosuper(function, full_method_name):
  assert type(function) == types.FunctionType
  function._is_nosuper = True
  return function


@pobject_decorator
def final(function, full_method_name):
  assert type(function) == types.FunctionType

  if getattr(function, '_is_abstract', None):
    raise BadDecoration(_GenerateBothAbstractAndFinal(full_method_name))

  function._is_final = True
  return function


@pobject_decorator
def finalim(function, full_method_name):
  """Declare that the method cannot be overriden with instance methods."""
  assert type(function) == types.FunctionType

  function._is_finalim = True
  return function


@pobject_decorator
def override(function, full_method_name):
  assert type(function) == types.FunctionType
  function._is_override = True
  return function


def _DumpSuperClassList(bases):
  if len(bases) == 1:
    return 'superclass %s.%s' % (bases[0].__module__, bases[0].__name__)
  else:
    return 'superclasses ' + ', '.join(
        ['%s.%s' % (base.__module__, base.__name__) for base in bases])


class _PObjectMeta(type):
  def __new__(cls, class_name, bases, dict_obj):
    # cls is _PObjectMeta here.
    #print ('NewClass', cls, class_name, bases, dict_obj)
    module = dict_obj['__module__']
    for name in sorted(dict_obj):
      function, _ = _UnwrapFunction(dict_obj[name])
      if isinstance(function, types.FunctionType):
        #print (class_name, name)
        if getattr(function, '_is_nosuper', None):
          bases_with_name = [base for base in bases if hasattr(base, name)]
          if bases_with_name:
            # Unfortunately, we don't get the method definition line in the
            # traceback. TODO(pts): Somehow forge it.
            raise BadMethod(
                '@nosuper method %s.%s.%s defined in %s' %
                (module, class_name, name,
                 _DumpSuperClassList(bases_with_name)))
        if getattr(function, '_is_override', None):
          bases_with_name = [base for base in bases if hasattr(base, name)]
          if not bases_with_name:
            raise BadMethod(
                '@override method %s.%s.%s not defined in %s' %
                (module, class_name, name, _DumpSuperClassList(bases)))
        # We don't need any special casing for getattr(..., '_is_final', None) below
        # if getattr(base, name) is an ``instancemethod'' created from a
        # classmethod or a function. This is because an instancemathod
        # automirorrs all attributes of its im_func.
        bases_with_final = [
            base for base in bases if hasattr(base, name) and
            getattr(getattr(base, name), '_is_final', None)]
        if bases_with_final:
          raise BadMethod(
              'method %s.%s.%s overrides @final method in %s' %
                (module, class_name, name,
                 _DumpSuperClassList(bases_with_final)))
        if function is dict_obj[name]:  # function is instance method
          bases_with_finalim = [
              base for base in bases if hasattr(base, name) and
              getattr(getattr(base, name), '_is_finalim', None)]
          if bases_with_finalim:
            raise BadMethod(
                'instance method %s.%s.%s overrides @finalim method in %s' %
                  (module, class_name, name,
                   _DumpSuperClassList(bases_with_finalim)))

    obj = type.__new__(cls, class_name, bases, dict_obj)
    #print 'NEW', obj, dir(obj)
    return obj

class pobject(object):
  __metaclass__ = _PObjectMeta  # be type(pobjects) == _PObjectMeta, not type  

# Updates for all modules.
__builtins__['pobject'] = pobject
__builtins__['final'] = final
__builtins__['finalim'] = finalim
__builtins__['nosuper'] = nosuper
__builtins__['abstract'] = abstract
__builtins__['override'] = override
