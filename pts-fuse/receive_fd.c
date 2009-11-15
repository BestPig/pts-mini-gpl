#define DUMMY \
  set -ex; \
  gcc -ansi -W -Wall -I/usr/include/python2.4 -shared -fpic -fPIC -O2 \
  -o _receive_fd.so receive_fd.c; \
  exit;
/*
 * reveive_fd.c: Python module to receive a filehandle over an AF_UNIX socket.
 * by pts@fazekas.hu at Sat Nov 14 11:57:04 CET 2009
 *
 * Compile with:
 * 
 */

#include <Python.h>
#include <string.h>  /* strerror() */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

/* Based on receive_fd() in fuse-2.5.3/lib/mount.c, should work on Linux.
 * errbuf must be 256 characters maximum.
 */
/* return value:
 * >= 0  => fd
 * -1    => error
 */
static int receive_fd(int fd, char *errbuf) {
  struct msghdr msg;
  struct iovec iov;
  char buf[1];
  int rv;
  int connfd = -1;
  size_t ccmsg[CMSG_SPACE(sizeof(connfd))/sizeof(size_t)];
  struct cmsghdr *cmsg;

  iov.iov_base = buf;
  iov.iov_len = 1;

  msg.msg_name = 0;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  /* old BSD implementations should use msg_accrights instead of
   * msg_control; the interface is different. */
  msg.msg_control = ccmsg;
  msg.msg_controllen = sizeof(ccmsg);
  msg.msg_flags = 0;

  while(((rv = recvmsg(fd, &msg, 0)) == -1) && errno == EINTR);
  if (rv == -1) {
    /* TODO(pts): propagate errno to Python. */
    snprintf(errbuf, 256, "recvmsg: %s", strerror(errno));
    return -1;
  }
  if(!rv) {
    snprintf(errbuf, 256, "message not received");
    return -1;
  }

  cmsg = CMSG_FIRSTHDR(&msg);
  if (!cmsg->cmsg_type == SCM_RIGHTS) {
    snprintf(errbuf, 256, "got control message of unknown type %d",
             cmsg->cmsg_type);
    return -1;
  }
  rv = *(int*)CMSG_DATA(cmsg);
  if (rv < 0) {
    snprintf(errbuf, 256, "received negative fd %d", rv);
    return -1;
  }
  return rv;
}

/* Python bindings */

static PyObject *receive_fd_function(PyObject *self, PyObject *args) {
  char errbuf[256];
  int fd, got_fd;
  (void)self;
  if (!PyArg_ParseTuple(args, "i", &fd))
    return NULL;
  if (fd < 0) {
    PyErr_SetString(PyExc_ValueError, "fd must be a filehandle");
    return NULL;
  }
  got_fd = receive_fd(fd, errbuf);
  if (got_fd < 0) {
    PyErr_SetFromErrnoWithFilename(PyExc_OSError, errbuf);
    return NULL;
  }
  return PyInt_FromLong(got_fd);
}

static PyMethodDef receive_fd_methods[] = {
  {"receive_fd", receive_fd_function, METH_VARARGS,
   "Take an AF_UNIX file descriptor and return the new file descriptor "
   "received."},
  {NULL, NULL, 0, NULL}   /* end-of-list sentinel value */
};

#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
init_receive_fd(void)
{
  PyObject *m, *mod_dic;

  if (!(m = Py_InitModule3(
      "_receive_fd", receive_fd_methods,
      "Receive a file descriptor on an AF_UNIX socket."))) {
    goto exit;
  }

  /* register classes */
  if (!(mod_dic = PyModule_GetDict(m))) { goto exit; }

  /* PyModule_AddIntConstant(m, "ANSWER", 42); */

exit:
  if (PyErr_Occurred()) {
    /* PyErr_Print(); */
    PyErr_SetString(PyExc_ImportError, "receive_fd: init failed");
  }
}
