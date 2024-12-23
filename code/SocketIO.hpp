#ifndef SOCKET_IO_HPP
#define SOCKET_IO_HPP

#include <unistd.h>
#include <errno.h>
#include <cstddef>
#include <nlohmann/json.hpp>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <vector>

// read exactly n bytes
inline ssize_t readn(int fd, void *vptr, size_t n) {
    size_t nleft = n;
    ssize_t nread;
    char *ptr = static_cast<char *>(vptr);

    while (nleft > 0) {
        if ((nread = ::read(fd, ptr, nleft)) <= 0) {
            if (nread == -1 && errno == EINTR) {
                // Interrupted by signal -- retry
                nread = 0;
            } else {
                // Error or EOF
                return -1;
            }
        }
        nleft -= nread;
        ptr   += nread;
    }
    return n;
}

// write exactly n bytes
inline ssize_t writen(int fd, const void *vptr, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    const char *ptr = static_cast<const char *>(vptr);

    while (nleft > 0) {
        if ((nwritten = ::write(fd, ptr, nleft)) <= 0) {
            if (nwritten == -1 && errno == EINTR) {
                nwritten = 0;
            } else {
                return -1;
            }
        }
        nleft -= nwritten;
        ptr   += nwritten;
    }
    return n;
}

inline bool send_json(int socket_fd, const nlohmann::json& message) {
    // Serialize JSON to string
    std::string message_str = message.dump(); // or message.dump(4) if you prefer pretty print
    std::cerr<<"sending\n"<<message_str<<"\n";
    uint32_t msg_length = static_cast<uint32_t>(message_str.size());

    // Convert message length to network byte order
    uint32_t msg_length_net = htonl(msg_length);

    // Send the length
    if (writen(socket_fd, &msg_length_net, sizeof(msg_length_net)) != sizeof(msg_length_net)) {
        std::cerr << "[send_json] Error sending length.\n";
        return false;
    }

    // Send the actual JSON data
    if (writen(socket_fd, message_str.data(), msg_length) != static_cast<ssize_t>(msg_length)) {
        std::cerr << "[send_json] Error sending JSON message.\n";
        return false;
    }

    return true;
}

/**
 * @brief Read a JSON message from the socket.
 *
 * If `non_blocking` is false, this function blocks until the full JSON message is read
 * (length + payload).
 *
 * If `non_blocking` is true, it returns immediately if there's not enough data
 * in the buffer to form the entire message (all 4 bytes of length + payload).
 * Otherwise, it reads the entire message without blocking (since we confirm
 * via MSG_PEEK that it's already in the buffer).
 *
 * @param socket_fd       The socket file descriptor.
 * @param non_blocking    If true, do an “all or nothing” non-blocking read.
 * @return nlohmann::json An empty JSON object if incomplete data or error,
 *                        or the parsed JSON if successful.
 */
inline nlohmann::json get_json(int socket_fd, bool non_blocking = false) {
    // 1) Configure the socket to blocking or non-blocking as requested
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        std::cerr << "[get_json] fcntl(F_GETFL) failed.\n";
        return {};
    }

    if (non_blocking) {
        // Enable non-blocking mode
        if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            std::cerr << "[get_json] fcntl(F_SETFL, O_NONBLOCK) failed.\n";
            return {};
        }
    } else {
        // Disable non-blocking mode
        if (fcntl(socket_fd, F_SETFL, flags & ~O_NONBLOCK) < 0) {
            std::cerr << "[get_json] fcntl(F_SETFL, ~O_NONBLOCK) failed.\n";
            return {};
        }
    }

    // 2) If non_blocking == true, do the “all or nothing” approach using MSG_PEEK
    if (non_blocking) {
        // Peek 4 bytes for length
        uint32_t length_net;
        ssize_t peeked = ::recv(socket_fd, &length_net, sizeof(length_net),
                                MSG_PEEK | MSG_DONTWAIT);
        if (peeked < 0) {
            // If there's no data (EAGAIN/EWOULDBLOCK), or a real error
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available
                return {};
            }
            // Another error
            std::cerr << "[get_json] MSG_PEEK error, errno=" << errno << "\n";
            return {};
        } else if (peeked < static_cast<ssize_t>(sizeof(length_net))) {
            // Not enough bytes to even read the length
            return {};
        }

        // Convert to host byte order, but still in kernel buffer
        uint32_t length_host = ntohl(length_net);
        if (length_host == 0) {
            // Could be a protocol that sends length=0 as a heartbeat or something
            // We'll just return {}
            return {};
        }

        // Now peek the entire (length + 4) to ensure it's all there
        std::vector<char> peek_buf(sizeof(length_net) + length_host);
        peeked = ::recv(socket_fd, peek_buf.data(), peek_buf.size(),
                        MSG_PEEK | MSG_DONTWAIT);
        if (peeked < static_cast<ssize_t>(peek_buf.size())) {
            // Not all data is ready yet -> return empty (immediate, non-blocking)
            return {};
        }

        // 3) We know the entire message is there, so do actual reads now
        //    These reads won't block because the kernel says it's all in the buffer

        // Read length (4 bytes)
        if (readn(socket_fd, &length_net, sizeof(length_net)) != sizeof(length_net)) {
            std::cerr << "[get_json] Error reading length.\n";
            return {};
        }
        length_host = ntohl(length_net);

        // Now read the JSON payload
        std::vector<char> buffer(length_host);
        if (readn(socket_fd, buffer.data(), length_host) != static_cast<ssize_t>(length_host)) {
            std::cerr << "[get_json] Error reading JSON payload.\n";
            return {};
        }

        // Parse the JSON
        try {
            return nlohmann::json::parse(buffer.begin(), buffer.end());
        } catch (const nlohmann::json::parse_error &e) {
            std::cerr << "[get_json] JSON parse error: " << e.what() << "\n";
            return {};
        }

    } else {
        // 4) Blocking mode
        //    We just read 4 bytes (length), then read the payload, blocking if needed.

        uint32_t length_net;
        if (readn(socket_fd, &length_net, sizeof(length_net)) != sizeof(length_net)) {
            // Could be EOF or error
            std::cerr << "[get_json] Blocking read length failed.\n";
            return {};
        }
        uint32_t length_host = ntohl(length_net);
        if (length_host == 0) {
            // Edge case: length is zero
            return {};
        }

        std::vector<char> buffer(length_host);
        if (readn(socket_fd, buffer.data(), length_host) != static_cast<ssize_t>(length_host)) {
            std::cerr << "[get_json] Blocking read payload failed.\n";
            return {};
        }

        try {
            std::cout<<"received:\n"<<std::string(buffer.begin(), buffer.end())<<"\n";
            return nlohmann::json::parse(buffer.begin(), buffer.end());
        } catch (const nlohmann::json::parse_error &e) {
            std::cerr << "[get_json] JSON parse error: " << e.what() << "\n";
            return {};
        }
    }
}

#endif // SOCKET_IO_HPP
