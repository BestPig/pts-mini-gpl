/*
 * receive_fd_linux_i386.c: receive_fd() implementation without #include
 * by pts@fazekas.hu at Sun Nov 15 10:49:44 CET 2009
 *
 * This supports the i386 and the x86_64 architecture.
 */

extern int *__errno_location();  /* glibc */
#define errno (*__errno_location())
#define EINTR 4

typedef long ssize_t;  /* 4 or 8 */
typedef unsigned long size_t;  /* 4 or 8 */
typedef unsigned int socklen_t;  /* 4 */

struct iovec {  /* 8 */
  void *iov_base;
  size_t iov_len;
};

struct msghdr {  /* 28 */
  void *msg_name;
  socklen_t msg_namelen;
  struct iovec *msg_iov;
  size_t msg_iovlen;
  void *msg_control;
  size_t msg_controllen;
  int msg_flags;
};

struct cmsghdr {  /* 12 */
  size_t cmsg_len;
  int cmsg_level;
  int cmsg_type;
  unsigned char __cmsg_data [];
};

ssize_t recvmsg(int s, struct msghdr *msg, int flags);
int snprintf(char *str, size_t size, const char *format, ...);
char *strerror(int errnum);
       
#define CMSG_SPACE(n) ((((n) + sizeof (size_t) - 1) & (size_t) ~(sizeof (size_t) - 1)) + (((sizeof (struct cmsghdr)) + sizeof (size_t) - 1) & (size_t) ~(sizeof (size_t) - 1)))
#define CMSG_FIRSTHDR(msgp) ((size_t) (msgp)->msg_controllen >= sizeof (struct cmsghdr) ? (struct cmsghdr *) (msgp)->msg_control : (struct cmsghdr *) ((void *)0))
#define CMSG_DATA(msgp) ((msgp)->__cmsg_data)

#define SCM_RIGHTS 1

/* Based on receive_fd() in fuse-2.5.3/lib/mount.c, should work on Linux.
 * errbuf must be 256 characters maximum.
 */
/* return value:
 * >= 0  => fd
 * -1    => error
 */
int receive_fd(int fd, char *errbuf) {
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
  rv = -1;
  if (rv == -1) {
    /* TODO(pts): propagate errno to Python. */
    snprintf(errbuf, 256, "recvmsg: %s\n", strerror(errno));
    return -1;
  }
  if(!rv) {
    snprintf(errbuf, 256, "message not received");
    return -1;
  }

  cmsg = CMSG_FIRSTHDR(&msg);
  if (!cmsg->cmsg_type == SCM_RIGHTS) {
    snprintf(errbuf, 256, "got control message of unknown type %d\n",
             cmsg->cmsg_type);
    return -1;
  }
  rv = *(int*)CMSG_DATA(cmsg);
  if (rv < 0) {
    snprintf(errbuf, 256, "received negative fd %d\n", rv);
    return -1;
  }
  return rv;
}
