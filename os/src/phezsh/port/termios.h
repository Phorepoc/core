/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
 *
 * termios.h -- the POSIX terminal interface zsh compiles against. The P.I.P.S. libc has none; until the tty device
 * exists (the interactive rung) tcgetattr/tcsetattr answer ENOTTY, so zsh treats every fd as "not a terminal".
 * Values follow the BSD layout the libc's other headers use.
 */
#ifndef PHE_TERMIOS_H
#define PHE_TERMIOS_H
#include <sys/types.h>

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 20
#define VEOF 0
#define VEOL 1
#define VEOL2 2
#define VERASE 3
#define VWERASE 4
#define VKILL 5
#define VREPRINT 6
#define VINTR 8
#define VQUIT 9
#define VSUSP 10
#define VDSUSP 11
#define VSTART 12
#define VSTOP 13
#define VLNEXT 14
#define VDISCARD 15
#define VMIN 16
#define VTIME 17
#define VSTATUS 18

#define IGNBRK 0x001
#define BRKINT 0x002
#define IGNPAR 0x004
#define PARMRK 0x008
#define INPCK 0x010
#define ISTRIP 0x020
#define INLCR 0x040
#define IGNCR 0x080
#define ICRNL 0x100
#define IXON 0x200
#define IXOFF 0x400
#define IXANY 0x800
#define IMAXBEL 0x2000

#define OPOST 0x01
#define ONLCR 0x02
#define OXTABS 0x04
#define ONOCR 0x20
#define ONLRET 0x40

#define CSIZE 0x300
#define CS5 0x000
#define CS6 0x100
#define CS7 0x200
#define CS8 0x300
#define CSTOPB 0x400
#define CREAD 0x800
#define PARENB 0x1000
#define PARODD 0x2000
#define HUPCL 0x4000
#define CLOCAL 0x8000

#define ECHOKE 0x001
#define ECHOE 0x002
#define ECHOK 0x004
#define ECHO 0x008
#define ECHONL 0x010
#define ECHOPRT 0x020
#define ECHOCTL 0x040
#define ISIG 0x080
#define ICANON 0x100
#define IEXTEN 0x400
#define TOSTOP 0x400000
#define FLUSHO 0x800000
#define PENDIN 0x20000000
#define NOFLSH 0x80000000

#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2
#define TCIFLUSH 1
#define TCOFLUSH 2
#define TCIOFLUSH 3

#define B0 0
#define B9600 9600
#define B38400 38400

struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_cc[NCCS];
	speed_t c_ispeed;
	speed_t c_ospeed;
};

#ifdef __cplusplus
extern "C" {
#endif
int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int act, const struct termios *t);
speed_t cfgetospeed(const struct termios *t);
speed_t cfgetispeed(const struct termios *t);
int tcflush(int fd, int q);
#ifdef __cplusplus
}
#endif
#endif
