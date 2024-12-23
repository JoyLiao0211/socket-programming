#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <thread>
#include "SocketIO.hpp"
#include "CreateMessage.hpp"
#include "Code.h"

using namespace std;
using json = nlohmann::json;

queue<json> message_queue;
int server_socket;
bool logged_in = false;
string self_username;

struct connected_user{
    string name;
    int socket;
    queue<json> message;
};

map<string, connected_user>connected_users;

int create_socket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cerr << "Socket creation error!\n";
        exit(-1);
    }
    return sock;
}

bool connect_to_addr(int sock, const string& ip, uint16_t port) {
    sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_address.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cerr << "Connection Failed!\n";
        close(sock);
        return 0;
    }
    return 1;
}

map<string, string> cmd_to_type = {
    {"1", "Login"},
    {"2", "Register"},
    {"3", "OnlineUsers"},
    {"4", "SendMessage"},
    {"5", "SendDirectMessage"},
};

void print_all_commands(){
    if(!logged_in)cout<<"1: Login\n";
    else cout<<"1: Logout\n";
    cout<<"2: Register\n";
    cout<<"3: See Online Users\n";
    if(logged_in){
        cout<<"4: Send Message\n";
        cout<<"5: Send Direct Messages\n";
    }
    cout<<"0: Exit\n";
}

void direct_connect_thread_function(string other) {
    int sock = connected_users[other].socket;
    queue<json> &q = connected_users[other].message;
    while (true) {
        uint32_t msg_length_net;
        if (readn(sock, &msg_length_net, sizeof(msg_length_net)) <= 0) {
            cerr << "Connection closed by " << other << ".\n";
            close(sock);
            connected_users.erase(other);
            return;
        }
        uint32_t msg_length = ntohl(msg_length_net);
        vector<char> buffer(msg_length);
        if (readn(sock, buffer.data(), msg_length) <= 0) {
            cerr << "Connection closed by " << other << ".\n";
            close(sock);
            connected_users.erase(other);
            return;
        }
        string message_string = string(buffer.begin(), buffer.end());
        json message = json::parse(message_string);
        q.push(message);
    }
}

void receive_response_thread() {
    while (true) {
        uint32_t resp_length_net;
        if (readn(server_socket, &resp_length_net, sizeof(resp_length_net)) <= 0) {
            cerr << "Connection closed by server.\n";
            exit(-1);
        }
        uint32_t resp_length = ntohl(resp_length_net);
        vector<char> buffer(resp_length);
        if (readn(server_socket, buffer.data(), resp_length) <= 0) {
            cerr << "Connection closed by server.\n";
            exit(-1);
        }
        string response_string = string(buffer.begin(), buffer.end());
        json response = json::parse(response_string);
        if (response["type"] == "NewMessage") {
            cout << "You got a new message from " << response["from"] << "\n";
            cout << "==========\n";
            cout << (string)response["message"]<< "\n";
            cout << "==========\n";
        }
        else if(response["type"] == "DirectConnectRequest"){
            cout<<"receive direct connect request\n";
            cout<<response.dump(4)<<"\n";
            string username = response["username"];
            int code=0;//response to server
            json response_to_peer = create_direct_connect_response_to_client_from_peer(response["passcode"]);
            string ip = response["IP"].get<string>();
            int port = response["port"].get<int>();
            // connect to ip & port
            int peer_sock = create_socket();
            connected_users[username].socket = peer_sock;
            connected_users[username].name = username;
            if(!connect_to_addr(peer_sock, ip, port)){
                code = 9;
            }else{//send connect_to_peer to peer with passcode
                if(!send_json(peer_sock, response_to_peer)){
                    code = 9;
                    break;
                }
                //start thread to receive messages from peer
                thread direct_thread(direct_connect_thread_function, username);
                direct_thread.detach(); 
            }
            json response_to_server = create_direct_connect_response_to_server(code);
            cout<<"response to server: "<<response_to_server.dump(4)<<"\n";
            send_json(server_socket, response_to_server);
        }
        else {
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

pair<int,int> create_listening_socket(){//return socket_fd, port_num
    int direct_sock = create_socket();
    sockaddr_in direct_address = {0};
    direct_address.sin_family = AF_INET;
    direct_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    direct_address.sin_port = htons(0); // Let the OS choose an available port

    if (bind(direct_sock, (struct sockaddr*)&direct_address, sizeof(direct_address)) < 0) {
        cerr << "Bind failed!\n";
        close(direct_sock);
        return {-1, -1};
    }

    if (listen(direct_sock, 1) < 0) {
        cerr << "Listen failed!\n";
        close(direct_sock);
        return {-1, -1};
    }

    socklen_t addr_len = sizeof(direct_address);
    if (getsockname(direct_sock, (struct sockaddr*)&direct_address, &addr_len) == -1) {
        cerr << "getsockname() failed!\n";
        close(direct_sock);
        return {-1, -1};
    }

    int port = ntohs(direct_address.sin_port);
    return {direct_sock, port};
}



void send_direct_message() {//handler TODO
    cout << "Enter the username of the recipient: ";
    string recipient;
    cin >> recipient;
    
    if(!connected_users.count(recipient) || connected_users[recipient].socket == -1){
        
        string passcode = to_string(rand()); // some random string
        auto [listening_sock, listening_port] = create_listening_socket();
        if(listening_sock == -1){
            cout<<"Failed to create listening socket\n";
            return;
        }

        json message = create_direct_connect_request_to_server(recipient, "127.0.0.1", listening_port, passcode);
        connected_users[recipient].socket = listening_sock;
        connected_users[recipient].name = recipient;
        send_json(server_socket, message);

        json res_json = get_response();
        cout<<"response from server: "<<res_json.dump(4)<<"\n";
        int res_code = res_json["code"].get<int>();
        cout << RESPONSE_MESSAGES[res_code] << endl;
        if(res_code != 0){
            cout<<"Failed to send direct message\n";
            return;
        }
        connected_users[recipient] = {};
        json peer_response = get_json(listening_sock);
        if(peer_response["type"] == "DirectConnect" && peer_response["passcode"] == passcode){
            //create a worker thread for the connected user
            thread direct_thread(direct_connect_thread_function, recipient);
            direct_thread.detach();
            cout<<"Connected to "<<recipient<<"\n";
        }
        else{
            cout<<"Invalid response from peer\n";
            return;
        }
    }
    cout << "Enter the message: ";
    string message_body;
    cin.ignore();
    getline(cin, message_body);
    json message = create_direct_message(message_body);

}

void process_command(const string& cmd) {
    string username, password, message_body;
    if(cmd == "1"){//login / logout
        if (logged_in) {//logout
            json message = create_logout_request();
            send_json(server_socket, message);
            json res_json = get_response();
            cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
            if (res_json["code"].get<int>() == 0) {
                logged_in = false;
                self_username = "";
            }
        } else {//login
            cout << "Username: ";
            cin >> username;
            cout << "Password: ";
            cin >> password;
            json message = create_login_request(username, password);
            send_json(server_socket, message);
            json res_json = get_response();
            cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
            if (res_json["code"].get<int>() == 0) {
                logged_in = true;
                self_username = username;
            }
        }
    }
    else if (cmd == "2") {//register
        cout << "Username: ";
        cin >> username;
        cout << "Password: ";
        cin >> password;
        json message = create_register_request(username, password);
        send_json(server_socket, message);
        json res_json = get_response();
        cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
    }
    else if (cmd == "3") { // see Online Users
        json message = create_online_users_request();
        send_json(server_socket, message);
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
        json message = create_send_message_request(username, message_body);
        send_json(server_socket, message);
        json res_json = get_response();
        cout << res_json.dump(4) << endl;
        cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
    }
    else if(cmd == "5"){//send direct message
        if (!logged_in) {
            cout << "You need to login first\n";
            return;
        }
        send_direct_message();
    }
    else{
        cout<<"Invalid command\n";
    }
}


int main() {
    server_socket = create_socket();
    while(!connect_to_addr(server_socket, "127.0.0.1", 8080)){
        cout<<"Trying to connect to server\n";
        this_thread::sleep_for(chrono::seconds(1));
    }

    thread receiver(receive_response_thread); // Start the receive response thread
    receiver.detach(); // Detach the thread to allow it to run independently

    while (true) {
        print_all_commands();
        string cmd;
        cin >> cmd;
        if (cmd == "0") break;
        process_command(cmd);
    }

    close(server_socket);
    return 0;
}
