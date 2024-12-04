#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "Parse.h"
#include <errno.h>
#include <cstdint>
using namespace std;

struct User {
    string username, password;
};

struct Client {
    int uid = -1;
    string message = "";
    int socket;

    Client(int _socket) : socket(_socket) {}
};

vector<User> users;
queue<Client> client_queue;
vector<pthread_t> worker_threads;
const int NUM_WORKERS = 4;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

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

string handle_client_message(Client &client, const string &message) {
    // Parse message using `$` as the delimiter
    auto tokens = parse_message(message, '$');
    if (tokens.empty()) return "$5$";  // 1 indicates "Invalid command"

    // Check command type based on the first token
    if (tokens[0] == "1" && tokens.size() >= 3) {
        // Login request
        string username = tokens[1];
        string password = tokens[2];

        // Verify credentials
        for (int i = 0; i < users.size(); ++i) {
            if (users[i].username == username) {
                if (users[i].password == password) {
                    if (client.uid != -1) {
                        cerr << "LOGIN FAIL: Already logged in\n";
                        return "$1$";  // Code 1: Already logged in
                    }
                    client.uid = i;  // Set client's user ID to logged-in user
                    return "$0$";  // Code 0: Login success
                } else {
                    cerr << "LOGIN FAIL: Incorrect password\n";
                    return "$2$";  // Code 2: Incorrect password
                }
            }
        }
        cerr << "LOGIN FAIL: Username not found\n";
        return "$3$";  // Code 3: Username not found
    } 
    else if (tokens[0] == "2" && tokens.size() == 3) {
        // Registration request
        string username = tokens[1];
        string password = tokens[2];

        // Check if the username already exists
        for (const auto &user : users) {
            if (user.username == username) {
                cerr << "REGISTER FAIL: Username already exists\n";
                return "$4$";  // Code 4: Username already exists
            }
        }
        
        // Register the new user
        users.push_back({username, password});
        return "$0$";  // Code 0: Registration success
    } 

    return "$1$";  // Code 1: Unrecognized command
}

void* worker_function(void* arg) {
    while (true) {
        Client client(-1);

        // Lock the queue and wait for a client
        pthread_mutex_lock(&queue_mutex);
        while (client_queue.empty()) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 5;  // Timeout after 5 seconds
            int ret = pthread_cond_timedwait(&queue_cond, &queue_mutex, &ts);
            if (ret == ETIMEDOUT && client_queue.empty()) {
                pthread_mutex_unlock(&queue_mutex);
                continue;
            }
        }

        // Get the next client from the queue
        client = client_queue.front();
        client_queue.pop();
        pthread_mutex_unlock(&queue_mutex);

        // Read the length of the incoming message
        uint32_t msg_length_net;
        ssize_t n = readn(client.socket, &msg_length_net, sizeof(msg_length_net));
        if (n <= 0) {
            cout << "Client disconnected.\n";
            close(client.socket);
            continue;
        }

        uint32_t msg_length = ntohl(msg_length_net);
        if (msg_length == 0) {
            cerr << "Received message with length 0. Skipping.\n";
            continue;
        }

        // Read the message body
        vector<char> buffer(msg_length);
        n = readn(client.socket, buffer.data(), msg_length);
        if (n <= 0) {
            cout << "Client disconnected.\n";
            close(client.socket);
            continue;
        }

        string message(buffer.begin(), buffer.end());

        // Process the message and prepare a response
        string response = handle_client_message(client, message);

        // Send the length of the response
        uint32_t response_length = response.size();
        uint32_t response_length_net = htonl(response_length);
        n = writen(client.socket, &response_length_net, sizeof(response_length_net));
        if (n <= 0) {
            cout << "Failed to send response length.\n";
            close(client.socket);
            continue;
        }

        // Send the response message
        n = writen(client.socket, response.c_str(), response_length);
        if (n <= 0) {
            cout << "Failed to send response message.\n";
            close(client.socket);
            continue;
        }

        cout << "Response sent to client: " << response << "\n";

        // Push the client back to the queue for further communication
        pthread_mutex_lock(&queue_mutex);
        client_queue.push(client);
        pthread_mutex_unlock(&queue_mutex);
        pthread_cond_signal(&queue_cond);
    }
    return nullptr;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        cerr << "Socket creation error! errno: " << errno << "\n";
        return -1;
    }
    cout << "Socket created successfully: " << server_fd << "\n";

    // Set socket options to allow address reuse
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "setsockopt failed! errno: " << errno << "\n";
        close(server_fd);
        return -1;
    }

    // Server address setup
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        cerr << "Bind failed! errno: " << errno << "\n";
        close(server_fd);
        return -1;
    }
    cout << "Bind successful.\n";

    // Listen for incoming connections
    if (listen(server_fd, 10) < 0) {
        cerr << "Listen failed! errno: " << errno << "\n";
        close(server_fd);
        return -1;
    }
    cout << "Server listening on port 8080...\n";

    // Create worker threads
    worker_threads.resize(NUM_WORKERS);
    for (int i = 0; i < NUM_WORKERS; ++i) {
        pthread_create(&worker_threads[i], nullptr, worker_function, nullptr);
    }

    // Accept clients and add them to the queue
    while (true) {
        int new_socket = accept(server_fd, nullptr, nullptr);
        if (new_socket < 0) {
            cerr << "Accept failed! errno: " << errno << "\n";
            continue;
        }
        cout << "New client connected!\n";

        // Lock the queue and add the new client
        pthread_mutex_lock(&queue_mutex);
        client_queue.emplace(new_socket);
        pthread_mutex_unlock(&queue_mutex);

        // Signal a worker thread that a client is available
        pthread_cond_broadcast(&queue_cond);
    }

    // Clean up
    for (pthread_t &thread : worker_threads) {
        pthread_join(thread, nullptr);
    }
    close(server_fd);
    return 0;
}
