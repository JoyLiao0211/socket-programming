#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <thread>
#include "SocketIO.hpp"
#include "Code.h"
#include "SSL.hpp"

using namespace std;
using json = nlohmann::json;

queue<json> message_queue;
int sock;
SSL *ssl;
bool logged_in = false;
bool allow_direct_messages = false;

int create_socket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cerr << "Socket creation error!\n";
        exit(-1);
    }
    return sock;
}

void connect_to_server(int sock, const string& ip, uint16_t port) {
    sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_address.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cerr << "Connection Failed!\n";
        close(sock);
        exit(-1);
    }
}

map<string, string> cmd_to_type = {
    {"1", "Login"},
    {"2", "Register"},
    {"3", "OnlineUsers"},
    {"4", "SendMessage"},
    {"5", "AllowDirectMessages"}
};

void print_all_commands(){
    if(!logged_in)cout<<"1: Login\n";
    else cout<<"1: Logout\n";
    cout<<"2: Register\n";
    cout<<"3: See Online Users\n";
    if(logged_in){
        cout<<"4: Send Message\n";
        if(allow_direct_messages)cout<<"5: Disallow Direct Messages\n";
        else cout<<"5: Allow Direct Messages\n";
    }
    cout<<"0: Exit\n";
}

json construct_message(const string& type, const string& username, const string& password, const string& message_body) {
    json message;
    message["type"] = type;
    if (!username.empty()) message["username"] = username;
    if (!password.empty()) message["password"] = password;
    if (!message_body.empty()) message["message"] = message_body;
    return message;
}



void receive_response_thread() {
    while (true) {
        uint32_t resp_length_net;
        if (readn(ssl, &resp_length_net, sizeof(resp_length_net)) <= 0) {
            std::cerr << "Connection closed by server.\n";
            exit(-1);
        }
        uint32_t resp_length = ntohl(resp_length_net);
        std::vector<char> buffer(resp_length);
        if (readn(ssl, buffer.data(), resp_length) <= 0) {
            std::cerr << "Connection closed by server.\n";
            exit(-1);
        }
        std::string response_string = std::string(buffer.begin(), buffer.end());
        json response = json::parse(response_string);
        if (response["type"] == "NewMessage") {
            cout << "You got a new message from " << response["from"] << "\n";
            cout << "==========\n";
            cout << (string)response["message"]<< "\n";
            cout << "==========\n";
        } else {
            message_queue.push(response);
        }
    }
}

json get_response() {
    while (true) {
        {
            if (!message_queue.empty()) {
                json response = message_queue.front();
                message_queue.pop();
                return response;
            }
        }
        this_thread::sleep_for(chrono::milliseconds(10));
    }
}

void process_command(const string& cmd) {
    string username, password, message_body;
    if(cmd == "1"){//login / logout
        if (logged_in) {//logout
            json message = construct_message("Logout", "", "", "");
            send_json(ssl, message);
            json res_json = get_response();
            cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
            if (res_json["code"].get<int>() == 0) {
                logged_in = false;
            }
        } else {//login
            cout << "Username: ";
            cin >> username;
            cout << "Password: ";
            cin >> password;
            json message = construct_message("Login", username, password, "");
            send_json(ssl, message);
            json res_json = get_response();
            cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
            if (res_json["code"].get<int>() == 0) {
                logged_in = true;
            }
        }
    }
    else if (cmd == "2") {//register
        cout << "Username: ";
        cin >> username;
        cout << "Password: ";
        cin >> password;
        json message = construct_message("Register", username, password, "");
        send_json(ssl, message);
        json res_json = get_response();
        cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
    }
    else if (cmd == "3") { // see Online Users
        json message = construct_message("OnlineUsers", "", "", "");
        send_json(ssl, message);
        json res_json = get_response();
        cout << res_json.dump(4) << endl;
    }
    else if (cmd == "4") { // send message (relay)
        if (!logged_in) {
            cout << "You need to login first\n";
            return;
        }
        cout << "Enter the username of the recipient: ";
        cin >> username;
        cout << "Enter the message: ";
        cin.ignore();
        getline(cin, message_body);
        json message = construct_message("SendMessage", username, "", message_body);
        send_json(ssl, message);
        json res_json = get_response();
        cout << res_json.dump(4) << endl;
        cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
    }
    // else if (cmd == "5") { // allow / disallow direct messages
    //     if (!logged_in) {
    //         cout << "You need to login first\n";
    //         return;
    //     }
    //     if(allow_direct_messages){//TODO
    //         allow_direct_messages = false;
    //         json message = construct_message("5", "", "", "");
    //         send_json(sock, message);
    //     }else{
    //         allow_direct_messages = true;
    //         json message = construct_message("5", "", "", "");
    //         send_json(sock, message);
    //     }
        
    // }
}

int main() {
    initialize_openssl();

    sock = create_socket();
    connect_to_server(sock, "127.0.0.1", 8080);
    
    SSL_CTX* ctx = create_ssl_client_context();
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);

    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
    }

    thread receiver(receive_response_thread); // Start the receive response thread

    while (true) {
        print_all_commands();
        string cmd;
        cin >> cmd;
        if (cmd == "0") break;
        process_command(cmd);
    }

    close(sock);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    cleanup_openssl();
    receiver.detach(); // Detach the thread to allow it to run independently
    return 0;
}
