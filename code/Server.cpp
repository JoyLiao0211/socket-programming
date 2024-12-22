#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <nlohmann/json.hpp>
#include "SocketIO.hpp"

using namespace std;
using json = nlohmann::json;

struct Client {
    int uid = -1;
    string message = "";
    int socket;

    Client(int _socket) : socket(_socket) {}
};

struct User {
    string username, password;
    bool online;
    Client* client;
};

vector<User> users;
queue<Client> client_queue;
vector<pthread_t> worker_threads;
const int NUM_WORKERS = 4;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

void client_logout(Client &client) {
    if (client.uid != -1) {
        users[client.uid].online = false;
        users[client.uid].client = nullptr;
        client.uid = -1;
    }
}

json handle_client_message(Client &client) {
    cout << "Received message: " << client.message << endl;
    json response;
    json request = json::parse(client.message);

    if (request["type"] == "Login") {
        string username = request["username"];
        string password = request["password"];
        for (auto &user : users) {
            if (user.username == username) {
                if (user.password == password) {
                    if (client.uid != -1) {
                        response["type"] = "Login";
                        response["code"] = 1;  // Already logged in
                        return response;
                    }
                    client.uid = &user - &users[0];
                    user.online = true;
                    user.client = &client;
                    response["type"] = "Login";
                    response["code"] = 0;  // Success
                    return response;
                }
                response["type"] = "Login";
                response["code"] = 2;  // Incorrect password
                return response;
            }
        }
        response["type"] = "Login";
        response["code"] = 3;  // Username not found
        return response;
    } else if(request["type"] == "Logout") {
        client_logout(client);
        response["type"] = "Logout";
        response["code"] = 0;  // Success
        return response;
    }
    else if (request["type"] == "Register") {
        string username = request["username"];
        string password = request["password"];
        for (const auto &user : users) {
            if (user.username == username) {
                response["type"] = "Register";
                response["code"] = 4;  // Username already exists
                return response;
            }
        }
        users.push_back({username, password, 0});
        response["type"] = "Register";
        response["code"] = 0;  // Registration success
        return response;
    } else if (request["type"] == "OnlineUsers") {
        response["type"] = "OnlineUsers";
        response["code"] = 0;
        vector<string> online_users;
        for (const auto &user : users) if(user.online) {
            online_users.push_back(user.username);
        }
        response["users"] = online_users;
        return response;
    } else if (request["type"] == "SendMessage") {
        if (client.uid == -1) {
            response["type"] = "SendMessage";
            response["code"] = 6;  // Not logged in
            return response;
        }
        string recipient = request["username"];
        string message = request["message"];
        for (const auto &user : users) {
            if (user.username == recipient && user.online) {
                // send message to recipient
                json message_json;
                message_json["type"] = "NewMessage";
                message_json["from"] = users[client.uid].username;
                message_json["message"] = message;
                if(!send_json(user.client->socket, message_json)){
                    break;
                }
                response["type"] = "SendMessage";
                response["code"] = 0;  // Success
                return response;
            }
        }
        response["type"] = "SendMessage";
        response["code"] = 7;  // Recipient not found
        return response;
    }

    response["type"] = "INVALID";
    response["code"] = 5;  // Invalid command
    return response;
}



void client_disconnect(Client &client) {
    client_logout(client);
    close(client.socket);
}

void* worker_function(void* arg) {
    while (true) {
        pthread_mutex_lock(&queue_mutex);
        while (client_queue.empty()) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }
        Client client = client_queue.front();
        client_queue.pop();
        pthread_mutex_unlock(&queue_mutex);

        uint32_t msg_length_net;
        if (readn(client.socket, &msg_length_net, sizeof(msg_length_net)) <= 0) {
            client_disconnect(client);
            continue;
        }

        uint32_t msg_length = ntohl(msg_length_net);
        vector<char> buffer(msg_length);
        if (readn(client.socket, buffer.data(), msg_length) <= 0) {
            client_disconnect(client);
            continue;
        }

        client.message = string(buffer.begin(), buffer.end());
        json response = handle_client_message(client);
        if(!send_json(client.socket, response)){
            client_disconnect(client);
            continue;
        }
        client_queue.push(client);
    }
    return nullptr;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        cerr << "Socket creation error!\n";
        return -1;
    }

    sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        cerr << "Bind failed!\n";
        return -1;
    }

    if (listen(server_fd, 10) < 0) {
        cerr << "Listen failed!\n";
        return -1;
    }

    worker_threads.resize(NUM_WORKERS);
    for (auto &thread : worker_threads) {
        pthread_create(&thread, nullptr, worker_function, nullptr);
    }

    while (true) {
        int new_socket = accept(server_fd, nullptr, nullptr);
        if (new_socket < 0) continue;

        pthread_mutex_lock(&queue_mutex);
        client_queue.emplace(new_socket);
        pthread_mutex_unlock(&queue_mutex);
        pthread_cond_signal(&queue_cond);
    }

    for (auto &thread : worker_threads) {
        pthread_join(thread, nullptr);
    }
    close(server_fd);
    return 0;
}
#include <iostream>

void print_json_dump(const string &json_dump) {
    cout << "Received JSON: " << json_dump << endl;
}