README for rfsdelta
by pts@fazekas.hu at Thu Jan 11 17:33:59 CET 2007

rfsdelta is a kernel module for the Linux 2.6 series, which collects all
filesystem inode changes (recursively), and reports them to a userspace
process. It is similar to inotify, dnotify (but provides recurisve
notification, and only a single watcher process is allowed), fschange,
fsevent and rlocate (but also reports unlink(), rmdir() and st_*). rfsdelta
was based on the kernel module shipped with rlocate 0.5.5.

Limitations
~~~~~~~~~~~
-- Only one watch per system: the 2nd attempt to open rfsdelta-events
   will give `Device or resource busy'.
-- Doesn't work with rlocate.ko or any other security module (registered
   with register_security()). A subsequent call to register_security will
   yield `Invalid parameters'. (This shouldn't be hard to fix provided that
   rlocate.ko is the first security module to be loaded.)

How to compile and install
~~~~~~~~~~~~~~~~~~~~~~~~~~
You need a Linux 2.6 kernel source tree, and a kernel with these settings:

  CONFIG_MODULES=y
  CONFIG_SECURITY=y
  CONFIG_SECURITY_CAPABILITES: disabled or module (=m)

In the folder containing `rfsdelta', run

  $ make

This has built the rfsdelta.ko kernel module.

As root, run this to try:

  # insmod rfsdelta.ko
  # cat /proc/rfsdelta
  # rmmod rfsdelta

As root, run these to install:

  # make install
  # modprobe rfsdelta

To try it, as root start

  # tr '\000' '\n' </proc/rfsdelta-event

If you are not using udev or devfs, create the device manually:

  # mknod /dev/rfsdelta-event `perl -pe'$_ x=s@^devnumbers:@@' /proc/rfsdelta`

How to configure
~~~~~~~~~~~~~~~~
If you want to force a specific character device major number
(and minor number 0) for rfsdelta-event (instead of letting the kernel
allocate one automatically), use

  # modprobe rfsdelta major=<int>

You can change a couple of settings in /proc/rfsdelta. Run this command
as root:

  # echo '<key>:<value>' >/proc/rfsdelta

Example:

  # echo 'activated:0' >/proc/rfsdelta

A write always succeeds, syntax errors are not indicated.

The settings in /proc/rfsdelta are (just as in rlocate):

-- version (default: 0.06 or higher, read-only).
-- activated: (default: `r', single-character, read-write). The activation
   status of the watch. Value `0' means inactive: events are not watched
   for, and reading rfsdelta-event returns `Resource temporary unavailable'
   (EAGAIN) immediately. Value `1' means active: events are watched for,
   reading rfsdelta-event returns next event, or puts the process to sleep
   (unless O_NOBLOCK) to wait for next event. The default value `r' means
   only-if-reader: events are watched for only if somebody is reading
   rfsdelta-event, otherwise it's like `1'. (There is also a legacy value
   `d': it means ``daemon'', and it's like `1', and it gets changed to `1'
   automatically at the next read.)
-- startingpath (default: empty, read-write): events with filename outside
   this folder are not reported. An extra slash is always ensured at the
   end of `startingpath' (if it's not empty).
   Please note that a path starts by a device name and a colon
   (such as `sda5:' or `dm-1:' for LVM devices), which has to be matched
   by startingpath. Please note also that `sda5:/foo' matches `sda5:/foo'
   and `sda5:/foo/bar/baz', but not `sda5:/food'.
-- devnumbers (default: something like `c 254 0', read-only). Device number
   information about the /dev/rfsdelta-event character device. First word
   is device type (always `c', meaning character device), second word is
   the major namuber, third word is the minor number (always 0). The value
   of `devnumbers' can be passed to mknod(1) in this order.

How to use
~~~~~~~~~~
Once the 

Don't 



To try it, as root start

  # tr '\000' '\n' </dev/rfsdelta-event

!!

How to use
~~~~~~~~~~
Once the rfsdelta.ko module is loaded and rfsdelta is activated (by default
or with `echo activated:1 >/proc/rfsdelta'), rfsdelta gathers filesystem
change events into an internal queue. By reading /proc/rfsdelta-event (as
root) or /dev/rfsdelta-event (by any user who has permissions), the contents
of the queue can be read in order. (Only one process can have the queue
files open at the same time.)

Each item in the queue is a null-terminated string. Details:

-- for file creation (open(O_CREAT)):

   "c" <dev> "," <ino> "," <mode> "," <nlink> "," <rdev> ":" <devname> ":/" <pathname> "\0"

-- for mkdir():

   "M" <dev> ",?:" <devname> ":/" <pathname> "\0"

-- for rmdir():

   "R" <dev> "," <ino> "," <mode> "," <nlink> "," <rdev> ":" <devname> ":/" <pathname> "\0"

-- for unlink():

   "u" <dev> "," <ino> "," <mode> "," <nlink> "," <rdev> ":" <devname> ":/" <pathname> "\0"

   If the last link to a file is about to be removed, <nlink> is reported to
   be `1'.

-- for the new inode of link():

   "l" <dev> ",?:" <devname> ":/" <pathname> "\0"

-- for the new inode of symlink():

   "s" <dev> ",?:" <devname> ":/" <pathname> "\0"

-- for mknod() (including character and block devices, pipes and sockets):

   "m" <dev> ",?:" <devname> ":/" <pathname> "\0"

-- for rename():

   "o" <dev> "," <ino> "," <mode> "," <nlink> "," <rdev> ":" <devname> ":/" <old-pathname> "\0"
   "a" <dev> "," <ino> "," <mode> "," <nlink> "," <rdev> ":" <devname> ":/" <new-pathname> "\0"

All events are reported _before_ they take place. That's the reason why
mkdir() cannot report `ino'.

The following operations are not reported by rfsdelta:

-- non-filesystem operations
-- read-only filesystem operations
-- intra-file operations (such as write(2))
-- chmod(), chown() and setxattr()

In addition to those, rfsdelta watches for mount() and umount(), but defers
reporting these until a change to a file (any of those events above) is
made. mount-related events are:

-- there was a mount() but no umount():

   "1\n"

-- there was an umount() but no mount():

   "2\n"

-- there was both a mount() and an umount():

   "3\n"

The <dev>, <ino>, <mode>, <nlink>, <rdev> fields are hexadecimal numbers
(created with printf("%X", ...)) holding the value of the corresponding st_*
field of `struct stat':

-- <dev> is st_dev in `struct stat'
-- <ino> is st_ino in `struct stat'
-- <mode> is st_mode in `struct stat'
-- <nlink> is st_nlink in `struct stat'
-- <rdev> is st_rdev in `struct stat'

The <devname> field is the device name in the kernel (such as `sda5' or
`dm-1', the latter for LVM logical volumes). Usually there is a device node
in `/dev' by that name (except for `dm-*', whose device nodes are usually in
`/dev/mapper'). The recommended method for finding the /dev/* device and the
device for the mount point is using <dev> instead of <devname>, and matching
<dev> to the stat()ted st_dev values of the devices in /proc/mounts.

<pathname> indicates both the name and the directory path of the file,
separated by `/'. It is always preceded by `/', never starts or ends by `/',
but it may contain `/' (but no `//').

Most important incompatible differences from rlocate
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
-- /dev/rfsdelta-event and /proc/rfsdelta-event instead of /dev/rlocate.
-- /proc/rfsdelta instead of /proc/rlocate.
-- `updatedb', `output' and `excludedir' settings disabled in
   /proc/rfsdelta.
-- /dev/rfsdelta-event reports st_* numbers upon each change.
-- Change mode characters (such as `a' or `d') are different.
-- Source file name is reported for each rename().


Changes over rlocate.ko by pts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OK: module: rlocate renamed to rfsdelta throughout the source code
OK: module: no Makefile.in
OK: module: check for CONFIG_SECURITY=y, no CONFIG_SECURITY_CAPABILITIES=
OK: module: devnumbers in /proc/rfsdelta
OK: module: add: rmdir(), unlink()
OK: module: lock /dev/rfsdelta-event for further reading
OK: module: reenabled filenames_wq (so reading /dev/rfsdelta-event blocks
    if no events are available)
OK: module: set ACTIVATED_ARG='r' by default
OK: module: disabled UPDATEDB_ARG and OUTPUT_ARG
OK: module: major=<int> arg to force a specific major number
OK: module: /proc/rfsdelta-event
OK: module: startingpath checks re-enabled (with trailing `/' check)

Improvement possibilities
~~~~~~~~~~~~~~~~~~~~~~~~~
!! stress test (e.g. rsync, kernel compilation)
!! proper hooks for .register_security (just `return 0' is not enough, this
   won't call the security ooks), call all unregister_security bu hand upon
   module removal
Dat: module: do we have to lock operations on filenames_list? I don't
     think so, drivers/acpi/event.c doesn't lock either
Dat: mkdir(), creat() is `a', mv() is `m', unlink() is not reported,
     file changes are not reported in /dev/rfsdelta
Dat: /proc/acpi/event can be a model for making the process sleep on event
     kernel:drivers/acpi/event.c:acpi_system_read_event()
       kernel:drivers/acpi/bus.c:acpi_bus_receive_event()
     -> rlocate_dev_read() (just uncomment filenames_wq)
     interruptible_sleep_on()
Dat: Errno::EAGAIN==Errno::EWOULDBLOCK
Dat: example: crw-rw----  1 root root 254, 0 Jan 11 14:55 /dev/rfsdelta
Dat: having two processes reading /dev/rfsdelta, Ctrl-<C> is
     ignored in the 2nd one (because of the mutex??) -- anyway we have only 1
     queue, so we'll return EBUSY for the 2nd open().
Dat: the default of ACTIVATED_ARG is for disabling the filling of
     filenames_list until someone is reading /dev/rfsdelta
Imp: get rid of UPDATEDB_ARG
Imp: avoid empty lines (UPDATEDB_ARG etc.) in /proc/rfsdelta
Imp: isn't it too early to check mount points at "1\0" for a slow mount?
Imp: don't we have a race condition with the system performing the task and
     our security module inserting the event?
Imp: call parse_proc() before close() to return syntax errors
Dat: Make inode_* operations are changed to inode_post_* operations to avoid
     race conditions (only inode_post_setxattr() in 2.6.18.1 -- earlier
     kernels have much more inode_post_*).

__END__
