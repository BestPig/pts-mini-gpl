#! /usr/bin/python2.4

"""pywfotd: PYthon Web Framework Of The Day

by pts@fazekas.hu at Sat Mar 27 19:54:26 CET 2010

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

This is a mostly featureless web framework for education purposes. The only
feature is how little the programmer has to type to get basic HTTP request
functionality.

Example webapp:

  from wfotd import *

  @GET
  def _(name=None):
    name = name or 'World'
    return 'Hello, %s!' % name

Please note the absence of the call to the main loop. (wfotd does it
via sys.exitfunc.)
"""

__author__ = 'pts@fazekas.hu (Peter Szabo)'


import atexit
import logging
import re
import sys
import types

__all__ = ['logging', 'GET']

logging.BASIC_FORMAT = '[%(created)f] %(levelname)s %(message)s'
logging.root.setLevel(logging.INFO)  # Prints INFO, but not DEBUG.

# If nonempty, ServerFunc won't be started at Python exit.
do_prevent_server_at_exit = []

old_exitfunc = getattr(sys, 'exitfunc', None)

def ExitFunc():
  try:
    if not do_prevent_server_at_exit:
      ServerFunc()
    sys.exit(0)
  except SystemExit:
    exc_info = sys.exc_info()
    if old_exitfunc is not None:
      old_exitfunc()  # Run atexit hooks.
    raise exc_info[0], exc_info[1], exc_info[2]
  except:
    exc_info = sys.exc_info()
    import traceback
    print >>sys.stderr, "Error in automatic pywfotd server:"
    traceback.print_exc(exc_info)
    if old_exitfunc is not None:
      old_exitfunc()  # Run atexit hooks.
    # Don't raise it (already printed)
    sys.exit(1)

sys.exitfunc = ExitFunc

orig_excepthook = sys.excepthook

def ExceptHook(*args):
  do_prevent_server_at_exit.append(1)
  orig_excepthook(*args)

sys.excepthook = ExceptHook

REGISTERED_PATHS = []

def Register(path, function):
  path = path.rstrip('/')
  if path:
    assert path.startswith('/')
  else:
    path = '/'
  assert '//' not in path
  # Only Python functions are OK.
  assert isinstance(function, types.FunctionType)
  REGISTERED_PATHS.append((path, function))

def SortRegisteredPathsBySize():
  """Sort registered paths by the size of the path.
  
  This is to make sure that /foo matches before /foo/bar.
  """
  REGISTERED_PATHS.sort(lambda a, b: len(b[0]) - len(a[0]))

def WsgiApp(environ, start_response):
  # TODO(pts): don't print request traceback to client
  # (CherryPy WSGI server does this)
  path_info = environ['PATH_INFO']
  path_rest = None
  for path, function in REGISTERED_PATHS:
    if path == path_info:
      path_rest = []
      break
    elif path == '/' or path_info.startswith(path + '/'):
      path_rest = path_info[len(path):].strip('/')
      if '//' in path_rest:
        path_rest = re.sub('//+', '/', path_rest)
      if path_rest:
        path_rest = path_rest.split('/')
      else:
        path_rest = []
      break
  if path_rest is None:
    msg = 'path %s not found' % path_info
    start_response('404 Not Found', [('Content-Type', 'text/plain'),
                                     ('Content-Length', str(len(msg)))])
    return [msg]
  logging.info('handling for path %r' % path)
  status = '200 OK'
  response_headers = [('Content-Type', 'text/html')]
  start_response(status, response_headers)
  # TODO(pts): Let function set the HTTP status.
  # TODO(pts): Handle argument count mismatch (TypeError).
  return function(*path_rest)

def ServerFunc():
  logging.info('pywfotd server startup')
  RegisterPathsByDict()
  SortRegisteredPathsBySize()

  if not REGISTERED_PATHS:
    logging.error('no path handlers registered')
    sys.exit(3)
  logging.info('registered paths: %r' %
               [pathx[0] for pathx in REGISTERED_PATHS])

  bind_host = '0.0.0.0'  # Bind on all interfaces.
  bind_port = 8080
  if len(sys.argv) == 2:
    try:
      port = int(sys.argv[1])
    except ValueError:
      pass
  if len(sys.argv) == 3:
    bind_host = sys.argv[1]
    try:
      port = int(sys.argv[2])
    except ValueError:
      pass
    
  import wsgiserver
  server = wsgiserver.CherryPyWSGIServer((bind_host, bind_port), WsgiApp)

  def Tick():
    listen_host, listen_port = server.socket.getsockname()
    logging.info('pywfotd listening on http://%s:%s/',
                 listen_host, listen_port)
    logging.info('press Ctrl-<C> to abort the server')
    del server.tick
    server.tick()

  server.tick = Tick

  try:
    server.start()
  finally:  # for KeyboardInterrupt and SystemExit
    server.stop()

REGISTERED_PATHS_BY_DICT = []

def RegisterPathsByDict():
  """Move items from REGISTERED_PATHS_BY_DICT to REGISTERED_PATHS."""
  # TODO(pts): Remove partially from REGISTERED_PATHS_BY_DICT on exception.
  for function, orig_dicts in REGISTERED_PATHS_BY_DICT:
    dicts = list(orig_dicts)
    components = ['']
    while len(dicts) > 1:
      parent = dicts.pop()
      child = dicts[-1]
      keys = [key for key in sorted(parent) if
              getattr(parent[key], '__dict__', None) is child]
      assert keys, 'class not found for function %s, %d of %d levels left' % (
          function.func_name, len(dicts), len(orig_dicts) - 1)
      assert len(keys) == 1, (
          'multiple classes for function %s, %d of %d levels left' %
          (function.func_name, len(dicts), len(orig_dicts) - 1))
      assert not keys[0].startswith('_')
      components.append(keys[0])
    function2 = dicts[0][function.func_name]
    if isinstance(function2, staticmethod):
      function2 = function2.__get__(0)  # Get the function.
    assert function is function2
    if function.func_name != '_':
      components.append(function.func_name)
    if len(components) == 1:
      path = '/'
    else:
      path = '/'.join(components)
    Register(path, function)

  del REGISTERED_PATHS_BY_DICT[:]

def RegisterWithDecorator(method, function, frame):
  assert function.func_globals is frame.f_globals
  depth = 0
  dicts = []
  while frame.f_locals is not function.func_globals:
    dicts.append(frame.f_locals)
    depth += 1
    frame = frame.f_back
    assert frame
  dicts.append(function.func_globals)
  REGISTERED_PATHS_BY_DICT.append((function, dicts))
  if len(dicts) > 1:
    return staticmethod(function)
  else:
    return function

def GET(function):
  return RegisterWithDecorator('GET', function, sys._getframe().f_back)
