#ifndef SOCKET_IO_HPP
#define SOCKET_IO_HPP

#include <unistd.h>
#include <errno.h>
#include <cstddef>
#include <nlohmann/json.hpp>

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

bool send_json(int socket_fd, const nlohmann::json& message) {
    // using json = nlohmann::json;
    // Serialize JSON to string
    std::string message_str = message.dump(4);
    uint32_t msg_length = message_str.size();

    // Convert message length to network byte order
    uint32_t msg_length_net = htonl(msg_length);

    // Send the length of the JSON message
    ssize_t bytes_sent = writen(socket_fd, &msg_length_net, sizeof(msg_length_net));
    if (bytes_sent != sizeof(msg_length_net)) {
        std::cerr << "[send_json] Error sending message length. Bytes sent: " << bytes_sent << "\n";
        return false;
    }

    // Send the actual JSON message
    bytes_sent = writen(socket_fd, message_str.c_str(), msg_length);
    if (bytes_sent != static_cast<ssize_t>(msg_length)) {
        std::cerr << "[send_json] Error sending JSON message. Bytes sent: " << bytes_sent << "\n";
        return false;
    }

    return true;
}

nlohmann::json get_json(int socket_fd) {
    uint32_t msg_length_net;
    ssize_t bytes_received = readn(socket_fd, &msg_length_net, sizeof(msg_length_net));
    if (bytes_received != sizeof(msg_length_net)) {
        std::cerr << "[recv_json] Error receiving message length. Bytes received: " << bytes_received << "\n";
        return {};
    }

    // Convert message length to host byte order
    uint32_t msg_length = ntohl(msg_length_net);

    // Receive the actual JSON message
    std::vector<char> buffer(msg_length);
    bytes_received = readn(socket_fd, buffer.data(), msg_length);
    if (bytes_received != static_cast<ssize_t>(msg_length)) {
        std::cerr << "[recv_json] Error receiving JSON message. Bytes received: " << bytes_received << "\n";
        return {};
    }

    // Parse the received JSON message
    std::string message_str(buffer.begin(), buffer.end());
    return nlohmann::json::parse(message_str);
}

#endif // SOCKET_UTILS_HPP
