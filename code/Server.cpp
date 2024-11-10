#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>    // For close()
#include <arpa/inet.h> // For inet_ntoa()
#include "Parse.h"
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
vector<Client> clients;

string handle_client_message(Client &client, const string &message) {
    // Parse message using `$` as the delimiter
    auto tokens = parse_message(message, '$');
    if (tokens.empty()) return "$1$";  // 1 indicates "Invalid command"

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

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        cerr << "Socket creation error! errno: " << errno << "\n";
        return -1;
    }
    cout << "Socket created successfully: " << server_fd << "\n";

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

    fd_set read_fds;
    
    while (true) {
        // Reset max_sd and file descriptor set
        int max_sd = server_fd;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);

        // Add client sockets to the set and update max_sd
        for (const Client &client : clients) {
            FD_SET(client.socket, &read_fds);
            max_sd = max(max_sd, client.socket);
        }

        // Wait for an activity on one of the sockets
        int activity = select(max_sd + 1, &read_fds, nullptr, nullptr, nullptr);
        if ((activity < 0) && (errno != EINTR)) {
            cerr << "select error" << endl;
        }

        // Check if there is a new incoming connection
        if (FD_ISSET(server_fd, &read_fds)) {
            int new_socket = accept(server_fd, nullptr, nullptr);
            if (new_socket < 0) {
                cerr << "Accept failed! errno: " << errno << "\n";
            } else {
                cout << "New client connected!\n";
                clients.emplace_back(new_socket);
            }
        }

        // Iterate over clients to check for incoming messages
        for (auto it = clients.begin(); it != clients.end(); ) {
            Client &client = *it;
            
            // Check if there is data to read from this client
            if (FD_ISSET(client.socket, &read_fds)) {
                char buffer[1024] = {0};
                int bytes_received = read(client.socket, buffer, sizeof(buffer) - 1);

                // If the client has disconnected, remove it from the list
                if (bytes_received <= 0) {
                    cout << "Client disconnected.\n";
                    close(client.socket);
                    it = clients.erase(it);  // Move iterator to the next element
                    continue;  // Skip the increment as erase() has updated the iterator
                }

                // Handle client message
                string response = handle_client_message(client, buffer);
                send(client.socket, response.c_str(), response.size(), 0);
                cout << "Response sent to client: " << response << "\n";
            }
            
            // Increment the iterator only if no client was erased
            ++it;
        }
    }

    close(server_fd);
    return 0;
}
