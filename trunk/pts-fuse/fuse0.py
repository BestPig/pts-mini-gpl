#! /usr/bin/python2.4 --
# by pts@fazekas.hu at Sat Nov 14 10:50:42 CET 2009

import errno
import os
import re
import socket
import stat
import struct
import sys

# !! Have a look at passfd.c in Neil Schemenauer's SCGI protocol implementation:
#http://www.mems-exchange.org/software/scgi/
#It wraps sendmsg/recvmsg to send and receive file descriptors.
#
#It's a C module, but's it's very lightweight. I think it does what you
#want to do (the test_passfd.py is almost exactly like the script you
#posted; showing their common ancestors...) It's supposed to work under
#Linux, FreeBSD and Solaris.
try:
  import _receive_fd
except ImportError, e:
  if str(e) == 'No module named _receive_fdd':
    print >>sys.stderr, (
        'error: please compile receive_fd.c to an .so file first')
    sys.exit(2)
  else:
    raise

"""A FUSE filesystem library in Python not using libfuse.

FUSE (/dev/fuse) protocol documentation:

* http://research.cs.queensu.ca/~wolfe/misc/rtbfs/fuse-7.8.xml
* http://sourceforge.net/apps/mediawiki/fuse/index.php?title=FuseProtocolSketch
"""

O_ACCMODE = os.O_RDONLY | os.O_WRONLY | os.O_RDWR

class InvalidConfigError(Exception):
  """Raised when a Config contains invalid attributes."""

class Config(object):
  __slots__ = [
      'mount_prog', 'mount_opts', 'mount_point',
      # Number of seconds (can be float) the kernel may cache inodes (FuseAttr)
      # returned by FUSE_GETATTR (and FUSE_LOOKUP). That is, if the user does
      # an fstat(2) and then an fstat(2) again within attr_timeout, the kernel
      # will not issue a FUSE_GETATTR, but it will return the value from its
      # cache.
      #
      # It's OK to specify 0 to disable caching in the kernel.
      'attr_timeout',
      # Number of seconds (can be float) the kernel may cache directory
      # entries (dentries) returned by FUSE_LOOKUP. That is, if the user does
      # a stat(2) and then a stat(2) again within entry_timeout, the kernel
      # will not ussue a FUSE_LOOKUP, but it will return the value from its
      # cache.
      #
      # It's OK to specify 0 to disable caching in the kernel.
      'entry_timeout',
      # Number of seconds (can be float) the kernel may cache missing
      # directory entries (dentries) if FUSE_LOOKUP doesn't find the
      # requested entry.
      #
      # It's OK to specify 0 to disable caching in the kernel.
      'negative_timeout']
  
  def __init__(self):
    self.mount_prog = 'fusermount'
    self.mount_opts = ''
    self.mount_point = None  # Has to be overridden.
    
    self.attr_timeout = 1.0
    self.entry_timeout = 0.0
    self.negative_timeout = 0.0

  def Validate(self):
    if not isinstance(self.mount_prog, str):
      raise InvalidConfigError
    if not self.mount_prog:
      raise InvalidConfigError
    if '\0' in self.mount_prog:
      raise InvalidConfigError
    if not isinstance(self.mount_opts, str):
      raise InvalidConfigError
    if '\0' in self.mount_opts:
      raise InvalidConfigError
    if not isinstance(self.mount_point, str):
      raise InvalidConfigError
    if not self.mount_point:
      raise InvalidConfigError
    if '\0' in self.mount_point:
      raise InvalidConfigError
    if (not isinstance(self.attr_timeout, int) and
        not isinstance(self.attr_timeout, long) and
        not isinstance(self.attr_timeout, float)):
      raise InvalidConfigError
    if self.attr_timeout < 0:
      raise InvalidConfigError
    if (not isinstance(self.entry_timeout, int) and
        not isinstance(self.entry_timeout, long) and
        not isinstance(self.entry_timeout, float)):
      raise InvalidConfigError
    if self.entry_timeout < 0:
      raise InvalidConfigError
    if (not isinstance(self.negative_timeout, int) and
        not isinstance(self.negative_timeout, long) and
        not isinstance(self.negative_timeout, float)):
      raise InvalidConfigError
    if self.negative_timeout < 0:
      raise InvalidConfigError


def QuoteShellWord(data):
  if re.match(r'[-_/+.A-Za-z0-9][-_/+=.A-Za-z0-9]*\Z', data):
    return data
  return "'%s'" % data.replace("'", "'\\''")


def FuseUnmount(mount_prog, mount_point):
  """Unmount a FUSE filesystem."""
  if not isinstance(mount_point, str):
    raise TypeError
  if "'" in mount_point:
    raise ValueError

  assert not os.system(
      '%s -u \'%s\'' % (mount_prog, QuoteShellWord(mount_point))), (
      'unmount with fusermount failed (see above)')


def FuseMount(mount_prog, mount_point, mount_opts):
  """Mount a FUSE filesystem to directory mount_point.

  Returns:
    Integer file descriptor to be used in a FUSE server (userspace filesystem
    implementation) to communicate with the FUSE client (Linux kernel).
  Raises:
    TypeError:
    ValueError:
    socket.error:
    OSError:
  """
  if not isinstance(mount_point, str):
    raise TypeError
  if "'" in mount_point:
    raise ValueError

  try:
    os.stat(mount_point)
  except OSError, e:
    if e.errno == errno.ENOTCONN:
      # Stale mount, FUSE server is dead.
      FuseUnmount(mount_prog, mount_point)

  if mount_opts:
    opt_str = ' -o ' + QuoteShellWord(mount_opts)
  else:
    opt_str = ''

  # Use fusermount(1) to mount mount_point and open /dev/fuse for us (as root).
  fd0, fd1 = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM, 0)
  try:
    # This fails when already mounted (even if not connected):
    # fusermount: failed to access mountpoint ...: Permission denied
    assert not os.system('export _FUSE_COMMFD=%s; %s%s %s' %
        (fd0.fileno(), mount_prog, opt_str, QuoteShellWord(mount_point))), (
        'fusermount failed (see above)')
    return _receive_fd.receive_fd(fd1.fileno())
  finally:
    fd0.close()
    fd1.close()

# --- From fuse-2.5.3/kernel/fuse_kernel.h

# Version number of this interface
# TODO(pts): Upgrade this to newer fuse kernels.
FUSE_KERNEL_VERSION = 7

# Minor version number of this interface
FUSE_KERNEL_MINOR_VERSION = 5

# The node ID of the root inode
FUSE_ROOT_ID = 1

class FuseAttr(object):
  """Derived from struct fuse_attr."""
  __slots__ = ['ino', 'size', 'blocks', 'atime', 'mtime', 'ctime',
               'atimensec', 'mtimensec', 'ctimensec', 'mode', 'nlink', 'uid',
               'gid', 'rdev']

  def __init__(self, data=None, ino=None, size=None):
    if data is None or isinstance(data, int):
      self.blocks = self.atime = self.mtime = 0
      self.ctime = self.atimensec = self.mtimensec = self.ctimensec = 0
      self.uid = self.gid = self.rdev = 0
      if (isinstance(ino, int) or isinstance(ino, long)) and ino > 0:
        self.ino = int(ino)
      elif ino is None:
        self.ino = 1
      else:
        raise TypeError
      if (isinstance(size, int) or isinstance(size, long)) and size > 0:
        self.size = int(size)
      elif size is None:
        self.size = 0
      else:
        raise TypeError
      self.nlink = 1  # Safe to disable stat() optimization in find(1).
      if isinstance(data, int):
        self.mode = data
      elif data is None:
        self.mode = stat.S_IFDIR | 0755
      else:
        raise TypeError
      return
    if not isinstance(data, str):
      raise TypeError
    if len(str) < 80:
      raise ValueError
    (self.ino, self.size, self.blocks, self.atime, self.mtime, self.ctime,
     self.atimensec, self.mtimensec, self.ctimensec, self.mode, self.nlink,
     self.uid, self.gid, self.rdev) = struct.unpack(
        '=QQQQQQLLLLLLLL', data[:80])

  def Format(self):
    return struct.pack(
        '=QQQQQQLLLLLLLL', self.ino, self.size, self.blocks, self.atime,
        self.mtime, self.ctime, self.atimensec, self.mtimensec, self.ctimensec,
        self.mode, self.nlink, self.uid, self.gid, self.rdev)


def FormatInitOut(major, minor, max_readahead, flags, max_write, unused=0):
  """Derived from struct fuse_init_out."""
  return struct.pack('=LLLLLL',
                     major, minor, max_readahead, flags, unused, max_write)


def FormatAttrOut(fuse_attr, attr_valid=3, attr_valid_ns=0, unused=0):
  """Derived from struct fuse_attr_out."""
  if not attr_valid and not attr_valid_ns:
    # Hack otherwise the kernel would forget the inode too soon, and
    # stat(2) would immediately fail with errno.ENOENT
    # (No such file or directory).
    # TODO(pts): Is 1 high enough and stable here?
    attr_valid_ns = 1
  return struct.pack('=QLL', attr_valid, attr_valid_ns, unused) + (
      fuse_attr.Format())

def FormatEntryOut(fuse_attr, nodeid, generation, attr_valid=0,
                   attr_valid_ns=0, entry_valid=0, entry_valid_ns=0):
  """Derived from struct fuse_entry_out."""
  if not attr_valid and not attr_valid_ns:
    # Hack otherwise the kernel would forget the inode too soon, and
    # stat(2) would immediately fail with errno.ENOENT
    # (No such file or directory).
    # TODO(pts): Is 1 high enough and stable here?
    attr_valid_ns = 1
  return struct.pack('=QQQQLL', nodeid, generation, entry_valid, attr_valid,
                     entry_valid_ns, attr_valid_ns) + fuse_attr.Format()

def FormatOpenOut(fh, fopen_flags, unused=0):
  """Derived from struct fuse_open_out."""
  return struct.pack('=QLL', fh, fopen_flags, unused)


class Dirent(object):
  """A directory entry, derived from struct fuse_dirent."""
  __slots__ = ['name', 'ino', 'type']

  def __init__(self, name, ino=1, type_num=0):
    if not isinstance(name, str):
      raise TypeError
    if '/' in name or '\0' in name:
      raise ValueError
    if not isinstance(ino, int) or ino <= 0:
      raise TypeError
    if not isinstance(type_num, int) or type_num < 0:
      raise TypeError
    self.name = name
    self.ino = ino
    self.type = type_num

  def GetFormatSize(self):
    return 24 + ((len(name) + 7) & ~7)

  def Format(self, base_off):
    """Derived from struct fuse_dirent."""
    off = base_off + 24 + ((len(self.name) + 7) & ~7)
    padding = '\0' * ((8 - len(self.name) & 7) & 7)
    return '%s%s%s' % (
        struct.pack('=QQLL', self.ino, off, len(self.name), self.type),
        self.name, padding)


def FormatDirents(dirents):
  """Formats multiple dirents with proper offsets."""
  dirents_formatted = []
  base_off = 0
  for dirent in dirents:
    dirents_formatted.append(dirent.Format(base_off=base_off))
    base_off += len(dirents_formatted[-1])
  return ''.join(dirents_formatted)


#struct fuse_kstatfs {
#  __u64 blocks, bfree, bavail, files, ffree;
#  __u32 bsize, namelen, frsize, padding, spare[6];
#};

FATTR_MODE	= (1 << 0)
FATTR_UID	= (1 << 1)
FATTR_GID	= (1 << 2)
FATTR_SIZE	= (1 << 3)
FATTR_ATIME	= (1 << 4)
FATTR_MTIME	= (1 << 5)
FATTR_FH	= (1 << 6)

# Flags returned by the OPEN request (struct fuse_open_out)
# FOPEN_DIRECT_IO: bypass page cache for this open file
# FOPEN_KEEP_CACHE: don't invalidate the data cache on open
FOPEN_DIRECT_IO  = (1 << 0)
FOPEN_KEEP_CACHE = (1 << 1)

# enum fuse_opcode {
FUSE_LOOKUP	   = 1
FUSE_FORGET	   = 2  # no reply
FUSE_GETATTR	   = 3
FUSE_SETATTR	   = 4
FUSE_READLINK	   = 5
FUSE_SYMLINK	   = 6
FUSE_MKNOD	   = 8
FUSE_MKDIR	   = 9
FUSE_UNLINK	   = 10
FUSE_RMDIR	   = 11
FUSE_RENAME	   = 12
FUSE_LINK	   = 13
FUSE_OPEN	   = 14
FUSE_READ	   = 15
FUSE_WRITE	   = 16
FUSE_STATFS	   = 17
FUSE_RELEASE       = 18
FUSE_FSYNC         = 20
FUSE_SETXATTR      = 21
FUSE_GETXATTR      = 22
FUSE_LISTXATTR     = 23
FUSE_REMOVEXATTR   = 24
FUSE_FLUSH         = 25
FUSE_INIT          = 26
FUSE_OPENDIR       = 27
FUSE_READDIR       = 28
FUSE_RELEASEDIR    = 29
FUSE_FSYNCDIR      = 30
FUSE_ACCESS        = 34
FUSE_CREATE        = 35
# Above 5.7:
FUSE_INTERRUPT     = 36
FUSE_BMAP          = 37
FUSE_DESTROY       = 38  # no reply

# The read buffer is required to be at least 8k, but may be much larger.
FUSE_MIN_READ_BUFFER = 8192

#struct fuse_entry_out {
#	__u64	nodeid;		/* Inode ID */
#	__u64	generation;	/* Inode generation: nodeid:gen must
#				   be unique for the fs's lifetime */
#	__u64	entry_valid;	/* Cache timeout for the name */
#	__u64	attr_valid;	/* Cache timeout for the attributes */
#	__u32	entry_valid_nsec;
#	__u32	attr_valid_nsec;
#	struct fuse_attr attr;
#};

#struct fuse_mknod_in {
#	__u32	mode;
#	__u32	rdev;
#};

#struct fuse_mkdir_in {
#	__u32	mode;
#	__u32	padding;
#};

#struct fuse_rename_in {
#	__u64	newdir;
#};

#struct fuse_link_in {
#	__u64	oldnodeid;
#};

#struct fuse_setattr_in {
#	__u32	valid;
#	__u32	padding;
#	__u64	fh;
#	__u64	size;
#	__u64	unused1;
#	__u64	atime;
#	__u64	mtime;
#	__u64	unused2;
#	__u32	atimensec;
#	__u32	mtimensec;
#	__u32	unused3;
#	__u32	mode;
#	__u32	unused4;
#	__u32	uid;
#	__u32	gid;
#	__u32	unused5;
#};

#struct fuse_flush_in {
#	__u64	fh;
#	__u32	flush_flags;
#	__u32	padding;
#};


#struct fuse_write_in {
#	__u64	fh;
#	__u64	offset;
#	__u32	size;
#	__u32	write_flags;
#};

#struct fuse_write_out {
#	__u32	size;
#	__u32	padding;
#};

FUSE_COMPAT_STATFS_SIZE = 48

#struct fuse_statfs_out {
#	struct fuse_kstatfs st;
#};

#struct fuse_fsync_in {
#	__u64	fh;
#	__u32	fsync_flags;
#	__u32	padding;
#};

#struct fuse_setxattr_in {
#	__u32	size;
#	__u32	flags;
#};

#struct fuse_getxattr_in {
#	__u32	size;
#	__u32	padding;
#};

#struct fuse_getxattr_out {
#	__u32	size;
#	__u32	padding;
#};

#struct fuse_access_in {
#	__u32	mask;
#	__u32	padding;
#};

#struct fuse_out_header {
#	__u32	len;
#	__s32	error;
#	__u64	unique;
#};


##define FUSE_NAME_OFFSET ((unsigned) ((struct fuse_dirent *) 0)->name)
##define FUSE_DIRENT_ALIGN(x) (((x) + sizeof(__u64) - 1) & ~(sizeof(__u64) - 1))
##define FUSE_DIRENT_SIZE(d) FUSE_DIRENT_ALIGN(FUSE_NAME_OFFSET + (d)->namelen)

# ---

class Server(object):
  """Corresponds to struct fuse_in_header."""
  __slots__ = ['opcode', 'unique', 'nodeid', 'uid', 'gid', 'pid',
               'init_major', 'init_minor', 'init_readahead', 'init_flags',
               # The minor version of the FUSE protocol to use. The major
               # vrsion is always FUSE_KERNEL_VERSION.
               'use_minor',
               # O_RDONLY etc.
               'open_flags',
               'open_mode',
               'read_fh', 'read_offset', 'read_size',
               'release_fh', 'release_flags',
               'lookup_name',
               'forget_nlookup',
               # File descriptor for communication with the FUSE client
               # (usually the Linux kernel).
               'fd',
               # Generation number for nodeids.
               # nodeid:generation must be unique for the fs's lifetime
               'generation',
               # Integer representation of Config.attr_timeout.
               'attr_valid', 'attr_valid_ns',
               # Integer representation of Config.entry_timeout.
               'entry_valid', 'entry_valid_ns',
               # Integer representation of Config.negative_timeout.
               'negative_valid', 'negative_valid_ns',
               'dirh_to_dirent_data',
              ]

  def __init__(self, fd):
    if not isinstance(fd, int) or fd < 0:
      raise TypeError
    self.fd = fd
    self.generation = 0
    self.attr_valid = self.attr_valid_ns = 0
    self.entry_valid = self.entry_valid_ns = 0
    self.negative_valid = self.negative_valid_ns = 0
    # Maps open directory filehandles to their full dirent_data strings
    # (or None if not listed yet).
    # This dict caches the output of HandleListDir until the FUSE client
    # reads it (and calls FUSE_RELEASEDIR).
    self.dirh_to_dirent_data = {}

  def WriteReply(self, data='', errno_num=None):
    """Build and write reply packet after a preceding Read()."""
    if not isinstance(data, str):
      raise TypeError
    if errno_num is None:
      errno_num = 0
    elif not isinstance(errno_num, int):
      raise TypeError
    elif errno_num > 0:
      # The kernel expects negative errnos.
      errno_num = -errno_num
    size = 16 + len(data)
    packet = struct.pack('=LlQ', size, errno_num, self.unique) + data
    # This raises errno.EINVAL on bad packet format.
    os.write(self.fd, packet)

  def NegotiateInit(self):
    self.Read()
    assert self.opcode == FUSE_INIT
    #print (self.init_major, self.init_minor)  # (7L, 8L) on lila
    assert self.init_major == FUSE_KERNEL_VERSION, (
        'major version mismatch: kernel=%d.%d lib=%d.%d' %
        (self.init_major, self.init_minor,
         FUSE_KERNEL_VERSION, FUSE_KERNEL_MINOR_VERSION))
    self.use_minor = min(self.init_minor, FUSE_KERNEL_MINOR_VERSION)
    self.WriteReply(FormatInitOut(
        major=self.init_major, minor=self.use_minor, max_readahead=65536,
        flags=0, # !! FUSE_ASYNC_READ | FUSE_POSIX_LOCKS for minor >= 8.
        unused=0, max_write=65536))
    return self

  @classmethod
  def MountAndOpen(cls, config):
    """Mount a FUSE filesystem according to config, and return new Server."""
    if not isinstance(config, Config):
      raise TypeError
    config.Validate()
    print >>sys.stderr, 'info: mounting FUSE to %s' % config.mount_point
    fd = FuseMount(config.mount_prog, config.mount_point, config.mount_opts)
    # TODO(pts): call FuseUnmount(() when we die (in a finally: block)
    assert stat.S_ISCHR(os.fstat(fd).st_mode)
    print >>sys.stderr, 'info: negotiating version and parameters'
    server = cls(fd)
    server.attr_valid = int(config.attr_timeout)
    server.attr_valid_ns = int((
        config.attr_timeout - int(config.attr_timeout)) * 1000000)
    server.entry_valid = int(config.entry_timeout)
    server.entry_valid_ns = int((
        config.entry_timeout - int(config.entry_timeout)) * 1000000)
    if config.negative_timeout > 0:
      server.negative_valid = int(config.negative_timeout)
      server.negative_valid_ns = int((
          config.negative_timeout - int(config.negative_timeout)) * 1000000)
    else:
      server.negative_valid = server.negative_valid_ns = -1
    return server.NegotiateInit()

  def NewHandle(self, is_dirh=False):
    """Allocate and return a new filehandle or dirhandle."""
    # TODO(pts): Faster handle allocation.
    dirh = 1
    while dirh in self.dirh_to_dirent_data:
      dirh += 1
    if is_dirh:
      value = None
    else:
      value = False
    self.dirh_to_dirent_data[dirh] = value
    return dirh

  def Read(self):
    # TODO(pts): Negotiate maximum packet size.
    try:
      packet = os.read(self.fd, 65536 + 100)
    except OSError, e:
      if e.errno == errno.ENODEV:
        # Filesystem unmount gets propagated to us.
        self.opcode = FUSE_DESTROY
        self.unique = self.nodeid = self.uid = self.gid = self.pid = 0
        return
    # TODO(pts): Convert long values to int (after each struct.unpack).
    (size, self.opcode, self.unique, self.nodeid,
        self.uid, self.gid, self.pid) = struct.unpack('=LLQQLLL', packet[:36])
    data = packet[40:]
    if size != len(packet):
      raise ValueError('inconsistent length: %d vs %s\n' % (size, len(packet)))
    if self.opcode == FUSE_INIT:
      # struct fuse_init_in
      if len(data) < 16:
        raise ValueError('FUSE_INIT data too short: %d < 16' % len(data))
      # (init_major, init_minor): The FUSE version in the kernel
      self.init_major, self.init_minor, self.init_readahead, self.init_flags = (
          struct.unpack('=LLLL', data[:16]))
    elif self.opcode == FUSE_OPENDIR or self.opcode == FUSE_OPEN:
      # struct fuse_open_in
      if len(data) < 8:
        raise ValueError('fuse_open_in too short: %d < 8' % len(data))
      self.open_flags, self.open_mode = struct.unpack('=LL', data[:8])
    elif self.opcode == FUSE_READDIR or self.opcode == FUSE_READ:
      # struct fuse_read_in
      if len(data) < 20:
        raise ValueError('fuse_read_in too short: %d < 20' % len(data))
      self.read_fh, self.read_offset, self.read_size = (
          struct.unpack('=QQL', data[:20]))
    elif self.opcode == FUSE_RELEASEDIR or self.opcode == FUSE_RELEASE:
      # struct fuse_release_in
      if len(data) < 12:
        raise ValueError('fuse_release_in data too short: %d < 12' % len(data))
      self.release_fh, self.release_flags = (
          struct.unpack('=QL', data[:12]))
    elif self.opcode == FUSE_LOOKUP:
      if data and data[-1] == '\0':
        self.lookup_name = data[:-1]  # Get rid of terminating \n
    elif self.opcode == FUSE_FORGET:
      # struct fuse_forget_in
      if len(data) < 8:
        raise ValueError('FUSE_FORGET data too short: %d < 8' % len(data))
      self.forget_nlookup = struct.unpack('=Q', data[:8])

  def ServeUntilUnmounted(self):
    # TODO(pts): Become multithreaded? Is it possible to reply asynchronously
    # to the kernel?
    # TODO(pts): Handle exceptions in Handle_* and recover from them?
    while True:
      self.Read()
      assert self.opcode != FUSE_INIT, 'already initialized'
      if self.opcode == FUSE_DESTROY:
        break
      print >>sys.stderr, 'info: got opcode=%d nodeid=%d' % (
          self.opcode, self.nodeid)

      # All remaining operation types need a reply.
      reply_data = None
      try:
        if self.opcode == FUSE_GETATTR:
          fuse_attr = self.Handle_FUSE_GETATTR()
          if isinstance(fuse_attr, int):
            reply_data = fuse_attr
          elif isinstance(fuse_attr, FuseAttr):
            reply_data = FormatAttrOut(
                fuse_attr=fuse_attr,
                attr_valid=self.attr_valid, attr_valid_ns=self.attr_valid_ns)
          else:
            raise TypeError
        elif self.opcode == FUSE_LOOKUP:
          ret = self.Handle_FUSE_LOOKUP()
          if isinstance(ret, int):
            reply_data = ret
          elif isinstance(ret, tuple):
            if len(ret) != 2:
              raise TypeError
            if not isinstance(ret[0], FuseAttr):
              raise TypeError
            if not isinstance(ret[1], int) and not isinstance(ret[1], long):
              raise TypeError
            reply_data = FormatEntryOut(
                fuse_attr=ret[0], nodeid=ret[1], generation=self.generation,
                attr_valid=self.attr_valid, attr_valid_ns=self.attr_valid_ns,
                entry_valid=self.entry_valid,
                entry_valid_ns=self.entry_valid_ns)
          else:
            raise TypeError
          if (reply_data in (errno.ENOENT, -errno.ENOENT) and
              self.negative_valid > 0):
            # Reply with a zero nodeid to make the kernel cache the missing
            # entry. This is based on fuse-2.5.3/lib/fuse.c.
            reply_data = FormatEntryOut(
                fuse_attr=FuseAttr(), nodeid=0, generation=0,
                attr_valid=0, attr_valid_ns=0,
                entry_valid=self.negative_valid,
                entry_valid_ns=self.negative_valid_ns)
        elif self.opcode == FUSE_OPENDIR:
          # TODO(pts): Disallow deletion of a nodeid while it's open.
          ret = self.HandleOpenDir()
          if isinstance(ret, int):
            reply_data = fuse_attr
          elif ret == '':
            # TODO(pts): Make it possible to return like HandleListDir.
            # TODO(pts): Tune with open_flags=FOPEN_KEEP_CACHE etc.
            reply_data = FormatOpenOut(
                fh=self.NewHandle(is_dirh=True), fopen_flags=0)
        elif self.opcode == FUSE_READDIR:
          dirent_data = self.dirh_to_dirent_data.get(self.read_fh)
          if dirent_data is False:
            reply_data = errno.ENOTDIR
          elif dirent_data is None:  # FUSE_OPENDIR has put None here.
            ret = self.HandleListDir()
            if isinstance(ret, int):
              reply_data = ret
            elif hasattr(ret, '__iter__'):  # a sequence
              dirent_data = self.dirh_to_dirent_data[self.read_fh] = (
                  FormatDirents(ret))
            else:
              raise TypeError
          if dirent_data is not None:
            reply_data = dirent_data[
                self.read_offset : self.read_offset + self.read_size]
        elif self.opcode == FUSE_RELEASEDIR:
          # TODO(pts): Interpret self.release_flags.
          dirent_data = self.dirh_to_dirent_data.get(self.read_fh)
          if dirent_data is False:
            reply_data = errno.ENOTDIR
          elif self.read_fh in self.dirh_to_dirent_data:
            del self.dirh_to_dirent_data[self.read_fh]
          reply_data = ''
        elif self.opcode == FUSE_OPEN:
          ret = self.Handle_FUSE_OPEN()
          if isinstance(ret, int):
            reply_data = ret
          elif isinstance(ret, tuple):
            if len(ret) != 2:
              raise TypeError
            if not isinstance(ret[0], int):
              raise TypeError
            if not isinstance(ret[1], int):
              raise TypeError
            # Make sure FUSE_OPENDIR won't allocate the same filehandle.
            self.dirh_to_dirent_data[ret[0]] = False
            reply_data = FormatOpenOut(fh=ret[0], fopen_flags=ret[1])
        elif self.opcode == FUSE_READ:
          reply_data = self.Handle_FUSE_READ()
          if isinstance(reply_data, int):
            pass
          elif isinstance(reply_data, str):
            pass
          else:
            raise TypeError
        elif self.opcode == FUSE_RELEASE:
          reply_data = self.Handle_FUSE_RELEASE()
          if isinstance(reply_data, int):
            pass
          elif reply_data is None or reply_data == '':
            reply_data = ''
            if self.release_fh in self.dirh_to_dirent_data:
              del self.dirh_to_dirent_data[self.release_fh]
          else:
            raise TypeError
        elif self.opcode == FUSE_FORGET:
          # The linux kernel is usually lazy to call FUSE_FORGET if there is
          # plenty of free memory. Do this to flush the indoe + dentry cache
          # about 5 seconds after your operation on the FUSE filesystem:
          # $ sleep 5; sudo sh -c 'echo 2 > /proc/sys/vm/drop_caches'
          # !! TODO(pts): Implement this, decrement refcount by
          # self.forget_nlookup except if self.nodeid == FUSE_ROOT_ID.
          reply_data = self.Handle_FUSE_FORGET()
          if isinstance(reply_data, int):
            pass
          elif reply_data is None:
            pass
          else:
            raise TypeError
        else:
          reply_data = self.HandleUnknown()
      except (OSError, IOError, socket.error), e:
        # TODO(pts): Print stack trace  with traceback.
        print >>sys.stderr, 'warning: %s' % e
        errno_num = e.errno

      if self.opcode == FUSE_FORGET:
        if reply_data is not None:
          assert isinstance(reply_data, int)
          print >>sys.stderr, (
              'warning: cannot propagate FUSE_FORGET errno=%d' % reply_data)
      elif isinstance(reply_data, int):
        self.WriteReply(errno_num=reply_data)
      elif isinstance(reply_data, str):
        self.WriteReply(reply_data)
      else:
        raise TypeError
      
  def HandleUnknown(self):
    """Handle an unknown opcode (which needs a reply).

    The request needs a reply, but we don't how what this self.opcode means,
    so we just return errno.ENOSYS.
    """
    print >>sys.stderr, 'info: unknown opcode=%d' % self.opcode
    return errno.ENOSYS

  def HandleUnimplemented(self):
    """Handle an unimplemented opcode (which needs a reply).

    The request needs a reply, but we don't have an implementation in a
    subclass for handling self.opcode, so we just return errno.ENOSYS.
    """
    print >>sys.stderr, 'info: unimplemented opcode=%d' % self.opcode
    return errno.ENOSYS

  def Handle_FUSE_GETATTR(self):
    """Return the FuseAttr corresponding to self.nodeid.

    This method should be overridden in subclasses.

    Upon error, return errno.* (or raise a suitable OSError etc.).
    """
    self.HandleUnimplemented()

  def Handle_FUSE_LOOKUP(self):
    """Return (fuse_attr, nodeid) for self.nodeid + self.lookup_name.

    This method finds a file (or directory) within a directory.
    Given a nodeid of a directory (self.nodeid) and a file name
    (self.lookup_name), this method finds and returns the nodeid and the
    attributes (FuseAttr) of the specified file in that the directory.

    This method should be overridden in subclasses.

    Upon error, return errno.* (or raise a suitable OSError etc.).

    A FUSE_LOOKUP on all directories from the filesystem root precedes most
    operations. For example, for a FUSE_OPEN on <mount_point>/foo/bar/baz
    first FUSE_LOOKUP(root, 'foo'), then FUSE_LOOKUP(foo, 'bar'), then
    FUSE_LOOKUP(foo_bar, 'baz'), then FUSE_OPEN(foo_bar_baz) gets called.
    """
    self.HandleUnimplemented()

  def HandleOpenDir(self):
    """Return '' if the directory can be listed.

    This method should be overridden in subclasses.

    Upon error, return errno.* (or raise a suitable OSError etc.).

    If this method returns '', then a subsequent self.HandleListDir() should
    be able to list the contents of the directory.
    """
    self.HandleUnimplemented()

  def HandleListDir(self):
    """Return sequence of Dirent objects within dir self.nodeid.

    This method should be overridden in subclasses.

    Upon error, return errno.* (or raise a suitable OSError etc.).
    """
    self.HandleUnimplemented()

  def Handle_FUSE_FORGET(self):
    """Forget self.nodeid, the kernel doesn't need it anymore.

    This method should be overridden in subclasses.

    Upon error, you may return errno.* (or raise a suitable OSError etc.), but
    you shouldn't, because that error won't be propagated to the kernel.
    
    The root node (with nodeid=FUSE_ROOT_ID) should never be forgotten.
    """

  def Handle_FUSE_OPEN(self):
    """Open file self.nodeid for self.open_flags and self.open_mode.

    This method should be overridden in subclasses.

    Upon error, return errno.* (or raise a suitable OSError etc.).

    self.open_flags is a bitwise or of the O_* constants defined in
    <fcntl.h>. self.open_mode & 07777 is used only when a new file is
    created by (self.open_flags & os.O_CREAT).

    Return a (fh, fopen_flags) tuple. fopen_flags is the bitwise or
    of FOPEN_*. If in doubt, leave fopen_flags=0. fh is a filehandle
    (unsigned integer) you generate. FUSE_READ (self.read_fh), FUSE_WRITE
    (self.write_fh) and FUSE_RELEASE (self.release_fh) will get the same
    filehandle. You must make sure not to return any fh who is in
    self.dirh_to_dirent_data.keys(), because those are reserved for dirhandles.
    After Handle_FUSE_OPEN return successfully, the caller adds the new
    filehandle to self.dirh_to_dirent_data.keys().
    """
    self.HandleUnimplemented()

  def Handle_FUSE_READ(self):
    """Read region (self.read_offset, self.read_size) from an open file.

    The file must be already open as (self.nodeid, self.fh).

    This method should be overridden in subclasses.

    Upon error, return errno.* (or raise a suitable OSError etc.).

    Return the string from the interval (self.read_offset, self.read_size).
    It can be shorter than self.read_size.

    Please note that the kernel won't read past the FuseAttr.size field
    returned for self.nodeid (by self.Handle_FUSE_GETATTR and
    self.Handle_FUSE_LOOKUP).
    """
    self.HandleUnimplemented()

  def Handle_FUSE_RELEASE(self):
    """Close an already open file (self.nodeid, self.fh).

    This method should be overridden in subclasses.

    Upon error, return errno.* (or raise a suitable OSError etc.).

    Return '' or None.
    """
    self.HandleUnimplemented()


class DemoServer(Server):
  """Demo filesystem implementing the Handle_* method of Server.

  The demo filesystem is quite primitive: it is read-only, and it has a few
  predefined directories and files.
  """

  __slots__ = Server.__slots__ + [
      'dir_contents', 'nodes', 'entries', 'file_contents', 'open_files']

  def Populate(self):
    """Populate the date for the demo filesystem."""
    # Maps directory inode numbers to sequences of directory contents. Each
    # sequence element is a (filename, type) pair, where type is
    # (st.st_mode & 0170000 >> 12), as in fuse_lowelevel.c
    # This dict is used by FUSE_READDIR.
    # TODO(pts): try using type_num==0 always.
    # We need an arbitrary positive Dirent(ino=). A zero value would hide the
    # entry from the kernel.
    self.dir_contents = {
        FUSE_ROOT_ID: [Dirent(name='bar.txt'),
                       Dirent(name='hi')],
        3: [Dirent(name='hello.txt')],
    }
    # Maps nodeids to FuseAttr objects. In other words: this dict contains
    # inode data. This dict is used by FUSE_GETATTR and FUSE_LOOKUP.
    # The size= values of regular files here must be consistent with the file
    # contents, because the kernel won't read past the file size.
    # self.Handle_FUSE_LOOKUP() and self.Handle_FUSE_GETATTR() take care of
    # this.
    # The ino= values here don't matter as long as they are positive (or not?).
    # They don't even have to be different from each other.
    # The ino= values are reported to the user by fstat(2) and stat(2).
    self.nodes = {
        FUSE_ROOT_ID: FuseAttr(stat.S_IFDIR | 0755),
        2: FuseAttr(stat.S_IFREG | 0644, ino=200),
        3: FuseAttr(stat.S_IFDIR | 0755),
        4: FuseAttr(stat.S_IFREG | 0600),
    }
    # Maps (nodeid, name) pairs to nodeid numbers. This dict is used by
    # FUSE_LOOKUP.
    self.entries = {
        (FUSE_ROOT_ID, 'bar.txt'): 2,
        (FUSE_ROOT_ID, 'hi'): 3,
        (3, 'hello.txt'): 4,
    }
    # Maps nodeids to file contents (as strings).
    self.file_contents = {
        2: 'What cocktail would you like to drink?\n',
        4: 'Hello, World!\n',
    }

    # Maps filehandles to nodeids.
    # !! Do we need this for a read-only filesystem? I don't think so,
    # we could just return filehandle=1 for everything. But then we'd need
    # reference counting before removing from self.dirh_to_dirent_data.
    self.open_files = {}
    return self

  def Handle_FUSE_GETATTR(self):
    fuse_attr = self.nodes.get(self.nodeid)
    if not fuse_attr:
      return errno.ENOENT
    if self.nodeid in self.file_contents:
      # Update the size attribute so the whole file can be read.
      fuse_attr.size = len(self.file_contents[self.nodeid])
    return fuse_attr

  def Handle_FUSE_LOOKUP(self):
    # Don't forget to override Handle_FUSE_FORGET if implementing a non-const
    # filesystem.
    #print ('LLL', self.nodeid, self.lookup_name, key)
    entry_key = (int(self.nodeid), self.lookup_name)
    entry_nodeid = self.entries.get(entry_key)
    entry_fuse_attr = entry_nodeid and self.nodes.get(entry_nodeid)
    if entry_fuse_attr:
      if entry_nodeid in self.file_contents:
        # Update the size attribute so the whole file can be read.
        entry_fuse_attr.size = len(self.file_contents[entry_nodeid])
      return entry_fuse_attr, entry_nodeid
    else:
      return errno.ENOENT

  def HandleOpenDir(self):
    dir_contents = self.dir_contents.get(self.nodeid)
    if self.dir_contents is not None:
      return ''
    elif self.nodeid in self.nodes:
      return errno.ENOTDIR
    else:
      return errno.ENOENT

  def HandleListDir(self):
    dir_contents = self.dir_contents.get(self.nodeid)
    if self.dir_contents is not None:
      return dir_contents
    elif self.nodeid in self.nodes:
      return errno.ENOTDIR
    else:
      return errno.ENOENT

  def Handle_FUSE_OPEN(self):
    if (self.open_flags & O_ACCMODE) != os.O_RDONLY:
      return errno.EROFS
    fh = self.NewHandle()
    self.open_files[fh] = self.nodeid
    return fh, 0

  def Handle_FUSE_READ(self):
    if self.read_fh not in self.open_files:
      return errno.EBADF  # This should never happen.
    nodeid = self.open_files[self.read_fh]
    assert self.nodeid == nodeid
    contents = self.file_contents.get(self.nodeid)
    if contents is None:
      # For non-constant filesystems we should ensure that a removed file is
      # still readable.
      return errno.EIO
    return contents[self.read_offset : self.read_offset + self.read_size]

  def Handle_FUSE_RELEASE(self):
    if self.release_fh not in self.open_files:
      return errno.EBADF  # This should never happen.
    nodeid = self.open_files.pop(self.release_fh)
    assert self.nodeid == nodeid


def main(argv):
  assert argv
  argv = list(argv)
  config = Config()
  if len(argv) > 2 and argv[1] == '-o':
    config.mount_opts = argv[2]
    del argv[1 : 3]
  if len(argv) != 2:
    print >>sys.stderr, 'Usage: %s [-o <mount_opts>] <mount-point>' % argv[0]
    sys.exit(1)
  config.mount_point = argv[1]
  fin = DemoServer.MountAndOpen(config).Populate()

  # TODO(pts): Users get EBUSY when accessing the mount point too early
  # (right after a few seconds).
  print >>sys.stderr, 'info: processing requests (indefinitely, ^C to abort)'
  fin.ServeUntilUnmounted()
  print >>sys.stderr, 'info: filesystem unmounted, exiting'


if __name__ == '__main__':
  sys.exit(main(sys.argv) or 0)
