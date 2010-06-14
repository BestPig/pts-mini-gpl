#! /bin/bash --
#
# fix_hplj1018.sh; fixer script for HP LaserJet 1018 printing with CUPS
# by pts@fazekas.hu for Ubuntu Hardy at Tue Dec 16 23:26:02 CET 2008
# revised for Ubuntu Karmic and Lucid at Mon Jun 14 09:41:47 CEST 2010

test "$EUID" != 0 && exec sudo -- "$0" "$@"

CUPSYS=""
for F in /etc/init.d/{cupsys,cups}; do
  if test -f "$F"; then
    CUPSYS="$F"
    break
  fi
done
if test -z "$CUPSYS" ||
   ! test -f /etc/cups/printers.conf; then
  echo "$0: error: cups/cupsys init script not found, try: sudo apt-get install cups" >&2
  exit 3
fi

if ! test -f /usr/sbin/hplj1018 ||
   ! test -d /usr/share/foo2zjs/firmware ||
   ! test -f /usr/share/ppd/foo2zjs/HP-LaserJet_1018.ppd.gz; then
  echo "$0: error: foo2zjs not found, try: sudo apt-get install foo2zjs" >&2
  exit 3
fi

RULES=""
for F in /{lib,etc}/udev/rules.d/85-hplj10xx.rules; do
  if test -f "$F"; then
    RULES="$F"
    break
  fi
done
if test -z "$RULES"; then
  echo "$0: error: udev rules file not found, try: sudo apt-get install foo2zjs" >&2
  exit 2
fi

if test -f /usr/share/foo2zjs/firmware/sihp1018.dl &&
   test "90cce767b25149433024a8307d7320fbd4ead879  -" != "`sha1sum </usr/share/foo2zjs/firmware/sihp1018.dl`"; then
  echo "$0: error: bad firmware file, please remove: /usr/share/foo2zjs/firmware/sihp1018.dl"
  exit 4
fi

if ! test -f /usr/share/foo2zjs/firmware/sihp1018.dl; then
  set -ex
  wget -O /usr/share/foo2zjs/firmware/sihp1018.tar.gz http://foo2zjs.rkkda.com/firmware/sihp1018.tar.gz
  tar xzOf /usr/share/foo2zjs/firmware/sihp1018.tar.gz sihp1018.img >/usr/share/foo2zjs/firmware/sihp1018.img
  rm -f /usr/share/foo2zjs/firmware/sihp1018.tar.gz
  cat >/usr/share/foo2zjs/firmware/pts-arm2hpdl.c <<'END'
#define DUMMY \
  set -ex; gcc -W -Wall -s -O2 -o pts-arm2hpdl "$0"; exit
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int
error(int fatal, char *fmt, ...)
{
	va_list ap;

	if (fatal)
	    fprintf(stderr, "Error: ");
	else
	    fprintf(stderr, "Warning: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fatal > 0)
	    exit(fatal);
	else
	    return (fatal);
}

void
usage(void)
{
	fprintf(stderr,
"Usage:\n"
"	pts-arm2hpdl sihp1005.img > sihp1005.dl\n"
"\n"
"	Add HP download header/trailer to an ARM ELF binary.\n"
"	If the file already has an HP header, just copy it to stdout.\n");
	exit(1);
}

/*
 * Compute HP-style checksum
 */
long
docheck(long check, unsigned char *buf, int len)
{
    int	i;

    if (len & 1)
	error(1, "We should never see an odd number of bytes in this app.\n");

    for (i = 0; i < len; i += 2)
	check += (buf[i]<<0) | (buf[i+1]<<8);
    return check;
}

int
main(int argc, char *argv[])
{
	extern int	optind;
	extern char	*optarg;
	int		rc;
	unsigned char	buf[BUFSIZ];
	int		len;
	FILE		*fp;
	struct stat	stats;
	int		size;
	unsigned char	elf[4];
	long		check;
	int		iself;
	int		ispjl;

	if (argc != 2)
	    usage();

	/*
	 * Open the file and figure out if its an ELF file
	 * by reading the first 4 bytes.
	 */
	fp = fopen(argv[1], "r");
	if (!fp)
	    error(1, "Can't open '%s'\n", argv[1]);

	len = fread(elf, 1, sizeof(elf), fp);
	if (len != 4)
	    error(1, "Premature EOF on '%s'\n", argv[1]);

	iself = 0;
	ispjl = 0;
	check = 0;
	if (memcmp(elf, "\177ELF", 4) == 0)
	{
	    /*
	     * Its an ELF executable file
	     */
	    unsigned char	filhdr[17] =
	    {
		0xbe, 0xef, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
		0, 0, 0, 0,	/* size goes here */
		0, 0, 0,
	    };
	    unsigned char	sechdr[12] =
	    {
		0xc0, 0xde, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 0, 0
	    };

	    iself = 1;

	    /*
	     * Create and write the file header
	     */
	    rc = stat(argv[1], &stats);
	    if (rc < 0)
		error(1, "Can't stat '%s'\n", argv[1]);

	    size = stats.st_size + 12 + 4;

	    filhdr[10] = size>>24;
	    filhdr[11] = size>>16;
	    filhdr[12] = size>> 8;
	    filhdr[13] = size>> 0;

	    rc = fwrite(filhdr, 1, sizeof(filhdr), stdout);

	    /*
	     * Create and write the section header
	     */
	    //memset(sechdr+2, 0, sizeof(sechdr)-2);

	    check = docheck(check, sechdr, sizeof(sechdr));
	    rc = fwrite(sechdr, 1, sizeof(sechdr), stdout);
	}
	else if (memcmp(elf, "\276\357AB", 4) == 0)
	{
	    /*
	     * This file already has an HP download header.
	     * Don't change it.
	     */
	}
	else if (memcmp(elf, "20", 2) == 0)
	{
	    unsigned char hdr[8];
	    
	    ispjl = 1;
	    printf("\033%%-12345X@PJL ENTER LANGUAGE=ACL\r\n");

	    rc = stat(argv[1], &stats);
	    if (rc < 0)
		error(1, "Can't stat '%s'\n", argv[1]);

	    size = stats.st_size - 8;

	    hdr[0] = 0x00;
	    hdr[1] = 0xac;
	    hdr[2] = 0xc0;
	    hdr[3] = 0xde;
	    hdr[4] = size>>24;
	    hdr[5] = size>>16;
	    hdr[6] = size>> 8;
	    hdr[7] = size>> 0;

	    rc = fwrite(hdr, 1, sizeof(hdr), stdout);
	}
	else
	{
	    error(1, "I don't understand this file at all!\n");
	}

	/*
	 * Write out the 4 bytes we read earlier
	 */
	if (iself)
	    check = docheck(check, elf, sizeof(elf));
	rc = fwrite(elf, 1, sizeof(elf), stdout);

	/*
	 * Write out the remainder of the file
	 */
	while ( (len = fread(buf, 1, sizeof(buf), fp)) )
	{
	    if (iself)
		check = docheck(check, buf, len);
	    rc = fwrite(buf, 1, len, stdout);
	}

	fclose(fp);

	/*
	 * Add the file trailer
	 */
	if (iself)
	{
	    /*
	     * Add in the checksum carries and complement it
	     */
	    while (check >> 16)
		check = (check&0xffff) + (check>>16);
	    check = ~check;

	    putchar(0xff);
	    putchar(0xff);
	    putchar((check >> 0) & 0xff);
	    putchar((check >> 8) & 0xff);
	}
	if (ispjl)
	    printf("\033%%-12345X");

	exit(0);
}
END
  (cd /usr/share/foo2zjs/firmware && bash pts-arm2hpdl.c) || exit 1
  /usr/share/foo2zjs/firmware/pts-arm2hpdl /usr/share/foo2zjs/firmware/sihp1018.img >/usr/share/foo2zjs/firmware/sihp1018.dl
  rm -f /usr/share/foo2zjs/firmware/{sihp1018.img,pts-arm2hpdl.c,pts-arm2phdl}
  set +ex
  if test "90cce767b25149433024a8307d7320fbd4ead879  -" != "`sha1sum </usr/share/foo2zjs/firmware/sihp1018.dl`"; then
    echo "$0: error: bad new firmware file, giving up: /usr/share/foo2zjs/firmware/sihp1018.dl"
    exit 4
  fi
fi

set -ex
"$CUPSYS" stop
perl -pi -0777 -e '
    s@^<(?:Default)?Printer HP_LaserJet_1018_pts>.*?\n</Printer>[ \t]*\n@@msg;
    s@^<DefaultPrinter @<Printer @mg;
    s@\s+\Z(?!\n)@\n@' /etc/cups/printers.conf
cat >>/etc/cups/printers.conf <<'END'
<DefaultPrinter HP_LaserJet_1018_pts>
Info Hewlett-Packard HP LaserJet 1018
MakeModel HP LaserJet 1018 Foomatic/foo2zjs (recommended)
DeviceURI usb://HP/LaserJet%201018
State Idle
StateTime 1276502574
Type 8425476
Filter application/vnd.cups-raw 0 -
Filter application/vnd.cups-postscript 100 foomatic-rip
Filter application/vnd.cups-pdf 0 foomatic-rip
Accepting Yes
Shared Yes
JobSheets none none
QuotaPeriod 0
PageLimit 0
KLimit 0
OpPolicy default
ErrorPolicy retry-job
</Printer>
END
cat >/etc/cups/lpoptions <<'END'
Default HP_LaserJet_1018_pts PageSize=A4 InputSlot=Upper MediaType=Envelope collate=false outputorder=normal number-up-layout=btlr wrap=false position=center
END
chmod 644 /etc/cups/printers.conf /etc/cups/lpoptions
if test "$SUDO_USER"; then
  SUDO_HOME="`eval "echo ~$SUDO_USER"`"
  # TODO(pts): Remove for more users?
  rm -f "$SUDO_HOME/.lpoptions" "$SUDO_HOME/.cups/lpoptions"
fi

cp /etc/cups/ppd/HP_LaserJet_1018_pts.ppd{,.bak.`date '+%F_%T'`}
# MANUAL: Change A4 to Letter below for letter default paper size.
gzip -cd </usr/share/ppd/foo2zjs/HP-LaserJet_1018.ppd.gz |
    perl -pe 's@: \w+[ \t]*$@: A4@ if m@^[*]Default@' \
    >/etc/cups/ppd/HP_LaserJet_1018_pts.ppd
chmod 644 /etc/cups/ppd/HP_LaserJet_1018_pts.ppd

# Change the ATTRS{product}=="HP LaserJet 1018" udev match to
# ATTRS{idProduct}==4117, because Ubuntu Lucid lsusb has "Hewlett-Packard
# Printing Support" as the product name instead of "HP LasertJet 1018" (as
# reported by lsusb(1)).
perl -pi -0777 -e '
    s@\s+\Z(?!\n)@\n@;
    $_ .= "RUN+=\"/usr/sbin/hplj1018\"\n" if !m@RUN[+]=\"/usr/sbin/hplj1018\"@;
    s@^[^#\n].*,\s*RUN[+]=\"/usr/sbin/hplj1018\".*@KERNEL=="lp*", SUBSYSTEMS=="usb", ATTRS{idVendor}=="03f0", ATTRS{idProduct}=="4117", SYMLINK+="hplj1018-%n", RUN+="/usr/sbin/hplj1018"@mg;
    ' "$RULES"

if test -e /dev/usb/lp0; then
  set +ex
  echo "Please turn off the printer!"
  while test -e /dev/usb/lp0; do sleep 1; done
  echo "OK"
  set -ex
fi
"$CUPSYS" start
cancel HP_LaserJet_1018_pts || true
"$CUPSYS" stop

if test ! -e /dev/usb/lp0; then
  set +ex
  echo "Please turn on the printer and connect it!"
  while test ! -e /dev/usb/lp0; do sleep 1; done
  echo "OK"
  set -ex
  sleep 3  # wait for the firmware being downloaded
fi

"$CUPSYS" start
cupsenable HP_LaserJet_1018_pts
test "`lpq -P HP_LaserJet_1018_pts |
    grep '^HP_LaserJet_1018_pts is ready$'`"
test "`lpq -P HP_LaserJet_1018_pts |
    grep '^no entries$'`"
sleep 10  # Wait for the firmware being uploaded by udev ($RULES).

: All OK
