#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "Code.h"
#include "Parse.h"
#include <errno.h>
#include <cstdint>
using namespace std;

// Function to read exactly n bytes from a descriptor
ssize_t readn(int fd, void *vptr, size_t n) {
    size_t  nleft;
    ssize_t nread;
    char   *ptr;

    ptr = (char*)vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nread = read(fd, ptr, nleft)) <= 0) {
            if (nread == -1 && errno == EINTR)
                nread = 0;      // and call read() again
            else
                return -1;
        }

        nleft -= nread;
        ptr   += nread;
    }
    return n;
}

// Function to write exactly n bytes to a descriptor
ssize_t writen(int fd, const void *vptr, size_t n) {
    size_t      nleft;
    ssize_t     nwritten;
    const char *ptr;

    ptr = (const char*)vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten == -1 && errno == EINTR)
                nwritten = 0;   // and call write() again
            else
                return -1;
        }

        nleft -= nwritten;
        ptr   += nwritten;
    }
    return n;
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cerr << "Socket creation error! errno: " << errno << "\n";
        return -1;
    }
    cout << "Socket created successfully: " << sock << "\n";

    // Server address setup
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);  // Port to connect to (convert to network byte order)
    
    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        cerr << "Invalid address / Address not supported\n";
        close(sock);
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cerr << "Connection Failed! errno: " << errno << "\n";
        close(sock);
        return -1;
    }
    cout << "Connected to server!\n";
    bool logged_in = false;
    string username;

    // Client interaction loop
    while (true) {
        if (logged_in){
            cout << "Logged in as " << username << "!\n";
        }
        else{
            cout << "Enter 1 to log in\nEnter 2 to register\n";
        }
        cout << "Enter 0 to close app\n";
        string cmd;
        cin >> cmd;

        if (cmd == "0") {
            cout << "Exiting...\n";
            break;
        }

        string usr, pwd;
        string message;

        if (cmd == "1") {
            // Login request
            cout << "Username: ";
            cin >> usr;
            cout << "Password: ";
            cin >> pwd;

            // Format message as "$1$<username>$<password>$"
            message = "$1$" + usr + "$" + pwd + "$";
        }
        else if (cmd == "2") {
            // Registration request
            cout << "Username: ";
            cin >> usr;
            cout << "Password: ";
            cin >> pwd;

            // Format message as "$2$" + username + "$" + password + "$"
            message = "$2$" + usr + "$" + pwd + "$";
        }
        else {
            cout << "Invalid command. Try again.\n";
            continue;
        }

        // Send the length of the message
        uint32_t msg_length = message.size();
        uint32_t msg_length_net = htonl(msg_length);
        ssize_t n = writen(sock, &msg_length_net, sizeof(msg_length_net));
        if (n <= 0) {
            cerr << "Failed to send message length. errno: " << errno << "\n";
            break;
        }

        // Send the message body
        n = writen(sock, message.c_str(), msg_length);
        if (n <= 0) {
            cerr << "Failed to send message body. errno: " << errno << "\n";
            break;
        }

        // Receive a response from the server
        // Read the length of the response
        uint32_t resp_length_net;
        n = readn(sock, &resp_length_net, sizeof(resp_length_net));
        if (n <= 0) {
            cout << "No response from server or connection lost.\n";
            break;
        }
        uint32_t resp_length = ntohl(resp_length_net);

        // Read the response body
        vector<char> resp_buffer(resp_length);
        n = readn(sock, resp_buffer.data(), resp_length);
        if (n <= 0) {
            cout << "No response from server or connection lost.\n";
            break;
        }
        string response(resp_buffer.begin(), resp_buffer.end());

        // Process the response
        vector<string> mes = parse_message(response);
        for (string s : mes) {
            cout << "Message received from server: " << RESPONSE_MESSAGES[stoi(s)] << "\n";
            if (cmd == "1" && stoi(s) == 0) {
                logged_in = true;
                username = usr;
            }
        }
    }

    // Close the socket
    close(sock);
    cout << "Disconnected from server.\n";
    return 0;
}
