#! /usr/bin/python2.4
# by pts@fazekas.hu at Sat Oct 30 17:28:18 CEST 2010

"""pysshsftp: Python SFTP client using the ssh(1) command.

pysshsftp is a proof-of-concept SFTP client for Unix, which uses the OpenSSH
ssh(1) command-line tool (for establishing the secure connection), but it
doesn't use the sftp(1) command-line tool (so it can e.g. upload files
without truncating them first).

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of   
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
"""

__author__ = 'pts@fazekas.hu (Peter Szabo)'

import struct
import subprocess
import sys

# --- constants based on sftp.h,v 1.9 2008/06/13
#
# draft-ietf-secsh-filexfer-01.txt

# version
SSH2_FILEXFER_VERSION = 3

# client to server
SSH2_FXP_INIT = 1
SSH2_FXP_OPEN = 3
SSH2_FXP_CLOSE = 4
SSH2_FXP_READ = 5
SSH2_FXP_WRITE = 6
SSH2_FXP_LSTAT = 7
SSH2_FXP_STAT_VERSION_0 = 7
SSH2_FXP_FSTAT = 8
SSH2_FXP_SETSTAT = 9
SSH2_FXP_FSETSTAT = 10
SSH2_FXP_OPENDIR = 11
SSH2_FXP_READDIR = 12
SSH2_FXP_REMOVE = 13
SSH2_FXP_MKDIR = 14
SSH2_FXP_RMDIR = 15
SSH2_FXP_REALPATH = 16
SSH2_FXP_STAT = 17
SSH2_FXP_RENAME = 18
SSH2_FXP_READLINK = 19
SSH2_FXP_SYMLINK = 20

# server to client
SSH2_FXP_VERSION = 2
SSH2_FXP_STATUS = 101
SSH2_FXP_HANDLE = 102
SSH2_FXP_DATA = 103
SSH2_FXP_NAME = 104
SSH2_FXP_ATTRS = 105

SSH2_FXP_EXTENDED = 200
SSH2_FXP_EXTENDED_REPLY = 201

# attributes
SSH2_FILEXFER_ATTR_SIZE = 0x00000001
SSH2_FILEXFER_ATTR_UIDGID = 0x00000002
SSH2_FILEXFER_ATTR_PERMISSIONS = 0x00000004
SSH2_FILEXFER_ATTR_ACMODTIME = 0x00000008
SSH2_FILEXFER_ATTR_EXTENDED = 0x80000000

# portable open modes
SSH2_FXF_READ = 0x00000001
SSH2_FXF_WRITE = 0x00000002
SSH2_FXF_APPEND = 0x00000004
SSH2_FXF_CREAT = 0x00000008
SSH2_FXF_TRUNC = 0x00000010
SSH2_FXF_EXCL = 0x00000020

# statvfs@openssh.com f_flag flags
SSH2_FXE_STATVFS_ST_RDONLY = 0x00000001
SSH2_FXE_STATVFS_ST_NOSUID = 0x00000002

# status messages
SSH2_FX_OK = 0
SSH2_FX_EOF = 1
SSH2_FX_NO_SUCH_FILE = 2
SSH2_FX_PERMISSION_DENIED = 3
SSH2_FX_FAILURE = 4
SSH2_FX_BAD_MESSAGE = 5
SSH2_FX_NO_CONNECTION = 6
SSH2_FX_CONNECTION_LOST = 7
SSH2_FX_OP_UNSUPPORTED = 8
SSH2_FX_MAX = 8

# ---

# openssh 5.1 sftp-server(1) doesn't accept longer messages.
SFTP_MAX_MSG_LENGTH = 256 * 1024

FX_STATUS_MESSAGES = {
    SSH2_FX_OK: 'No error',
    SSH2_FX_EOF: 'End of file',
    SSH2_FX_NO_SUCH_FILE: 'No such file or directory',
    SSH2_FX_PERMISSION_DENIED: 'Permission denied',
    SSH2_FX_FAILURE: 'Failure',
    SSH2_FX_BAD_MESSAGE: 'Bad message',
    SSH2_FX_NO_CONNECTION: 'No connection',
    SSH2_FX_CONNECTION_LOST: 'Connection lost',
    SSH2_FX_OP_UNSUPPORTED: 'Operation unsupported',
}


class SftpStatusError(IOError):
  def __init__(self, *args):
    if len(args) == 1 and isinstance(args[0], int):
      # Append the error message to the error code.
      args = (args[0], FX_STATUS_MESSAGES.get(args[0], 'Unknown status'))
    IOError.__init__(self, *args)


def SendMsg(f, msg):
  assert isinstance(msg, str)
  assert len(msg) < SFTP_MAX_MSG_LENGTH
  buf = struct.pack('>L', len(msg)) + msg
  got = f.write(buf)
  assert got is None  # The file(...) object returns None on write().
  f.flush()


def RecvRes(f):
  size_str = f.read(4)
  assert len(size_str) == 4, 'EOF at message length, got=%d' % len(size_str)
  size = int(struct.unpack('>L', size_str)[0])
  res = f.read(size)
  assert len(res) == size, 'EOF in incoming message, got=%d expected=%d' % (
      len(res), size)
  return res


def RaiseIfErrorStatus(res, msg_id):
  if len(res) >= 9 and res.startswith(chr(SSH2_FXP_STATUS)):
    _, res_id, res_status = struct.unpack('>BLL', res[:9])
    assert res_id == msg_id
    raise SftpStatusError(int(res_status))


def ParseKeyValuePairs(res, i, pair_count):
  """A negative pair_count means no limitation."""
  h = {}
  while i < len(res) and pair_count != 0:
    pair_count -= 1
    i += 4
    assert i <= len(res)
    size = int(struct.unpack('>L', res[i - 4: i])[0])
    assert i + size <= len(res)
    key = res[i : i + size]
    i += size
    i += 4
    assert i <= len(res)
    size = int(struct.unpack('>L', res[i - 4: i])[0])
    assert i + size <= len(res)
    value = res[i : i + size]
    i += size
    h[key] = value
  return i, h


def ParseAttrs(res, i):
  """Parse an SFTP attrs (stat) structure in res[i:]."""
  attrs = {}
  i += 4
  assert i <= len(res)
  # Flags are a bitwise or of SSH2_FILEXFER_ATTR_* constants.
  attrs['flags'] = flags = int(struct.unpack('>L', res[i - 4 : i])[0])
  if flags & SSH2_FILEXFER_ATTR_SIZE:
    i += 8
    assert i <= len(res)
    attrs['size'] = int(struct.unpack('>Q', res[i - 8 : i])[0])
  if flags & SSH2_FILEXFER_ATTR_UIDGID:
    i += 8
    assert i <= len(res)
    attrs['uid'] = int(struct.unpack('>L', res[i - 8 : i - 4])[0])
    attrs['gid'] = int(struct.unpack('>L', res[i - 4 : i])[0])
  if flags & SSH2_FILEXFER_ATTR_PERMISSIONS:
    i += 4
    assert i <= len(res)
    attrs['perm'] = int(struct.unpack('>L', res[i - 4 : i])[0])
  if flags & SSH2_FILEXFER_ATTR_ACMODTIME:
    i += 8
    assert i <= len(res)
    attrs['atime'] = int(struct.unpack('>L', res[i - 8 : i - 4])[0])
    attrs['mtime'] = int(struct.unpack('>L', res[i - 4 : i])[0])
  if flags & SSH2_FILEXFER_ATTR_EXTENDED:
    # sftp-server(1) usually doesn't return this one.
    i += 4
    assert i <= len(res)
    count = int(struct.unpack('>L', res[i - 4 : i])[0])
    i, h = ParseKeyValuePairs(res, i)
    for key in h:
      attrs['_' + key] = h[key]
  return attrs


def SftpStat(rf, wf, version, msg_id, path):
  if version == 0:
    msg_type = SSH2_FXP_STAT_VERSION_0
  else:
    msg_type = SSH2_FXP_STAT
  SendMsg(wf, struct.pack('>BLL', msg_type, msg_id, len(path)) + path)
  res = RecvRes(rf)
  RaiseIfErrorStatus(res, msg_id)
  assert len(res) >= 5
  res_type, res_id = struct.unpack('>BL', res[:5])
  assert res_id == msg_id
  assert res_type == SSH2_FXP_ATTRS
  return ParseAttrs(res, 5)


def main(argv):
  ssh_args = argv[1:] or ['127.0.0.1']
  # TODO(pts): Check that ssh_args doesn't already contain a command.
  ssh_args.append('sftp')

  # TODO(pts): Also open STDIN and STDOUT.
  p = subprocess.Popen(['ssh', '-s'] + ssh_args,
                       stdin=subprocess.PIPE, stdout=subprocess.PIPE)
  rf = p.stdout
  wf = p.stdin

  # Do the init handshake.
  SendMsg(wf, struct.pack('>BL', SSH2_FXP_INIT, SSH2_FILEXFER_VERSION))
  res = RecvRes(rf)

  # Parse the init response.
  assert res.startswith(chr(SSH2_FXP_VERSION))
  # Example res: '\x02\x00\x00\x00\x03\x00\x00\x00\x18posix-rename@openssh.com\x00\x00\x00\x011\x00\x00\x00\x13statvfs@openssh.com\x00\x00\x00\x012\x00\x00\x00\x14fstatvfs@openssh.com\x00\x00\x00\x012'
  assert len(res) >= 5
  version = int(struct.unpack('>L', res[1 : 5])[0])
  # Don't check this, we should be able to use any server version, just like
  # what sftp-client does.
  #assert version in (0, 1, 2, 3), (
  #    'version mismatch: client=%d server=%d' %
  #    (SSH2_FILEXFER_VERSION, version))
  i = 5
  i, server_init_flags = ParseKeyValuePairs(res, i, -1)

  # Example: {'fstatvfs@openssh.com': '2', 'statvfs@openssh.com': '2',
  #           'posix-rename@openssh.com': '1'}
  print >>sys.stderr, 'info: server_init_flags = %r' % server_init_flags
  msg_id = 0

  # Send a simple stat request and 
  msg_id += 1
  attrs = SftpStat(rf, wf, version, msg_id, '/dev/null')
  print >>sys.stderr, 'info: stat("/dev/null") = %r' % attrs

  # Close the SSH client connection.
  wf.close()
  rf.close()
  print >>sys.stderr, 'info: waiting for the SSH process exit'
  status = p.wait()
  assert status == 0, 'SSH process exited with status=0x%x' % status
  print >>sys.stderr, 'info: sftp done'

if __name__ == '__main__':
  sys.exit(main(sys.argv))
