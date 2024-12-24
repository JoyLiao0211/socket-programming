#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <nlohmann/json.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "SocketIO.hpp"
#include "SSL.hpp"
#include "CreateMessage.hpp"

using namespace std;
using json = nlohmann::json;

struct Client {
    int uid = -1;
    string message = "";
    int socket;
    SSL * ssl;
    pthread_mutex_t socket_mutex;
    #ifdef DEBUG
    int locking_line;//line number of the last lock
    #endif

    Client(int _socket, SSL * _ssl) : socket(_socket), ssl(_ssl){
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
    close(client.socket);
}

bool send_json_to_client(Client &client, const json &message, bool locked=false) {
    if (!locked) {
        #ifdef DEBUG
        auto user_name = getClientName(client);
        cerr << "[DEBUG] Locking socket_mutex for user: " << user_name
             << " at line " << __LINE__ << endl;
        #endif
        pthread_mutex_lock(&client.socket_mutex);
        #ifdef DEBUG
        cerr << "[DEBUG] Locked socket_mutex for user: " << user_name
             << " at line " << __LINE__ << endl;
        #endif
    }

    bool status = send_json(client.ssl, message);

    if (!locked) {
        #ifdef DEBUG
        auto user_name = getClientName(client);
        cerr << "[DEBUG] Unlocking socket_mutex for user: " << user_name
             << " at line " << __LINE__ << endl;
        #endif
        pthread_mutex_unlock(&client.socket_mutex);
        #ifdef DEBUG
        cerr << "[DEBUG] Unlocked socket_mutex for user: " << user_name
             << " at line " << __LINE__ << endl;
        #endif
    }

    if (!status) {
        client_disconnect(client);
        return false;
    }
    return true;
}

// ----------------------- Request Handlers ------------------------
int login_response_code(Client &client, json &request){
    string username = request["username"];
    string password = request["password"];
    for (auto &user : users) {
        if (user.username == username) {
            if (user.password == password) {
                // Already logged in?
                if (client.uid != -1 || user.online) {
                    return 1;
                }
                client.uid = &user - &users[0];
                user.online = true;
                user.client = &client;
                return 0; // Success
            }
            return 2; // Incorrect password
        }
    }
    return 3; // Username not found
}

void handle_Login(Client &client, json &request){
    int code = login_response_code(client, request);
    json response = create_login_response(code);
    send_json_to_client(client, response);
}

void handle_direct_connect(Client &client, json &request){
    string recipient = request["username"];
    for (auto &receiving_user : users) {
        if (receiving_user.username == recipient && receiving_user.online) {
            // Prepare message for the receiving user
            json message_json = create_direct_connect_request_to_peer(
                users[client.uid].username,
                request["IP"],
                request["port"],
                request["passcode"]
            );

            #ifdef DEBUG
            cerr << "[DEBUG] Locking socket_mutex for receiving_user: " 
                 << receiving_user.username << " at line " << __LINE__ << endl;
            #endif
            pthread_mutex_lock(&receiving_user.client->socket_mutex);
            #ifdef DEBUG
            cerr << "[DEBUG] Locked socket_mutex for receiving_user: "
                 << receiving_user.username << " at line " << __LINE__ << endl;
            #endif

            if (!send_json_to_client(*(receiving_user.client), message_json, true)) {
                #ifdef DEBUG
                cerr << "[DEBUG] Unlocking socket_mutex for receiving_user: "
                     << receiving_user.username << " at line " << __LINE__ << endl;
                #endif
                pthread_mutex_unlock(&receiving_user.client->socket_mutex);
                #ifdef DEBUG
                cerr << "[DEBUG] Unlocked socket_mutex for receiving_user: "
                     << receiving_user.username << " at line " << __LINE__ << endl;
                #endif
                break;
            }

            // get_json uses read internally
            json recipient_response = get_json(receiving_user.client->ssl);

            #ifdef DEBUG
            cerr << "[DEBUG] Unlocking socket_mutex for receiving_user: "
                 << receiving_user.username << " at line " << __LINE__ << endl;
            #endif
            pthread_mutex_unlock(&receiving_user.client->socket_mutex);
            #ifdef DEBUG
            cerr << "[DEBUG] Unlocked socket_mutex for receiving_user: "
                 << receiving_user.username << " at line " << __LINE__ << endl;
            #endif

            // If the receiving user refused or returned an error
            if (recipient_response["code"] != 0) {
                json response = create_direct_connect_response_to_client(recipient_response["code"]);
                send_json_to_client(client, response);
                return;
            }

            // otherwise success
            json response_to_client = create_direct_connect_response_to_client(0);
            send_json_to_client(client, response_to_client);
            return;
        }
    }
    // If we get here, recipient not found or not online
    json response_to_client = create_direct_connect_response_to_client(7);
    send_json_to_client(client, response_to_client);
}

void handle_client_message(Client &client) {
    cout << "Received message: " << client.message << endl;
    json request = json::parse(client.message);

    if (request["type"] == "Login") {
        handle_Login(client, request);
        return;
    }
    else if (request["type"] == "Logout") {
        client_logout(client);
        json response = create_logout_response(0);
        send_json_to_client(client, response);
        return;
    }
    else if (request["type"] == "Register") {
        string username = request["username"];
        string password = request["password"];
        for (auto &user : users) {
            if (user.username == username) {
                json response = create_register_response(4); // username already exists
                send_json_to_client(client, response);
                return;
            }
        }
        // register
        users.push_back({username, password, false, nullptr});
        json response = create_register_response(0); // success
        send_json_to_client(client, response);
        return;
    }
    else if (request["type"] == "OnlineUsers") {
        vector<string> online_users;
        for (auto &user : users) {
            if (user.online) {
                online_users.push_back(user.username);
            }
        }
        json response = create_online_users_response(0, online_users);
        send_json_to_client(client, response);
        return;
    }
    else if (request["type"] == "SendMessage") {
        if (client.uid == -1) {
            json response = create_send_message_response(6); // not logged in
            send_json_to_client(client, response);
            return;
        }
        string recipient = request["username"];
        string message   = request["message"];
        int code=7;
        for (auto &user : users) {
            if (user.username == recipient && user.online) {
                json message_json = create_new_message(users[client.uid].username, message);
                if (!send_json_to_client(*(user.client), message_json)) {
                    break;
                }
                code=0;
            }
        }
        json response = create_send_message_response(code);
        send_json_to_client(client, response);
        return;
    }
    else if (request["type"] == "DirectConnect") {
        if (client.uid == -1) {
            json response = create_direct_connect_response_to_client(6); // not logged in
            send_json_to_client(client, response);
            return;
        }
        handle_direct_connect(client, request);
        return;
    }
    else {
        // invalid command
        json response = create_invalid_response();
        send_json_to_client(client, response);
        return;
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
            #ifdef DEBUG
            auto user_name = getClientName(*client);
            cerr << "[DEBUG] Attempting lock for user: " << user_name
                 << " at line " << __LINE__
                 << ". Last locker line was " << client->locking_line << endl;
            #endif
            pthread_mutex_lock(&client->socket_mutex);
            #ifdef DEBUG
            client->locking_line = __LINE__;  // we locked
            cerr << "[DEBUG] Locked socket_mutex for user: " << user_name
                 << " at line " << __LINE__ << endl;
            #endif
        }

        // Read the message from the client
        json request=get_json(client->ssl, 1);//non-blockingly read the whole json
        // -- UNLOCK (with debug) --
        {
            #ifdef DEBUG
            auto user_name = getClientName(*client);
            cerr << "[DEBUG] Unlocking socket_mutex for user: " << user_name
                    << " at line " << __LINE__ << endl;
            #endif
            pthread_mutex_unlock(&client->socket_mutex);
            #ifdef DEBUG
            cerr << "[DEBUG] Unlocked socket_mutex for user: " << user_name
                    << " at line " << __LINE__ << endl;
            #endif
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

    initialize_openssl();

    SSL_CTX* ctx = create_ssl_server_context();
    configure_ssl_context(ctx);

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

        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, new_socket);
        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
        } 
        Client* new_client = new Client(new_socket, ssl);

        pthread_mutex_lock(&queue_mutex);
        client_queue.emplace(new_client);
        pthread_mutex_unlock(&queue_mutex);
        pthread_cond_signal(&queue_cond);
    }

    for (auto &thread : worker_threads) {
        pthread_join(thread, nullptr);
    }
    close(server_fd);
    SSL_CTX_free(ctx);
    cleanup_openssl();
    return 0;
}
