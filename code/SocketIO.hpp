#ifndef SOCKET_IO_HPP
#define SOCKET_IO_HPP

#include <unistd.h>
#include <errno.h>
#include <cstddef>

// Function to read exactly n bytes from a descriptor
inline ssize_t readn(int fd, void *vptr, size_t n) {
    size_t nleft = n;
    ssize_t nread;
    char *ptr = (char *)vptr;

    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) <= 0) {
            if (nread == -1 && errno == EINTR)
                nread = 0; // Call read() again
            else
                return -1; // Error or EOF
        }
        nleft -= nread;
        ptr += nread;
    }
    return n;
}

// Function to write exactly n bytes to a descriptor
inline ssize_t writen(int fd, const void *vptr, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    const char *ptr = (const char *)vptr;

    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten == -1 && errno == EINTR)
                nwritten = 0; // Call write() again
            else
                return -1; // Error
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}

#endif // SOCKET_UTILS_HPP
