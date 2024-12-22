#ifndef SOCKET_IO_HPP
#define SOCKET_IO_HPP

#include <unistd.h>
#include <errno.h>
#include <cstddef>
#include <nlohmann/json.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>

using json = nlohmann::json;

// Function to read exactly n bytes from a descriptor
inline ssize_t readn(SSL * ssl, void *vptr, size_t n) {
    size_t nleft = n;
    ssize_t nread;
    char *ptr = (char *)vptr;

    while (nleft > 0) {
        if ((nread = SSL_read(ssl, ptr, nleft)) <= 0) {
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
inline ssize_t writen(SSL *ssl, const void *vptr, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    const char *ptr = (const char *)vptr;

    while (nleft > 0) {
        if ((nwritten = SSL_write(ssl, ptr, nleft)) <= 0) {
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

bool send_json(SSL *ssl, const nlohmann::json& message) {
    // using json = nlohmann::json;
    // Serialize JSON to string
    std::string message_str = message.dump(4);
    uint32_t msg_length = message_str.size();

    // Convert message length to network byte order
    uint32_t msg_length_net = htonl(msg_length);

    // Send the length of the JSON message
    ssize_t bytes_sent = writen(ssl, &msg_length_net, sizeof(msg_length_net));
    if (bytes_sent != sizeof(msg_length_net)) {
        std::cerr << "[send_json] Error sending message length. Bytes sent: " << bytes_sent << "\n";
        return false;
    }

    // Send the actual JSON message
    bytes_sent = writen(ssl, message_str.c_str(), msg_length);
    if (bytes_sent != static_cast<ssize_t>(msg_length)) {
        std::cerr << "[send_json] Error sending JSON message. Bytes sent: " << bytes_sent << "\n";
        return false;
    }

    return true;
}

#endif // SOCKET_UTILS_HPP
