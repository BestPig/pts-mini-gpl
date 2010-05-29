#! /usr/bin/python2.4
# by pts@fazekas.hu at Sat May 29 20:34:58 CEST 2010

"""A simple semaphore implementation for Python using based on mutexes."""

import thread
from collections import deque


class Semaphore(object):
  def __init__(self, value=1):
    assert value >= 0, 'Semaphore initial value must be >= 0'
    self._lock = thread.allocate_lock()   # Create a mutex.
    self._waiters = deque()
    self._value = value

  def acquire(self, blocking=True):
    """Semaphore P primitive."""
    self._lock.acquire()
    while not self._value:
      if not blocking:
        break
      assert self._lock.locked(), 'wait() of un-acquire()d lock'
      waiter = thread.allocate_lock()
      waiter.acquire()
      self._waiters.append(waiter)
      self._lock.release()
      try:  # Restore state even if e.g. KeyboardInterrupt is raised.
        waiter.acquire()
      finally:
        self._lock.acquire()
    self._value -= 1
    self._lock.release()

  def release(self):
    """Semaphore V primitive."""
    self._lock.acquire()
    self._value += 1
    assert self._lock.locked(), 'notify() of un-acquire()d lock'
    _waiters = self._waiters
    if _waiters:
      _waiters.popleft().release()
    self._lock.release()


if __name__ == '__main__':
  s = Semaphore(3)
  def Reader():
    while True:
      raw_input()
      s.release()
  thread.start_new_thread(Reader, ())
  print 'Press <Enter> twice to continue.'
  for i in xrange(s._value + 2):
    print i
    s.acquire()
  print 'Done.'
