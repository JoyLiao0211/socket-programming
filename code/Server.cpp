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
    pthread_mutex_t socket_mutex;
    int locking_line;//line number of the last lock

    Client(int _socket) : socket(_socket) {
        pthread_mutex_init(&socket_mutex, nullptr);
    }
    // ~Client() { pthread_mutex_destroy(&socket_mutex); }
};

struct User {
    string username, password;
    bool online;
    Client* client;
};

vector<User> users;

#ifdef DEBUG
void init_debug_users(){
    users = {
        {"j", "j", false, nullptr},
        {"o", "o", false, nullptr},
        {"y", "y", false, nullptr}
    };
}
#endif

// A helper function to get the username for debug printing
static inline string getClientName(const Client &client) {
    // If the client has a valid uid, return that user's username; otherwise, "unknown"
    if (client.uid >= 0 && client.uid < (int)users.size()) {
        return users[client.uid].username;
    }
    return "unknown";
}

// A global queue of pointers to Clients
queue<Client*> client_queue;
vector<pthread_t> worker_threads;
const int NUM_WORKERS = 4;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  queue_cond  = PTHREAD_COND_INITIALIZER;

// ----------------------- Utility Functions ------------------------
void client_logout(Client &client) {
    if (client.uid != -1) {
        users[client.uid].online = false;
        users[client.uid].client = nullptr;
        client.uid = -1;
    }
}

void client_disconnect(Client &client) {
    client_logout(client);

    // // ----- LOCK (with debug) -----
    // {
    //     auto user_name = getClientName(client);
    //     cerr << "[DEBUG] Locking socket_mutex for user: " << user_name
    //          << " at line " << __LINE__ << endl;
    //     pthread_mutex_lock(&client.socket_mutex);
    //     cerr << "[DEBUG] Locked socket_mutex for user: " << user_name
    //          << " at line " << __LINE__ << endl;
    // }
    close(client.socket);

    // // ----- UNLOCK (with debug) -----
    // {
    //     auto user_name = getClientName(client);
    //     cerr << "[DEBUG] Unlocking socket_mutex for user: " << user_name
    //          << " at line " << __LINE__ << endl;
    //     pthread_mutex_unlock(&client.socket_mutex);
    //     cerr << "[DEBUG] Unlocked socket_mutex for user: " << user_name
    //          << " at line " << __LINE__ << endl;
    // }
}

bool send_json_to_client(Client &client, const json &message, bool locked=false) {
    if (!locked) {
        auto user_name = getClientName(client);
        cerr << "[DEBUG] Locking socket_mutex for user: " << user_name
             << " at line " << __LINE__ << endl;
        pthread_mutex_lock(&client.socket_mutex);
        cerr << "[DEBUG] Locked socket_mutex for user: " << user_name
             << " at line " << __LINE__ << endl;
    }

    bool status = send_json(client.socket, message);

    if (!locked) {
        auto user_name = getClientName(client);
        cerr << "[DEBUG] Unlocking socket_mutex for user: " << user_name
             << " at line " << __LINE__ << endl;
        pthread_mutex_unlock(&client.socket_mutex);
        cerr << "[DEBUG] Unlocked socket_mutex for user: " << user_name
             << " at line " << __LINE__ << endl;
    }

    if (!status) {
        client_disconnect(client);
        return false;
    }
    return true;
}

// ----------------------- Request Handlers ------------------------
void handle_Login(Client &client, json &request){
    json response;
    string username = request["username"];
    string password = request["password"];
    for (auto &user : users) {
        if (user.username == username) {
            if (user.password == password) {
                // Already logged in?
                if (client.uid != -1 || user.online) {
                    response["type"] = "Login";
                    response["code"] = 1; // Already logged in
                    send_json_to_client(client, response);
                    return;
                }
                client.uid = &user - &users[0];
                user.online = true;
                user.client = &client;
                response["type"] = "Login";
                response["code"] = 0;  // Success
                send_json_to_client(client, response);
                return;
            }
            response["type"] = "Login";
            response["code"] = 2; // Incorrect password
            send_json_to_client(client, response);
            return;
        }
    }
    response["type"] = "Login";
    response["code"] = 3; // Username not found
    send_json_to_client(client, response);
}

void handle_direct_connect(Client &client, json &request){
    string recipient = request["username"];
    for (auto &receiving_user : users) {
        if (receiving_user.username == recipient && receiving_user.online) {
            // Prepare message for the receiving user
            json message_json;
            message_json["type"]     = "DirectConnectRequest";
            message_json["username"] = users[client.uid].username;
            message_json["IP"]       = request["IP"];
            message_json["port"]     = request["port"];

            cerr << "[DEBUG] Locking socket_mutex for receiving_user: " 
                 << receiving_user.username << " at line " << __LINE__ << endl;
            pthread_mutex_lock(&receiving_user.client->socket_mutex);
            cerr << "[DEBUG] Locked socket_mutex for receiving_user: "
                 << receiving_user.username << " at line " << __LINE__ << endl;

            if (!send_json_to_client(*(receiving_user.client), message_json, true)) {
                cerr << "[DEBUG] Unlocking socket_mutex for receiving_user: "
                     << receiving_user.username << " at line " << __LINE__ << endl;
                pthread_mutex_unlock(&receiving_user.client->socket_mutex);
                cerr << "[DEBUG] Unlocked socket_mutex for receiving_user: "
                     << receiving_user.username << " at line " << __LINE__ << endl;
                break;
            }

            // get_json uses read internally
            json recipient_response = get_json(receiving_user.client->socket);

            cerr << "[DEBUG] Unlocking socket_mutex for receiving_user: "
                 << receiving_user.username << " at line " << __LINE__ << endl;
            pthread_mutex_unlock(&receiving_user.client->socket_mutex);
            cerr << "[DEBUG] Unlocked socket_mutex for receiving_user: "
                 << receiving_user.username << " at line " << __LINE__ << endl;

            // If the receiving user refused or returned an error
            if (recipient_response["code"] != 0) {
                json response;
                response["type"] = "DirectConnect";
                response["code"] = recipient_response["code"];
                send_json_to_client(client, response);
                return;
            }

            // otherwise success
            json response;
            response["type"] = "DirectConnect";
            response["code"] = 0;
            response["IP"]   = recipient_response["IP"];
            response["port"] = recipient_response["port"];
            send_json_to_client(client, response);
            return;
        }
    }
    // If we get here, recipient not found or not online
    json response;
    response["type"] = "DirectConnect";
    response["code"] = 7; // Recipient not found
    send_json_to_client(client, response);
}

void handle_client_message(Client &client) {
    cout << "Received message: " << client.message << endl;
    json request = json::parse(client.message);

    if (request["type"] == "Login") {
        handle_Login(client, request);
    }
    else if (request["type"] == "Logout") {
        client_logout(client);
        json response;
        response["type"] = "Logout";
        response["code"] = 0; // success
        send_json_to_client(client, response);
    }
    else if (request["type"] == "Register") {
        string username = request["username"];
        string password = request["password"];
        for (auto &user : users) {
            if (user.username == username) {
                json response;
                response["type"] = "Register";
                response["code"] = 4; // Username already exists
                send_json_to_client(client, response);
                return;
            }
        }
        // register
        users.push_back({username, password, false, nullptr});
        json response;
        response["type"] = "Register";
        response["code"] = 0; // success
        send_json_to_client(client, response);
    }
    else if (request["type"] == "OnlineUsers") {
        json response;
        response["type"] = "OnlineUsers";
        response["code"] = 0;
        vector<string> online_users;
        for (auto &user : users) {
            if (user.online) {
                online_users.push_back(user.username);
            }
        }
        response["users"] = online_users;
        send_json_to_client(client, response);
    }
    else if (request["type"] == "SendMessage") {
        if (client.uid == -1) {
            json response;
            response["type"] = "SendMessage";
            response["code"] = 6; // not logged in
            send_json_to_client(client, response);
            return;
        }
        string recipient = request["username"];
        string message   = request["message"];
        for (auto &user : users) {
            if (user.username == recipient && user.online) {
                json message_json;
                message_json["type"]    = "NewMessage";
                message_json["from"]    = users[client.uid].username;
                message_json["message"] = message;
                if (!send_json_to_client(*(user.client), message_json)) {
                    break;
                }
                json response;
                response["type"] = "SendMessage";
                response["code"] = 0;  // success
                send_json_to_client(client, response);
                return;
            }
        }
        json response;
        response["type"] = "SendMessage";
        response["code"] = 7;  // recipient not found
        send_json_to_client(client, response);
    }
    else if (request["type"] == "DirectConnect") {
        if (client.uid == -1) {
            json response;
            response["type"] = "DirectConnect";
            response["code"] = 6;  // not logged in
            send_json_to_client(client, response);
            return;
        }
        handle_direct_connect(client, request);
    }
    else {
        // invalid command
        json response;
        response["type"] = "INVALID";
        response["code"] = 5;
        send_json_to_client(client, response);
    }
}

// ----------------------- The Worker Function ------------------------
void* worker_function(void* arg) {
    while (true) {
        // Wait until there's a client in the queue
        pthread_mutex_lock(&queue_mutex);
        while (client_queue.empty()) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }
        // Pop a pointer from the queue
        Client* client = client_queue.front();
        client_queue.pop();
        pthread_mutex_unlock(&queue_mutex);

        // -- LOCK (with debug) --
        {
            auto user_name = getClientName(*client);
            cerr << "[DEBUG] Attempting lock for user: " << user_name
                 << " at line " << __LINE__
                 << ". Last locker line was " << client->locking_line << endl;

            pthread_mutex_lock(&client->socket_mutex);
            client->locking_line = __LINE__;  // we locked
            cerr << "[DEBUG] Locked socket_mutex for user: " << user_name
                 << " at line " << __LINE__ << endl;
        }

        // Read the message from the client
        json request=get_json(client->socket, 1);//non-blockingly read the whole json
        // -- UNLOCK (with debug) --
        {
            auto user_name = getClientName(*client);
            cerr << "[DEBUG] Unlocking socket_mutex for user: " << user_name
                    << " at line " << __LINE__ << endl;
            pthread_mutex_unlock(&client->socket_mutex);
            cerr << "[DEBUG] Unlocked socket_mutex for user: " << user_name
                    << " at line " << __LINE__ << endl;
        }

        if(!request.empty()){
            client->message = request.dump();

            // Now handle the message
            handle_client_message(*client);
        }


        // Re-queue the same client for next read
        pthread_mutex_lock(&queue_mutex);
        client_queue.push(client);
        pthread_mutex_unlock(&queue_mutex);
        pthread_cond_signal(&queue_cond);
    }
    return nullptr;
}


// ----------------------- main() ------------------------
int main() {
#ifdef DEBUG
    init_debug_users();
#endif

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        cerr << "Socket creation error!\n";
        return -1;
    }

    sockaddr_in address = {0};
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(8080);

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
        if (new_socket < 0) {
            cerr << "Accept failed!\n";
            continue;
        }

        Client* new_client = new Client(new_socket);

        pthread_mutex_lock(&queue_mutex);
        client_queue.push(new_client);
        pthread_mutex_unlock(&queue_mutex);
        pthread_cond_signal(&queue_cond);
    }

    for (auto &thread : worker_threads) {
        pthread_join(thread, nullptr);
    }
    close(server_fd);
    return 0;
}

// ----------------------- Optional Debug Helper ------------------------
#include <iostream>
void print_json_dump(const string &json_dump) {
    cout << "Received JSON: " << json_dump << endl;
}
