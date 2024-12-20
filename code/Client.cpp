#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include "SocketIO.hpp"
#include "Code.h"

using namespace std;
using json = nlohmann::json;

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
    {"3", "OnlineUsers"}
};

string construct_message(const string& cmd, const string& username, const string& password) {
    json message;
    message["type"] = cmd_to_type[cmd];
    if (!username.empty()) message["username"] = username;
    if (!password.empty()) message["password"] = password;
    return message.dump();
}

void send_message(int sock, const string& message) {
    uint32_t msg_length = message.size();
    uint32_t msg_length_net = htonl(msg_length);
    writen(sock, &msg_length_net, sizeof(msg_length_net));
    writen(sock, message.c_str(), msg_length);
}

string receive_response(int sock) {
    uint32_t resp_length_net;
    readn(sock, &resp_length_net, sizeof(resp_length_net));
    uint32_t resp_length = ntohl(resp_length_net);
    vector<char> buffer(resp_length);
    readn(sock, buffer.data(), resp_length);
    return string(buffer.begin(), buffer.end());
}

int main() {
    int sock = create_socket();
    connect_to_server(sock, "127.0.0.1", 8080);

    while (true) {
        cout << "1: Login, 2: Register, 3: Online Users, 0: Exit\n";
        string cmd, username, password;
        cin >> cmd;

        if (cmd == "0") break;
        if (cmd == "1" || cmd == "2") {
            cout << "Username: ";
            cin >> username;
            cout << "Password: ";
            cin >> password;
        }

        string message = construct_message(cmd, username, password);
        send_message(sock, message);

        string response = receive_response(sock);
        json res_json = json::parse(response);
        cout << res_json.dump(4) << endl;
        cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
    }

    close(sock);
    return 0;
}
