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
#include "SSL.hpp"
#include "Audio.hpp"
#include "Video.hpp"
#include "PrintPretty.hpp"

using namespace std;
using json = nlohmann::json;

queue<json> message_queue;
int server_socket;
SSL *server_ssl;
SSL_CTX *client_ctx, *server_ctx;//does client need two ctx? server & client
bool logged_in = false;
string self_username;
string session_name;
// const int window_width = 40;

struct connected_user{
    int socket;
    SSL *ssl;
    string name;
    queue<json> message;
    connected_user(int _socket, string _name, SSL* _ssl):socket(_socket), name(_name), ssl(_ssl){}
    connected_user():socket(-1), name(""), ssl(NULL){}
};

struct InputRequest {
    string* str_ptr;
    condition_variable* cond_var;
    bool *ready;
};
mutex input_queue_mtx;
queue<InputRequest> input_queue;

string get_input() {
    string input;
    condition_variable cond_var;
    bool ready = false;
    unique_lock<mutex> lock(input_queue_mtx);
    input_queue.push({&input, &cond_var, &ready});
    cond_var.wait(lock, [&ready] { return ready; });
    return input;
}

map<string, connected_user>connected_users;

int create_socket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cerr << "Socket creation error!\n";
        exit(-1);
    }
    return sock;
}

bool connect_to_addr(int sock, const string& ip, uint16_t port, SSL *ssl) {
    sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_address.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cerr << "Connection Failed!\n";
        close(sock);
        return 0;
    }
    if(ssl){
        SSL_set_fd(ssl, sock);
        if (SSL_connect(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            return 0;
        }
    }
    return 1;
}





void print_all_commands(){
    if(!logged_in)cout<<"1: Login\n";
    else{
        print_message_with_padding("Hi " + self_username + "!");
        cout<<"1: Logout\n";
    }
    cout<<"2: Register\n";
    cout<<"3: See Online Users\n";
    if(logged_in){
        cout<<"4: Send Message\n";
        cout<<"5: Send Direct Messages\n";
        cout<<"6: Send File\n";
        cout<<"7: Listen to Music\n";
        cout<<"8: Watch a Video\n";
    }
    cout<<"0: Exit\n";
    cout.flush();
}

void handle_receive_file(string other, json message);
void direct_connect_thread_function(string other) {//from peers
    SSL* other_ssl = connected_users[other].ssl;
    queue<json> &q = connected_users[other].message;
    while (true) {
        json message = get_json(other_ssl);
        if(message.empty()){
            cout<<"Connection closed by "<<other<<"\n";
            connected_users.erase(other);
            return;
        }
        else if(message["type"] == "DirectMessage"){
            print_message_box_with_padding(message["message"], other);
        }
        else if(message["type"] == "TransferFileRequest"){
            handle_receive_file(other, message);
        }
        else{
            q.push(message);
        }
    }
}
json get_response_from_peer(string other){
    auto &message_q = connected_users[other].message;
    while(message_q.empty()){
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    json response = message_q.front();
    message_q.pop();
    return response;
}

void receive_response_thread() {// response from server
    #ifdef DEBUG
        cerr << "Receive response thread started\n";
    #endif
    while (true) {
        json response = get_json(server_ssl);
        if(response.empty()){
            cerr<<"Connection closed by server\n";
            exit(0);
        }
        //cout << "Received response: " << response.dump(4) << endl;
        if (response["type"] == "NewMessage") {
            cout << "You got a new message!\n";
            print_message_box_with_padding(response["message"], response["from"]);
        }
        else if(response["type"] == "DirectConnectRequest"){
            // cout<<"receive direct connect request\n";
            // cout<<response.dump()<<"\n";
            string username = response["username"];
            int code=0;//response to server

            //first send response to peer
            json response_to_peer = create_direct_connect_response_to_client_from_peer(response["passcode"]); 
            string ip = response["IP"].get<string>();
            int port = response["port"].get<int>();
            // connect to ip & port
            int peer_sock = create_socket(); 
            SSL* peer_ssl = SSL_new(client_ctx); 
            if(!connect_to_addr(peer_sock, ip, port, peer_ssl)){ 
                cout<<"Failed to connect to peer\n";
                code = 9;
            }
            else if(!send_json(peer_ssl, response_to_peer)){//send connect_to_peer to peer with passcode
                cout<<"Failed to send response to peer\n";
                code = 9;
            }
            else{//successfull connection
                cout<<"Connected to "<<username<<"\n";
                connected_users[username] = connected_user(peer_sock, username, peer_ssl); 
                //start thread to receive messages from peer
                thread direct_thread(direct_connect_thread_function, username); 
                direct_thread.detach(); 
            }
            json response_to_server = create_direct_connect_response_to_server(code); 
            // cout<<"response to server: "<<response_to_server.dump(4)<<"\n";  
            send_json(server_ssl, response_to_server); 
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

bool establish_direct_connection(string other){
    string passcode = to_string(rand()); // some random string
    auto [opening_sock, listening_port] = create_listening_socket();
    if(opening_sock == -1){
        cout<<"Failed to create listening socket\n";
        return 0;
    }

    json message = create_direct_connect_request_to_server(other, "127.0.0.1", listening_port, passcode);
    if(!send_json(server_ssl, message))return 0;

    int listening_sock = accept(opening_sock, NULL, NULL);
    close(opening_sock);
    SSL* listening_ssl = SSL_new(server_ctx);
    SSL_set_fd(listening_ssl, listening_sock);
    if(SSL_accept(listening_ssl) <= 0){
        ERR_print_errors_fp(stderr);
        return 0;
    }
    //get response from server first
    json res_json = get_response();
    // cout<<"response from server: "<<res_json.dump()<<"\n";
    int res_code = res_json["code"].get<int>();
    cout << RESPONSE_MESSAGES[res_code] << endl;
    if(res_code != 0){
        cout<<"Failed to send direct message\n";
        return 0;
    }
    //get response from peer
    
    json peer_response = get_json(listening_ssl);
    if(peer_response["type"] == "DirectConnect" && peer_response["passcode"] == passcode){
        //create a worker thread for the connected user
        thread direct_thread(direct_connect_thread_function, other);
        direct_thread.detach();
        cout<<"Connected to "<<other<<"\n";
        connected_users[other] = connected_user(listening_sock, other, listening_ssl);
        return 1;
    }
    else{
        cout<<"Invalid response from peer\n";
        return 0;
    }
}

void send_direct_message() {//handler TODO
    cout << "Enter the username of the recipient: ";
    string recipient;
    cin >> recipient;
    
    if(!connected_users.count(recipient) || connected_users[recipient].socket == -1){
        cout<<"Establishing connection with "<<recipient<<"\n";
        if(!establish_direct_connection(recipient)){
            cout<<"Failed to establish connection with "<<recipient<<"\n";
            return;
        }
    }
    cout << "Enter the message: ";
    string message_body;
    cin.ignore();
    getline(cin, message_body);
    json message = create_direct_message(message_body);
    send_json(connected_users[recipient].ssl, message);
}

vector<char> readFileToVector(const std::string& filePath) {
    std::vector<char> fileContents;
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);

    if (!file) {
        throw std::ios_base::failure("Error opening file: " + filePath);
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    fileContents.resize(static_cast<size_t>(fileSize));
    if (!file.read(fileContents.data(), fileSize)) {
        throw std::ios_base::failure("Error reading file: " + filePath);
    }
    return fileContents;
}

void writeVectorToFile(const std::vector<char>& data, const std::string& filePath) {
    std::ofstream file(filePath, std::ios::binary);

    if (!file) {
        throw std::ios_base::failure("Error opening file for writing: " + filePath);
    }

    file.write(data.data(), static_cast<std::streamsize>(data.size()));

    if (!file) {
        throw std::ios_base::failure("Error writing to file: " + filePath);
    }
}

void handle_send_file_request() {
    cout << "Enter the username of the recipient: ";
    string recipient;
    cin >> recipient;

    if (!connected_users.count(recipient) || connected_users[recipient].socket == -1) {
        cout << "Establishing connection with " << recipient << "\n";
        if (!establish_direct_connection(recipient)) {
            cout << "Failed to establish connection with " << recipient << "\n";
            return;
        }
    }

    cout << "Enter the path of the file to send: ";
    string file_path;
    cin >> file_path;
    string file_name = file_path.substr(file_path.find_last_of('/') + 1);

    vector<char> file_data;
    try {
        file_data = readFileToVector(file_path);
    } catch (const std::ios_base::failure& e) {
        cout << "Failed to read file: " << e.what() << "\n";
        return;
    }
    json message = create_file_transfer_request(file_path);
    send_json(connected_users[recipient].ssl, message);
    json response_from_recipient = get_response_from_peer(recipient);
    int accept = response_from_recipient["accept"].get<int>();
    if(accept == 0){
        cout<<"Recipient rejected the file transfer\n";
        return;
    }
    if (!send_file(connected_users[recipient].ssl, file_data)) {
        cout << "Failed to send file\n";
        return;
    }
    cout << "File sent successfully\n";
}

void handle_receive_file(string other, json message) {
    string file_name = message["filename"];
    cout << "You got a file transfer request from " << other << "\n";
    cout << "Do you want to accept the file transfer? (y/n): ";
    cout.flush();
    string choice = get_input();
    if (choice != "y") {
        cout << "File transfer rejected\n";
        json response = create_file_transfer_response(0);
        send_json(connected_users[other].ssl, response);
        return;
    }
    json response = create_file_transfer_response(1);
    send_json(connected_users[other].ssl, response);
    vector<char> file_data;
    if(!receive_file(connected_users[other].ssl, file_data)){
        cout<<"Failed to receive file\n";
        return;
    }
    cout << "Enter the path to save the file: ";
    cout.flush();
    string save_path = get_input();
    try {
        writeVectorToFile(file_data, save_path);
        cout << "File saved successfully\n";
    } catch (const std::ios_base::failure& e) {
        cout << "Failed to write file: " << e.what() << "\n";
        return;
    }
    
}

void handle_audio_streaming(){
    json request = create_audio_request("");
    send_json(server_ssl, request);
    json res_json = get_response();
    if (res_json["code"].get<int>() != 0) {
        cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
        return;
    }
    cout << "Select a song to play: \n";
    vector<string> files = res_json["files"].get<vector<string>>();
    for (string song: files) {
        cout << "    " << song << endl; 
    }

    string filename;
    cin >> filename;
    if(find(files.begin(), files.end(), filename) == files.end()){
        cout<<"Invalid filename\n";
        return;
    }
    request = create_audio_request(filename);
    send_json(server_ssl, request);
    res_json = get_response();
    if (res_json["code"].get<int>() != 0) {
        cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
        return;
    }

    int rate = res_json["rate"].get<int>();
    int channels = res_json["channels"].get<int>();
    AudioPlayer player; 
    player.initialize(rate, channels);
    if (!player.play()) {
        cerr << "Audio player error\n";
        return;
    }
    while (true) {
        res_json = get_response();
        vector<char> vec = res_json["data"].get<vector<char>>();
        int start = player.data.start, end = player.data.end;
        int tot = QUEUE_SIZE;
        int num = vec.size();
        while ((start - end + tot - 1) % tot < num) {
            this_thread::sleep_for(chrono::milliseconds(10));
            start = player.data.start, end = player.data.end;
        }
        if (end + num >= tot) {
            memcpy(player.data.pcm_data + end, vec.data(), (tot - end));
            memcpy(player.data.pcm_data, vec.data() + (tot - end), (num - tot + end));
        } else {
            memcpy(player.data.pcm_data + end, vec.data(), (num));
        }
        end = (end + num) % tot;
        player.data.end = end;
        if (res_json["end"].get<int>() == 1) {
            cerr << "end\n";
            break;
        }
    }
    while (player.data.start != player.data.end) {
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    player.stop();
}

void handle_video_streaming(){
    json request_list = create_video_request("");
    send_json(server_ssl, request_list);
    json video_list = get_response();
    if(video_list["type"] != "VideoList"){
        cout<<"Invalid response\n";
        return;
    }
    if(video_list["code"].get<int>() != 0){
        cout<<RESPONSE_MESSAGES[video_list["code"].get<int>()]<<"\n";
        cout<<"cannot get video list\n";
        return;
    }
    vector<string> files = video_list["files"].get<vector<string>>();
    cout<<"Select a video to play: \n";
    for(string file: files){
        cout<<"    "<<file<<"\n";
    }
    string filename;
    cin>>filename;
    if(find(files.begin(), files.end(), filename) == files.end()){
        cout<<"Invalid filename\n";
        return;
    }
    json request_video = create_video_request(filename);
    send_json(server_ssl, request_video);
    while(true){
        json video_response = get_response();
        if(video_response["type"] != "VideoResponse"){
            cout<<"Invalid response\n";
            return;
        }
        if(video_response["code"].get<int>() != 0){
            cout<<RESPONSE_MESSAGES[video_response["code"].get<int>()]<<"\n";
            cout<<"cannot get video\n";
            return;
        }
        vector<char> data = video_response["data"].get<vector<char>>();
        bool start = video_response["start"].get<int>();
        if(start){
            cout<<"Start playing video\n";
        }
        bool end = video_response["end"].get<int>();
        receive_play_video(data, start, end);
        if(end == 1)break;
    }
}

void process_command(const string& cmd) {
    // string username, password, message_body;
    if(cmd == "1"){//login / logout
        if (logged_in) {//logout
            json message = create_logout_request();
            send_json(server_ssl, message);
            json res_json = get_response();
            cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
            if (res_json["code"].get<int>() == 0) {
                logged_in = false;
                self_username = "";
            }
        } else {//login
            string username, password;
            cout << "Username: ";
            cin >> username;
            cout << "Password: ";
            cin >> password;
            json message = create_login_request(username, password);
            send_json(server_ssl, message);
            json res_json = get_response();
            cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
            if (res_json["code"].get<int>() == 0) {
                logged_in = true;
                self_username = username;
            }
        }
    }
    else if (cmd == "2") {//register
        string username, password;
        if (logged_in) {
            cout << "You need to logout first\n";
            return;
        }
        cout << "Username: ";
        cin >> username;
        cout << "Password: ";
        cin >> password;
        json message = create_register_request(username, password);
        send_json(server_ssl, message);
        json res_json = get_response();
        cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
    }
    else if (cmd == "3") { // see Online Users
        json message = create_online_users_request(); cerr<<"line: "<<__LINE__<<"\n";
        send_json(server_ssl, message); cerr<<"line: "<<__LINE__<<"\n";
        json res_json = get_response(); cerr<<"line: "<<__LINE__<<"\n";
        // cout << res_json.dump(4) << endl;
        if(res_json["code"].get<int>() != 0){
            cout<<RESPONSE_MESSAGES[res_json["code"].get<int>()]<<"\n";
            return;
        } cerr<<"line: "<<__LINE__<<"\n";
        vector<string> users = res_json["users"].get<vector<string>>(); cerr<<"line: "<<__LINE__<<"\n";
        if(users.empty()){
            cout<<"No online users\n";
        }
        else{
            cout << "Online users:\n";
            for(string user: users){
                cout<<"    "<<user<<"\n";
            }
        }
        return;
    }
    else if (cmd == "4") { // send message (relay)
        string username, message_body;
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
        send_json(server_ssl, message);
        json res_json = get_response();
        // cout << res_json.dump(4) << endl;
        cout << RESPONSE_MESSAGES[res_json["code"].get<int>()] << endl;
    }
    else if(cmd == "5"){//send direct message
        if (!logged_in) {
            cout << "You need to login first\n";
            return;
        }
        send_direct_message();
    }
    else if(cmd == "6"){//send file
        if (!logged_in) {
            cout << "You need to login first\n";
            return;
        }
        handle_send_file_request();
        return;
    } else if (cmd == "7") {
        if (!logged_in) {
            cout << "You need to login first\n";
            return;
        }
        handle_audio_streaming();
    }else if(cmd == "8"){
        cout<<"Audio streaming\n";
        if(!logged_in){
            cout<<"You need to login first\n";
            return;
        }
        handle_video_streaming();
    }
    else{
        cout<<"Invalid command\n";
    }
}


int main() {
    initialize_openssl();
    //random session name
    srand(time(NULL));
    for (int i = 0;i < 5;i++) session_name += char(rand() % 26 + 'a');
    cerr << "Session name: " << session_name << "\n";
    generate_cert(session_name);

    server_ctx = create_ssl_server_context();
    client_ctx = create_ssl_client_context();
    configure_ssl_context(server_ctx, session_name);

    server_socket = create_socket();
    server_ssl = SSL_new(client_ctx);
    if(!connect_to_addr(server_socket, "127.0.0.1", 8080, server_ssl)){
        cout<<"Cannot to connect to server\n";
        return 0;
    }

    thread receiver(receive_response_thread); // Start the receive response thread
    receiver.detach(); // Detach the thread to allow it to run independently

    while (true) {
        print_all_commands();
        string cmd;
        cin >> cmd;
        {//handle input requests from other threads
            unique_lock<mutex> lock(input_queue_mtx);
            if (!input_queue.empty()) {
                InputRequest req = input_queue.front();
                *req.str_ptr = cmd;
                *req.ready = true;
                req.cond_var->notify_one();
                input_queue.pop();
                continue;
            }
            lock.unlock();
        }
        if (cmd == "0") break;
        process_command(cmd);// main thread things
    }

    close(server_socket);
    SSL_free(server_ssl);
    SSL_CTX_free(server_ctx);
    SSL_CTX_free(client_ctx);
    cleanup_openssl();
    return 0;
}
