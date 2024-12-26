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
#include "Audio.hpp"
// #include <opencv2/opencv.hpp>
#include "Video.hpp"

using namespace std;
using json = nlohmann::json;

string session_name;

const string server_data_path = "../server_data/";
const string server_audio_path = server_data_path + "audio_files/";
const string server_video_path = server_data_path + "video_files/";

struct Client {
    int uid = -1;
    string message = "";
    int socket;
    SSL * ssl;
    pthread_mutex_t socket_mutex;

    Client(int _socket, SSL * _ssl) : socket(_socket), ssl(_ssl){
        pthread_mutex_init(&socket_mutex, nullptr);
    }
    ~Client() { pthread_mutex_destroy(&socket_mutex); }
};

struct User {
    string username, password;
    bool online;
    Client* client;
};

vector<User> users;

#ifdef DEBUG
void init_debug_users(){
    for(char c='a'; c<='z'; c++){
        string username(1, c);
        string password(1, c);
        users.push_back({username, password, false, nullptr});
    }
    cerr<<"users initialized: all username of one lowercase alphabet\n";
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
        cerr << "logging out " << users[client.uid].username << endl;
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
        pthread_mutex_lock(&client.socket_mutex);
    }

    bool status = send_json(client.ssl, message);

    if (!locked) {
        pthread_mutex_unlock(&client.socket_mutex);
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

            pthread_mutex_lock(&receiving_user.client->socket_mutex);

            if (!send_json_to_client(*(receiving_user.client), message_json, true)) {
                pthread_mutex_unlock(&receiving_user.client->socket_mutex);
                break;
            }

            // get_json uses read internally
            json recipient_response = get_json(receiving_user.client->ssl);

            pthread_mutex_unlock(&receiving_user.client->socket_mutex);

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
namespace fs = std::filesystem;
vector<string> getDatabaseFiles(const string& directoryPath, string filetype) {
    vector<string> files;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(directoryPath)) {
            if (entry.is_regular_file() && entry.path().extension() == filetype) {
                files.push_back(fs::relative(entry.path(), directoryPath).string());
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << '\n';
    }
    return files;
}

void handle_audio_request(Client &client, json request) {
    string filepath = server_audio_path + request["filename"].get<string>();
    //check if filename exists: TODO
    Audio audio(filepath);
    if (!audio.initialize()) {
        cerr << "Audio initialize error\n";
        //send error to client: TODO
        return;
    }
    json response = create_audio_response(0, audio.rate, audio.channels);
    send_json_to_client(client, response);
    int num_bytes = 0;
    while (audio.read()) {
        vector<char> vec(audio.output_buffer, audio.output_buffer + audio.done);
        response = create_audio_data(vec, num_bytes, 0);
        send_json_to_client(client, response);
        num_bytes += audio.done;
    }

    response = create_audio_data(vector<char>(), num_bytes, 1);
    send_json_to_client(client, response);
}

void handle_video_request(Client &client, json request){
    string filepath = server_video_path + request["filename"].get<string>();
    if(!fs::exists(filepath)){
        json response = create_video_response(5, vector<char>(), 0, 0);
        send_json_to_client(client, response);
        return;
    }
    stream_video(client.ssl, filepath, client.socket);
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
    } else if (request["type"] == "AudioRequest") {
        if (client.uid == -1) {
            json response = create_audio_list(6, vector<string>()); // not logged in
            send_json_to_client(client, response);
            return;
        }
        if (request["filename"].get<string>() == "") {
            //first request, return list of files
            vector<string> files = getDatabaseFiles(server_audio_path, ".mp3");
            json response = create_audio_list(0, files); 
            send_json_to_client(client, response);
        } else {
            handle_audio_request(client, request); 
        }
    }
    else if(request["type"] == "VideoRequest"){
        if (client.uid == -1) {
            json response = create_video_list(6, vector<string>()); // not logged in
            send_json_to_client(client, response);
            return;
        }
        if (request["filename"].get<string>() == "") {
            //first request, return list of files
            vector<string> files = getDatabaseFiles(server_video_path, ".mp4");
            json response = create_video_list(0, files); 
            send_json_to_client(client, response);
        } else {
            handle_video_request(client, request); 
        }
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
            pthread_mutex_lock(&client->socket_mutex);
        }

        // Read the message from the client, if any
        json request=get_json(client->ssl, 1);//non-blockingly read the whole json
        // -- UNLOCK (with debug) --
        {
            pthread_mutex_unlock(&client->socket_mutex);
        }
        if(!request.empty() && request["type"] == "EOF"){
            client_disconnect(*client);
            continue;
        }

        else if(!request.empty()){
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
    srand(time(NULL));
    for (int i = 0;i < 5;i++) session_name += char(rand() % 26 + 'a');
    cerr << "Session name: " << session_name << "\n";
    generate_cert(session_name);

    SSL_CTX* ctx = create_ssl_server_context();
    configure_ssl_context(ctx, session_name);

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

        SSL* new_client_ssl = SSL_new(ctx);
        SSL_set_fd(new_client_ssl, new_socket);
        if (SSL_accept(new_client_ssl) <= 0) {
            ERR_print_errors_fp(stderr);
        } 
        Client* new_client = new Client(new_socket, new_client_ssl);

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
