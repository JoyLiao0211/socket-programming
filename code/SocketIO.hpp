#ifndef SOCKET_IO_HPP
#define SOCKET_IO_HPP

#include <unistd.h>
#include <errno.h>
#include <cstddef>
#include <nlohmann/json.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>

using json = nlohmann::json;
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <vector>
#include <algorithm>

// #define std::min(a, b) ((a) < (b) ? (a) : (b))
// #define max(a, b) ((a) > (b) ? (a) : (b))

// Function to read exactly n bytes from a descriptor
inline ssize_t readn(SSL * ssl, void *vptr, size_t n) {
    size_t nleft = n;
    ssize_t nread;
    char *ptr = static_cast<char *>(vptr);

    while (nleft > 0) {
        if ((nread = SSL_read(ssl, ptr, nleft)) <= 0) {
            if (nread == -1 && errno == EINTR)
                nread = 0; // Call read() again
            else
                return -1; // Error or EOF
        }
        nleft -= nread;
        ptr   += nread;
    }
    return n;
}

// Function to write exactly n bytes to a descriptor
inline ssize_t writen(SSL *ssl, const void *vptr, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    const char *ptr = static_cast<const char *>(vptr);

    while (nleft > 0) {
        if ((nwritten = SSL_write(ssl, ptr, nleft)) <= 0) {
            if (nwritten == -1 && errno == EINTR)
                nwritten = 0; // Call write() again
            else
                return -1; // Error
        }
        nleft -= nwritten;
        ptr   += nwritten;
    }
    return n;
}

const uint32_t CHUNK_SIZE = 4096;
bool send_file(SSL *ssl, std::vector<char> &data) {
    uint32_t msg_length = static_cast<uint32_t>(data.size());
    uint32_t msg_length_net = htonl(msg_length);
    ssize_t bytes_sent = writen(ssl, &msg_length_net, sizeof(msg_length_net));
    if (bytes_sent != sizeof(msg_length_net)) {
        std::cerr << "[send_file] Error sending message length. Bytes sent: " << bytes_sent << "\n";
        return false;
    }

    int pos = 0;
    while (msg_length > 0) {
        int send_length = std::min(CHUNK_SIZE, msg_length);
        char *ptr = &data[pos];
        bytes_sent = writen(ssl, ptr, send_length);
        if (bytes_sent != static_cast<ssize_t>(send_length)) {
            std::cerr << "[send_file] Error sending file. Bytes sent: " << bytes_sent << "\n";
            return false;
        }
        msg_length -= bytes_sent;
        pos += bytes_sent;
    }
    return true;
}

bool receive_file(SSL *ssl, std::vector<char> &data) {
    uint32_t length_net;
    if (readn(ssl, &length_net, sizeof(length_net)) != sizeof(length_net)) {
        std::cerr << "[read file] Error reading message length.\n";
        return 0;
    }
    uint32_t length_host = ntohl(length_net); // Convert length from network to host byte order
    if (length_host == 0) {
        return 0;
    }
    data.resize(length_host);

    uint32_t pos = 0;
    while (pos < length_host) {
        uint32_t rcv_length = std::min(CHUNK_SIZE, length_host - pos);
        ssize_t bytes_read = readn(ssl, data.data() + pos, rcv_length);
        if (bytes_read != static_cast<ssize_t>(rcv_length)) {
            std::cerr << "[read_file] Error reading file. Bytes sent: " << bytes_read << "\n";
            return false;
        }
        pos += bytes_read;
    }
    return true;
}


bool send_json(SSL *ssl, const nlohmann::json& message) {
    // using json = nlohmann::json;
    // Serialize JSON to string
    std::string message_str = message.dump(); // or message.dump(4) if you prefer pretty print
    std::cerr<<"sending\n";
    if(message_str.length()<50)std::cerr<<message_str<<"\n";
    uint32_t msg_length = static_cast<uint32_t>(message_str.size());

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


nlohmann::json get_json(SSL *ssl, bool peek_first = false) {
    // Configure the SSL connection to blocking or non-blocking as requested
    int fd = SSL_get_fd(ssl); // Get the file descriptor linked with the SSL pointer
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        std::cerr << "[get_json] fcntl(F_GETFL) failed.\n";
        return {};
    }

    if (peek_first) {//peek if any data is in the buffer
        // std::cerr<<"peeking\n";
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);//is this neccessary? 
        char c;
        int bytes_peeked = SSL_peek(ssl, &c, 1);
        fcntl(fd, F_SETFL, flags);
        if(bytes_peeked <= 0 && (errno == EAGAIN || errno == EWOULDBLOCK)){
            // fcntl(fd, F_SETFL, flags);
            // std::cerr<<"no data yet\n";
            return {};
        }else if(bytes_peeked <= 0){
            // if(errno == EAGAIN || errno == EWOULDBLOCK){
            //     fcntl(fd, F_SETFL, flags);
            
            //     return eof_response;
            // }
            std::cerr << "[get_json] Error peeking data: EOF\n";
            json eof_response;
            eof_response["type"] = "EOF";
            return eof_response;
        }
        // else std::cerr<<"peeked "<<bytes_peeked<<" bytes\n";
        
    }

    // 1) Receive the length of the JSON message
    uint32_t length_net;
    if (readn(ssl, &length_net, sizeof(length_net)) != sizeof(length_net)) {
        std::cerr << "[get_json] Error reading message length.\n";
        return {};
    }

    uint32_t length_host = ntohl(length_net); // Convert length from network to host byte order

    if (length_host == 0) {
        // If the length is 0, it could be a protocol-specific control message
        return {};
    }

    // 2) Receive the JSON message itself
    std::vector<char> buffer(length_host);
    if (readn(ssl, buffer.data(), length_host) != static_cast<ssize_t>(length_host)) {
        std::cerr << "[get_json] Error reading JSON payload.\n";
        return {};
    }

    // 3) Parse the JSON
    try {
        //std::cout << "Received:\n" << std::string(buffer.begin(), buffer.end()) << "\n";
        return nlohmann::json::parse(buffer.begin(), buffer.end());
    } catch (const nlohmann::json::parse_error &e) {
        std::cerr << "[get_json] JSON parse error: " << e.what() << "\n";
        return {};
    }
}

#endif // SOCKET_IO_HPP
