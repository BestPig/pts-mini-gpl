#! /usr/bin/python
"""Stand-alone lircd implementation for the Hama MCE remote control on Linux.

by pts@fazekas.hu at Sun Oct 18 17:08:26 CEST 2009
--- Thu Mar  4 20:48:36 CET 2010

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

hama_mce_lircd.py is a proof-of-concept stand-alone lircd implementation for
Linux 2.6, which receives and broadcasts button presses from the Hama MCE
remote control.  It is implemented in Python (2.4, 2.5 and 2.6 work), and it
uses only the standard modules.  It has been tested on Ubuntu Jaunty.

hama_mce_lircd.py runs as root, because it has to listen on Unix domain
socket /dev/lircd, and it also needs access to the /dev/input/event* devices.

Installation and usage instructions:

0. Make sure you have a Linux 2.6 system (tested on Ubuntu Jaunty with the
   stock kernel) and you have your Hama MCE remote,
   batteries and its USB dongle.

1. Make sure you have irexec(1), strace(1) and mplayer(1) installed.

   $ sudo apt-get install lirc strace irexec

2. Make sure lircd is not running:

   $ sudo killall lircd

3. In a new terminal window, run

   $ sudo ./hama_mce_lircd.py /dev/lircd

4. Connect the USB dongle of your Hama MCE to one of the USB ports of your
   computers. Watch the output of hama_mce_lircd.py as it recognizes the
   dongle.

5. Insert batteries to your Hama MCE remote.

6. Press any button on your Hama MCE remote. Watch the output of
   hama_mce_lircd.py. For example, if you press the ``OK'' button, you should
   see a line like this:

     info: [...] broadcasting to 0: '0000000000000000 00 enterok hama-mce\n'

7. Make sure that your mplayer has lirc support built in:

   $ mplayer -lircconf /dev/null /dev/null
   (Should not display: Error parsing option... or similar error)

8. Copy or append the bundled lircrc to $HOME/.lircrc . (Make sure you make a
   backup of your original $HOME/.lircrc if necessary.)

9. Run irexec(1) in a new terminal window:

   $ irexec

   and press the stop button (full square at the left side of row 6 from the
   top) on the Hama MCE remote. Watch irexec(1) display `STOP' (as
   configured in $HOME/.lircrc). In the meanwhile, watch the output of
   hama_mce_lircd.py as well. You can abort irexec(1) by pressing Ctrl-<C>.

10. Start media playback with mplayer. Press the stop button on the Hama MCE
    remote. Watch mplayer exit.

11. Study your $HOME/.lircrc for bindings (the `button =' is the name of the
    remote button, as reported by hama_mce_lircd.py, and the `config =' is
    the command executed by mplayer, in the form of /etc/mplayer/input.conf).
    The file $HOME/.lircrc is loaded by mplayer(1) and (irexec(1) and others)
    upon startup.

    Add or change bindings if necessary.

TODO(pts): Document an udev rule which chmods /dev/input/event*
"""

import errno
import fcntl
import re
import os
import select
import socket
import stat
import struct
import sys
import time

INPUT_EVENT_SIZE = len(struct.pack('l', 0)) * 2 + 8
"""16 for i386, 24 for amd64.

struct input_event {  // defined in /usr/include/linux/input.h
  struct timeval {
    long tv_sec;   // int32 or int64; seconds
    long tv_usec;  // int32 or int64; microseconds
  } time;
  uint16 type;
  uint16 code;
  int32 value;
};
"""

assert INPUT_EVENT_SIZE in (16, 24)

# !! use /proc/bus/input/devices instead
def DetectHamaMce():
  """Detect the Hama MCE remote and return the USB HID input event devices.
  
  Returns:
    (keyboard_num, mouse_num) or (None, None) if not found or
    more than one found.
    Example: (6, 7), which means that '/dev/input/event6' is the keyboard
    device, and '/dev/input/event7' is the mouse device.
  Raises:
    OSError:
  """
  # TODO(pts): catch OSError
  # Example: /sys/bus/usb/drivers/usbhid/2-6:1.0/input/input11/event6
  # Example: /sys/bus/usb/drivers/usbhid/2-6:1.1/input/input12/event7
  # TODO(pts): Support multiple connected remotes; they will correspond to
  # each other in mouse_device_nums and other_device_nums.
  device_list_dir = '/sys/bus/usb/drivers/usbhid'
  mouse_device_nums = []
  other_device_nums = []
  for device_entry in sorted(os.listdir('/sys/bus/usb/drivers/usbhid')):
    if ':' in device_entry:
      device_dir = '%s/%s' % (device_list_dir, device_entry)
      feature_entries = os.listdir(device_dir)
      # Example: /sys/bus/usb/drivers/usbhid/2-6:1.0/0003:05A4:9881.0008
      # For the Hama MCE remote: idVendor=0x5a4 idProduct=0x9881
      if ('input' in feature_entries and
          [1 for feature_entry in feature_entries if
           re.match(r'[0-9A-F]{4}:05A4:9881[.][0-9A-F]{4}\Z', feature_entry)]):
        device_event_device_nums = []
        device_mouse_count = 0
        input_dir = '%s/input' % device_dir
        input_entries = os.listdir(input_dir)
        for sub_input_dir in [
            '%s/%s' % (input_dir, input_entry) for input_entry in input_entries
            if re.match(r'input\d+\Z', input_entry)]:
          sub_input_entries = os.listdir(sub_input_dir)
          for event_entry in sub_input_entries:
            match = re.match(r'event(\d+)\Z', event_entry)
            if match:
              device_event_device_nums.append(int(match.group(1)))
          device_mouse_count += len(
              [1 for mouse_entry in sub_input_entries if
               re.match(r'mouse\d+\Z', mouse_entry)])
        if len(device_event_device_nums) == 1:
          if device_mouse_count:
            mouse_device_nums.extend(device_event_device_nums)
          else:
            other_device_nums.extend(device_event_device_nums)
  if len(other_device_nums) == 1 and len(mouse_device_nums) == 1:
    return other_device_nums[0], mouse_device_nums[0]
  else:
    return None, None


class HamaMceNotFound(OSError):
  pass


class HamaMceInput(object):
  """Read events (button presses and releases) from a Hama MCE."""

  __slots__ = ['keyboard_fd', 'mouse_fd', 'keyboard_buf', 'mouse_buf',
               'has_shift', 'do_ignore_release_digit5', 'do_release_start',
               'has_mod42', 'do_release_screenrow',
               'pending_mouse_x', 'pending_mouse_y']

  EVIOCGRAB = 1074021776

  KEYBOARD_CODES = {
      69: '.numlock',
      79: 'digit1',
      80: 'digit2',
      81: 'digit3',
      75: 'digit4',
      76: 'digit5',
      77: 'digit6',
      71: 'digit7',
      72: 'digit8',
      73: 'digit9',
      82: 'digit0',
      55: 'star',
      73: 'digit9',
      62: 'close',
      1: 'clear',
      28: 'enterok',  # SUXX: Enter and OK are the same keys
      56: '.shift',
      125: '.mod125',
      29: '.mod29',
      42: '.mod42',
      104: 'channelplus',   # repeats
      109: 'channelminus',  # repeats
      103: 'up',            # repeats
      108: 'down',          # repeats
      105: 'left',          # repeats
      106: 'right',         # repeats
      14: 'back',
      19: 'record',
      48: 'rewind',
      33: 'forward',
      24: 'screendot',    # in row 3 from top, ``Record TV and radio in Media Center''
      34: 'screentable',  # in row 3 from top, ``Open the TV programme menu''
      20: 'screenhill',   # in row 3 from top, ``Switch to full screen mode''
      50: 'screenufo',    # in row 3 from top, ``Access the  DVD menu directly''
      23: 'green', # ``Access picture library directly''
      18: 'red',  # ``Access video library directly''
      # 'yellow': ``Access TV recording directly''
      # 'blue': ``Access music library directly''
  }
  """Keys are struct input_event.code values from the keyboard device."""
  
  MOUSE_CODES = {
      113: 'volumemute',
      115: 'volumeplus',
      114: 'volumeminus',
      272: 'mouse-leftbutton',
      273: 'mouse-rightbutton',  # SUXX: mouse-rightbutton and info are same
      166: 'stop',
      164: 'playpause',  # SUXX: play and pause are the same key
      165: 'fullrewind', # ``Play previous media file in the playlist''
      163: 'fullforward',
      172: 'browser',
      142: 'power',
  }
  """Keys are struct input_event.code values from the mouse device."""

  def __init__(self):
    """Creates a HamaMceInput without opening it (i.e. doing I/O)."""
    self.keyboard_fd = None
    self.mouse_fd = None
    # [event_counter, delta_movement, first_timestamp, last_timestamp,
    #  before_last_timestamp]
    self.pending_mouse_x = [0, 0, 0.0, 0.0, 0.0]
    self.pending_mouse_y = [0, 0, 0.0, 0.0, 0.0]
    self.Close()

  def __del__(self):
    self.Close()

  def Close(self):
    if self.keyboard_fd is not None:
      self.SafeClose(self.keyboard_fd)
      self.keyboard_fd = None
    self.keyboard_buf = None
    if self.mouse_fd is not None:
      self.SafeClose(self.mouse_fd)
      self.mouse_fd = None
    self.mouse_buf = None
    self.has_shift = False
    self.has_mod42 = False
    self.do_ignore_release_digit5 = False
    # TODO(pts): Document the keycode sequences sent by the green start key
    # (and other keys).
    self.do_release_start = False
    self.do_release_screenrow = False
    self.pending_mouse_x[0] = 0
    self.pending_mouse_y[0] = 0

  def IsAlive(self):
    if self.keyboard_fd is None:
      if self.mouse_fd is not None:
        self.Close()
      return False
    elif self.mouse_fd is None:
      return False
    if self.keyboard_buf or self.mouse_buf:
      return True
    readable_fds = select.select(
        [self.keyboard_fd, self.mouse_fd], (), (), 0)[0]
    if self.keyboard_fd in readable_fds:
      try:
        self.keyboard_buf = os.read(self.keyboard_fd, INPUT_EVENT_SIZE) or ''
      except OSError:
        self.keyboard_buf = ''
      if len(self.keyboard_buf) != INPUT_EVENT_SIZE:
        self.Close()
        return False
    if self.mouse_fd in readable_fds:
      try:
        self.mouse_buf = os.read(self.mouse_fd, INPUT_EVENT_SIZE) or ''
      except OSError:
        self.mouse_buf = ''
      if len(self.mouse_buf) != INPUT_EVENT_SIZE:
        self.Close()
        return False
    return True

  @classmethod
  def SafeClose(cls, fd):
    if fd is not None:
      try:
        os.close(fd)
      except OSError, e:
        if e.errno != errno.ENODEV:
          raise

  def Open(self, keyboard_num=None, mouse_num=None):
    self.Close()
    if keyboard_num is None:
      assert mouse_num is None
      # TODO(pts): Handle OSError intelligently here.
      keyboard_num, mouse_num = DetectHamaMce()
      if keyboard_num is None or mouse_num is None:
        raise HamaMceNotFound
    try:
      keyboard_fd, mouse_fd = self.AttemptOpen(keyboard_num, mouse_num)
    except OSError, e:
      # `Permission denied' or `No such file or directory'.
      if e.errno not in (errno.EACCES, errno.ENOENT):
        raise
      print >>sys.stderr, 'info: Hame MCE found, creating devices'
      # TODO(pts): Make this return instantly if sudoers not set up.
      os.system(
          'sudo -S /usr/local/sbin/create_hama_mce_devices.py </dev/null')
      keyboard_fd, mouse_fd = self.AttemptOpen(keyboard_num, mouse_num)
    print >>sys.stderr, 'info: opened /dev/input/event{%d,%d}' % (
        keyboard_num, mouse_num)

    try:
      # EVIOCGRAB gives the filehandle exclusive access to the device events
      # until the filehandle is closed. It also prevents keyboard and mouse
      # events from contributing to the main system console.
      # EBUSY here means that another filehandle already has EVIOCGRAB.
      fcntl.ioctl(keyboard_fd, self.EVIOCGRAB, -1)
      fcntl.ioctl(mouse_fd, self.EVIOCGRAB, -1)
    except:
      self.SafeClose(keyboard_fd)
      self.SafeClose(mouse_fd)
      raise

    self.keyboard_fd = keyboard_fd
    self.mouse_fd = mouse_fd
    print >>sys.stderr, 'info: open OK'

  @classmethod
  def AttemptOpen(cls, keyboard_num, mouse_num):
    """Open both file descriptors or raise OSError."""
    keyboard_fd = os.open('/dev/input/event%d' % keyboard_num, os.O_RDONLY)
    mouse_fd = None
    try:
      mouse_fd = os.open('/dev/input/event%d' % mouse_num, os.O_RDONLY)
    except:
      if mouse_fd is None:
        self.SafeClose(keyboard_fd)
      raise
    return keyboard_fd, mouse_fd

  def ReadEvent(self, timeout=None):
    """Wait for an event and return it.
    
    Returns:
      A human-readable, single-word event description string, or False on
      timeout or None on error.
    """
    while True:
      if self.keyboard_fd is None or self.mouse_fd is None:
        return None  # unrecoverable error
      elif self.keyboard_buf is not None:
        keyboard_data = self.keyboard_buf
        self.keyboard_buf = None
        event = self.FilterKeyboardEvent(keyboard_data)
        if event:
          return event
      elif self.mouse_buf is not None:
        mouse_data = self.mouse_buf
        self.mouse_buf = None
        event = self.FilterMouseEvent(mouse_data)
        if event:
          return event
      else:
        # Upon USB disconnect, only self.keyboard_fd becomes readable.
        # Exceptional condition doesn't occur.
        readable_fds = select.select(
            [self.keyboard_fd, self.mouse_fd], (), (), timeout)[0]
        if not readable_fds:
          return False  # timed out
        if self.keyboard_fd in readable_fds:
          do_print_disconnect = False
          try:
            self.keyboard_buf = (os.read(self.keyboard_fd, INPUT_EVENT_SIZE)
                                 or '')
            do_print_disconnect = True
          except OSError, e:
            self.keyboard_buf = ''
            print >>sys.stderr, 'error: read error from keyboard: %s' % e
          if len(self.keyboard_buf) != INPUT_EVENT_SIZE:
            if not self.keyboard_buf:
              print >>sys.stderr, 'info: Hame MCE disconnected'
            self.Close()
            return None
        if self.mouse_fd in readable_fds:
          do_print_disconnect = False
          try:
            self.mouse_buf = os.read(self.mouse_fd, INPUT_EVENT_SIZE) or ''
            do_print_disconnect = True
          except OSError, e:
            self.mouse_buf = ''
            print >>sys.stderr, 'error: read error from mouse: %s' % e
          if len(self.mouse_buf) != INPUT_EVENT_SIZE:
            if not self.keyboard_buf:
              print >>sys.stderr, 'info: Hame MCE disconnected'
            self.Close()
            return None

  @classmethod
  def ParseEvent(cls, buf):
    if len(buf) == 16:
      tv_sec, tv_usec, etype, code, value = struct.unpack('=LLHHl', buf)
    elif len(buf) == 24:
      # This assumes sizeof(long) == 8, which is true if
      # len(buf) == INPUT_EVENT_SIZE.
      tv_sec, tv_usec = struct.unpack('@LL', buf[:16])
      etype, code, value = struct.unpack('=HHl', buf[16:])
    return tv_sec, tv_usec, etype, code, value

  def FilterKeyboardEvent(self, buf):
    """Return an event string or None to ignore."""
    tv_sec, tv_usec, etype, code, value = self.ParseEvent(buf)
    #print ('K', etype, code, value)
    # Some keypresses generate more events with etype=4, code=4 and
    # value containing the keycode, without information about the press or
    # release. But we can ignore those.
    if etype == 1 and 0 <= value <= 2:
      code = self.KEYBOARD_CODES.get(code, code)
      if code == 'digit3' and self.has_shift:
        code = 'hashmark'
      elif code == 'enterok' and self.has_shift:  # .mod125
        code = 'start'
        self.do_release_start = True
      elif code == 'enterok' and self.do_release_start:
        code = 'start'
        self.do_release_start = False
      elif code == 'screenhill' and self.has_mod42:
        code = 'yellow'
        self.do_release_screenrow = True
      elif code == 'screenhill' and self.do_release_screenrow:
        code = 'yellow'
        self.do_release_screenrow = False
      elif code == 'screenufo' and value == 1 and not self.has_mod42:
        code = 'blue'
        self.do_release_screenrow = True
      elif code == 'screenufo' and value == 0 and self.do_release_screenrow:
        code = 'blue'
        self.do_release_screenrow = False

      if code == '.numlock':  # Num Lock emulation for the keypad => ignore
        return None
      elif code == '.mod42':
        if value == 0:
          self.has_mod42 = False
        elif value == 1:
          self.has_mod42 = True
      elif code == '.shift' or code == '.mod125' or code == '.mod29':
        if value == 0:
          self.has_shift = False
        elif value == 1:
          self.has_shift = True
      elif code == 'digit5' and self.has_shift:
        # Extra echo digit after release-hashmark
        if value == 1:
          self.do_ignore_release_digit5 = True
      elif code == 'digit5' and self.do_ignore_release_digit5:
        self.do_ignore_release_digit5 = False
      elif value == 0:
        # The Hama MCE sends release just after press for most of the keys.
        # So there is no way to know when the release actually occured.
        return 'release-%s' % code
      elif value == 1:
        return 'press-%s' % code
      elif value == 2:
        return 'repeat-%s' % code
      else:
        return None
    elif etype == 0 and code == 0:
      pass
    elif etype == 4 and code == 4:
      # Identical extra events for each key press and release.
      pass
    else:
      return 'keyboard-other-%d-%d-0x%08x' % (etype, code, value)

  @classmethod
  def ShouldIgnoreMouseEvent(cls, pending, other_pending, now, value):
    if abs(value) > 1:
      pending[4] = pending[3]
      pending[3] = now
    else:
      pending[3] = pending[4] = 0.0
    # The Hama MCE sends the same mouse movement event 4 times, with slightly
    # increasing timestamps. Here we ignore all but the first instance.
    if (pending[0] in (1, 2, 3) and pending[1] == value and
        now - pending[2] <= 2.0):
      pending[0] += 1
      return True
    pending[0] = 1
    pending[1] = value
    pending[2] = now

    # The Hama MCE is not so reliable on reporting diagonal movements, e.g.
    # when moving the mouse down, it sometimes reports a little left or
    # right movement as well. So we just ignore diagonal movements here,
    # whose minor direction always has half of the absolute value as the
    # main direction.

    if (value in (1, -1) or
        (now - other_pending[3] <= 0.5 and
         abs(value) < abs(other_pending[1])) or
        (value in (2, -2) and
         other_pending[1] in (2, -2) and
         now - other_pending[4] <= 0.5 and
         now - pending[4] >= 1.0)):
      return True

    return False

  def FilterMouseEvent(self, buf):
    """Return an event string or None to ignore."""
    tv_sec, tv_usec, etype, code, value = self.ParseEvent(buf)
    # Some keypresses generate more events with etype=4, code=4 and
    # value containing the keycode, without information about the press or
    # release. But we can ignore those.
    if etype == 2 and code == 0 and -15 <= value <= 15:
      if self.ShouldIgnoreMouseEvent(pending=self.pending_mouse_x,
                                     other_pending=self.pending_mouse_y,
                                     now=tv_sec + tv_usec / 1000000.0,
                                     value=value):
        return None
      # `value' starts from 2 (occasionally 1 for diagonal movements), and
      # increases (4, 6, 8, 10, # 12, 14) as the button is kept pressed.
      if value > 0:
        # !! events seem to be repeated 4 times -- keep fewer
        return 'mouse-right-%d' % value
      elif value < 0:
        return 'mouse-left-%d' % -value
      else:
        return None
    elif etype == 2 and code == 1 and -15 <= value <= 15:
      if self.ShouldIgnoreMouseEvent(pending=self.pending_mouse_y,
                                     other_pending=self.pending_mouse_x,
                                     now=tv_sec + tv_usec / 1000000.0,
                                     value=value):
        return None
      if value > 0:
        return 'mouse-down-%d' % value
      elif value < 0:
        return 'mouse-up-%d' % -value
      else:
        return None
    elif etype == 1:
      code = self.MOUSE_CODES.get(code, code)
      if isinstance(code, int):
        code = 'mouse-%d' % code
      if value == 0:
        # The Hama MCE sends release just after press for most of the keys.
        # So there is no way to know when the release actually occured.
        return 'release-%s' % code
      elif value == 1:
        return 'press-%s' % code
      elif value == 2:
        # This should never occur.
        return 'repeat-%s' % code
    elif etype == 0 and code == 0:
      pass
    elif etype == 4 and code == 4:
      pass
    else:
      return 'mouse-other-%d-%d-0x%08x' % (etype, code, value)

class LineSource(object):
  """A source (for source_fds) which reads line from a file descriptor"""
  def __init__(self, fd, prefix):
    if not isinstance(fd, int):
      raise TypeError
    if fd < 0:
      raise TypeError
    self.fd = fd
    if not isinstance(prefix, str):
      raise TypeError
    self.prefix = prefix

  def close(self):
    """Closes the filehandle, as called by Close()."""
    if self.fd is not None and self.fd > 2:
      os.close(self.fd)
    self.fd = None

  def ReadMessage(self):
    """Return a message or None if not available or '' on EOF."""
    # TODO(pts): Do buffering, do non-blocking reads and return None.
    # TODO(pts): Don't use string += for speed.
    line = ''
    while True:
      c = os.read(self.fd, 1)
      if not c:
        if line:
          line += '\n'
          return prefix + line
        else:
          return ''
      else:
        line += c
        if c == '\n':
          break
    return self.prefix + line


def RunBroadcastServer(socket_filename, orig_sources, detect_sources_callback):
  """Infinite loop to run a socket server which broadcasts messages."""

  server_fds = []
  """accept()able socket file descriptors we are listening on."""
  source_fds = []
  """File desciptors whose lines we want to propagate."""
  fd_to_socket = {}

  def SetupServerSocket(socket_filename):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:  # prevent EADDRINUSE
      if stat.S_ISSOCK(os.stat(socket_filename).st_mode):
        os.remove(socket_filename)
    except OSError:
      pass
    old_umask = os.umask(0)
    os.umask(0111)
    print >>sys.stderr, 'info: listening on: %s' % socket_filename
    try:
      s.bind(socket_filename)
    except socket.error, e:
      if e.errno in (errno.EACCES, errno.EADDRINUSE) and os.geteuid() != 0:
        extra = ' -- try running as root for the file permissions'
      elif e.errno == errno.EACCES:
        extra = ' -- try to fix the file permissions'
      else:
        extra = ''
      print >>sys.stderr, (
          'error: could not bind Unix domain server socket to %s: %s%s' %
          (socket_filename, e, extra))
      # TODO(pts): Raise better exception here.
      raise SystemExit(1)
    os.umask(old_umask)
    s.listen(3)
    server_fds.append(s.fileno())
    fd_to_socket[s.fileno()] = s


  def Close(fd):
    assert fd in fd_to_socket
    try:
      s = fd_to_socket.pop(fd)
      if s is not None:
        s.close()
      elif fd > 2:
        os.close(fd)
    except (OSError, socket.error):
      pass
    while fd in server_fds:
      server_fds.remove(fd)
    while fd in source_fds:
      source_fds.remove(fd)
    if fd in sources:
      del sources[fd]

  def Broadcast(msg):
    """Broadcast string msg to all writable non-server non-source fds."""
    if not isinstance(msg, str):
      raise TypeError
    if not msg:
      return
    fds = set(fd_to_socket)
    fds.difference_update(server_fds)  # Doesn't fail if missing from fds.
    fds.difference_update(source_fds)
    print >>sys.stderr, 'info: [%d] broadcasting to %d: %r' % (
        int(time.time()), len(fds), msg)
    writable_fds = select.select((), fds, (), 0)[1]
    non_writable_fds = sorted(fds.difference(writable_fds))
    if non_writable_fds:
      print >>sys.stderr, (
          'info: skipping broadcast to busy fds %r' % non_writable_fds)
    for fd in writable_fds:
      # TODO(pts): Use non-blocking I/O to verify if it's still wriable.
      i = 0
      while i < len(msg):
        try:
          n = os.write(fd, msg[i:])
        except OSError, e:
          if e.errno != errno.EPIPE:
            print >>sys.stderr, 'info: %s writing to fd=%d, closing it' % (e, fd)
            # Example: errno.ECONNRESET
            Close(fd)
            raise
          print >>sys.stderr, 'info: broken pipe to fd=%d' % fd
          break
        assert n
        i += n
        # !! TODO(pts): Buffer the rest of the msg and return now non-blocking.
        # Now we'll be doing a blocking write().

  def AddSource(fd, source):
    assert isinstance(fd, int)
    assert fd >= 0
    assert fd not in fd_to_socket
    assert fd not in sources
    assert fd not in source_fds
    assert fd not in server_fds
    assert source
    assert hasattr(source, 'close')
    assert hasattr(source, 'ReadMessage')
    sources[fd] = source
    source_fds.append(fd)
    fd_to_socket[fd] = source

  def DeleteSource(fd):
    assert isinstance(fd, int)
    assert fd >= 0
    assert fd not in server_fds
    if fd in fd_to_socket:
      Close(fd)
    else:
      while fd in server_fds:
        server_fds.remove(fd)
      while fd in source_fds:
        source_fds.remove(fd)
      if fd in sources:
        del sources[fd]

  SetupServerSocket(socket_filename=socket_filename)

  sources = {}
  for fd in sorted(orig_sources):
    AddSource(fd=fd, source=orig_sources[fd])
  orig_sources = None  # break reference

  # Timestamp of the next call to detect_sources_callback(sources)
  next_detect = time.time()

  while True:
    if next_detect is not None and next_detect <= time.time():
      next_detect = None
      actions = detect_sources_callback(sources)
      for action in actions:
        if action[0] == 'add':
          AddSource(fd=action[1], source=action[2])
        elif action[0] == 'del':
          DeleteSource(fd=action[1])
        elif action[0] == 'next':
          assert isinstance(action[1], float)
          if next_detect is None or next_detect < action[1]:
            next_detect = action[1]
        else:
          assert 0, 'unknown action: %r' % (action,)
    now = time.time()
    if next_detect is None:
      timeout = None
    else:
      timeout = next_detect - now
      if timeout < 0:
        timeout = 0
      print >>sys.stderr, 'info: next detect in %.2f seconds' % timeout
    # TODO(pts): shrink fd_to_socket, otherwise we might get:
    # select.error: (9, 'Bad file descriptor')
    readable_fds = select.select(fd_to_socket, (), (), timeout)[0]
    for fd in readable_fds:
      s = fd_to_socket[fd]
      if fd in server_fds:
        conn, unused_addr = s.accept()
        fd_to_socket[conn.fileno()] = conn
        print >>sys.stderr, 'info: client connected on fd=%s' % conn.fileno()
        conn = None
      elif fd in source_fds:
        msg = s.ReadMessage()
        if msg is None:
          pass
        elif not msg:
          print >>sys.stderr, 'info: source disconnected fd=%s' % fd
          DeleteSource(fd)
          next_detect = now  # force early detect
        else:
          Broadcast(msg)
      else:
        # TODO(pts): Activate non-blocking read.
        data = os.read(fd, 4096)
        if data:
          print >>sys.stderr, 'info: %d bytes ignored on fd=%s' % (len(data), fd)
        else:
          print >>sys.stderr, 'info: client disconnected on fd=%s' % fd
          Close(fd)

PREFIX = '0000000000000000 00 '
LIRC_REMOTE_NAME = 'hama-mce'
"""This remote name is broadcast to the lirc clients."""

class HamaMceSource(object):
  def __init__(self, hmi, prefix, lirc_remote_name):
    if not isinstance(hmi, HamaMceInput):
      raise TypeError
    if not isinstance(prefix, str):
      raise TypeError 
    if not isinstance(lirc_remote_name, str):
      raise TypeError 
    self.hmi = hmi
    self.prefix = prefix
    if lirc_remote_name:
      self.suffix = ' %s\n' % lirc_remote_name.strip()
    else:
      self.suffix = '\n'

  def close(self):
    self.hmi.Close()

  def ReadMessage(self):
    event = self.hmi.ReadEvent(timeout=0)
    if event is None:  # not alive anymore
      return ''
    elif event is False:  # timed out
      return None
    else:
      assert isinstance(event, str)
      assert event
      if event.startswith('mouse-'):
        pass
      elif event.startswith('press-'):
        event = event[6:]
      elif event.startswith('repeat-'):
        # TODO(pts): Emit the repeat count instead of 00 in the PREFIX
        event = event[7:]
      else:
        return None  # release etc.
      return '%s%s%s' % (self.prefix, event, self.suffix)


hmi = HamaMceInput()
hmi_source = HamaMceSource(
    hmi, prefix=PREFIX, lirc_remote_name=LIRC_REMOTE_NAME)

def DetectSources(sources):
  had_fds = hmi.keyboard_fd is not None and hmi.mouse_fds is not None
  was_alive = hmi.IsAlive()
  if hmi.keyboard_fd in sources and hmi.mouse_fd in sources and was_alive:
    return ()
  actions = []
  # We iterate for 'del', because hmi.keyboard_fd might already be None now.
  for fd in sources:
    if sources[fd] == hmi_source:
      actions.append(('del', fd))
  if had_fds:
    print >>sys.stderr, (
        'error: [%d] Hama MCE dongle disconnected, please reconnect it' %
        int(time.time()))
  try:
    hmi.Open()
  except HamaMceNotFound:
    pass
  if hmi.IsAlive():
    print >>sys.stderr, 'info: [%d] Hama MCE dongle detected' % int(time.time())
    actions.append(('add', hmi.keyboard_fd, hmi_source))
    actions.append(('add', hmi.mouse_fd, hmi_source))
  else:
    if not had_fds:
      print >>sys.stderr, (
          'error: [%d] Hama MCE dongle not detected, please connect it' %
          int(time.time()))
    hmi.Close()
    actions.append(('next', time.time() + 3))
  return actions


def main(argv):
  print >>sys.stderr, 'info: This is the Hama MCE reader.'
  if not sys.platform.lower().startswith('linux'):
    print >>sys.stderr, 'error: a Linux system is needed'   
    # TODO(pts): Check for kernel 2.6 or newer, for /sys (sysfs)
  try:
    if len(sys.argv) < 2:
      while True:  # Inifinite loop.
        if not hmi.IsAlive():
          while True:
            try:
              hmi.Open()
            except HamaMceNotFound:
              pass
            if hmi.IsAlive():
              break
            sleep_secs = 3
            print >>sys.stderr, (
                'info: [%d] Hama MCE dongle not found, please connect it '
                '(sleep %d)' % (int(time.time()), sleep_secs))
            time.sleep(sleep_secs)
        print >>sys.stderr, 'info: use the remote or press Ctrl-<C> to exit'
        while True:
          event = hmi.ReadEvent()
          if event is None:
            break  # Unrecoverable error, closed.
          print '%s %s' % (int(time.time()), event)
    else:
      print >>sys.stderr, 'info: you may type synthetic lircd events on stdin'
      RunBroadcastServer(
          socket_filename=argv[1],  # '/dev/lircd',
          orig_sources={0: LineSource(0, prefix=PREFIX)},  # sys.stdin
          detect_sources_callback=DetectSources)
  except KeyboardInterrupt:
    # TODO(pts): Defer this until safe to handle.
    print >>sys.stderr, '\ninfo: got Ctrl-<C>, aborted'

if __name__ == '__main__':
  sys.exit(main(sys.argv) or 0)
