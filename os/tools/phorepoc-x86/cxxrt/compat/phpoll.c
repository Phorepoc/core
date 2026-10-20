/* phpoll.c -- OURS (Phorepoc, libphcompat): poll() over Open C's select(). Declared by overlay/poll.h. Descriptors >= FD_SETSIZE
 * or negative are skipped (negative = ignored per POSIX; too large = POLLNVAL). Timeout in ms, < 0 = wait forever. */
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stddef.h>

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    fd_set rd, wr, ex;
    int maxfd = -1, ready = 0;
    nfds_t i;
    struct timeval tv, *ptv = NULL;

    FD_ZERO(&rd);
    FD_ZERO(&wr);
    FD_ZERO(&ex);
    for (i = 0; i < nfds; i++) {
        int fd = fds[i].fd;
        fds[i].revents = 0;
        if (fd < 0)
            continue;
        if (fd >= FD_SETSIZE) {
            fds[i].revents = POLLNVAL;
            ready++;
            continue;
        }
        if (fds[i].events & (POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI))
            FD_SET(fd, &rd);
        if (fds[i].events & (POLLOUT | POLLWRBAND))
            FD_SET(fd, &wr);
        FD_SET(fd, &ex);
        if (fd > maxfd)
            maxfd = fd;
    }
    if (ready)
        return ready;
    if (timeout >= 0) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        ptv = &tv;
    }
    if (select(maxfd + 1, &rd, &wr, &ex, ptv) < 0)
        return -1;
    for (i = 0; i < nfds; i++) {
        int fd = fds[i].fd;
        if (fd < 0 || fd >= FD_SETSIZE)
            continue;
        if (FD_ISSET(fd, &rd))
            fds[i].revents |= fds[i].events & (POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI);
        if (FD_ISSET(fd, &wr))
            fds[i].revents |= fds[i].events & (POLLOUT | POLLWRBAND);
        if (FD_ISSET(fd, &ex))
            fds[i].revents |= POLLERR;
        if (fds[i].revents)
            ready++;
    }
    return ready;
}
