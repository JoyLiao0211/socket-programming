#include <nlohmann/json.hpp>
#include <iostream>


using namespace std;
using json = nlohmann::json;

// ----------------------- 1. Login ------------------------

json create_login_request(const string &username, const string &password) {
    json request;
    request["type"] = "Login";
    request["username"] = username;
    request["password"] = password;
    return request;
}

json create_login_response(int code) {
    json response;
    response["type"] = "Login";
    response["code"] = code;
    return response;
}

// ----------------------- 2. Logout ------------------------

json create_logout_request() {
    json request;
    request["type"] = "Logout";
    return request;
}

json create_logout_response(int code) {
    json response;
    response["type"] = "Logout";
    response["code"] = code;
    return response;
}

// ----------------------- 3. Register ------------------------
json create_register_request(const string &username, const string &password) {
    json request;
    request["type"] = "Register";
    request["username"] = username;
    request["password"] = password;
    return request;
}

json create_register_response(int code) {
    json response;
    response["type"] = "Register";
    response["code"] = code;
    return response;
}



// ----------------------- 4. OnlineUsers ------------------------

json create_online_users_request() {
    json request;
    request["type"] = "OnlineUsers";
    return request;
}

json create_online_users_response(int code, const vector<string> &users) {
    json response;
    response["type"] = "OnlineUsers";
    response["code"] = code;
    response["users"] = users;
    return response;
}

// ----------------------- 5. SendMessage ------------------------

json create_send_message_request(const string &username, const string &message) {
    json request;
    request["type"] = "SendMessage";
    request["username"] = username;
    request["message"] = message;
    return request;
}

json create_send_message_response(int code) {
    json response;
    response["type"] = "SendMessage";
    response["code"] = code;
    return response;
}

json create_new_message(const string &from, const string &message) {
    json response;
    response["type"] = "NewMessage";
    response["from"] = from;
    response["message"] = message;
    return response;
}

// ----------------------- 6. DirectConnect ------------------------

// 1. Request from client (a) to server
json create_direct_connect_request_to_server(const string &username, const string &IP, int port, const string &passcode) {
    json request;
    request["type"] = "DirectConnect";
    request["username"] = username;
    request["IP"] = IP;
    request["port"] = port;
    request["passcode"] = passcode;
    return request;
}

// 2. Request from server to client (b)
json create_direct_connect_request_to_peer(const string &username, const string &IP, int port, const string &passcode) {
    json request;
    request["type"] = "DirectConnectRequest";
    request["username"] = username;
    request["IP"] = IP;
    request["port"] = port;
    request["passcode"] = passcode;
    return request;
}

// 3. Response from client (b) to server
json create_direct_connect_response_to_server(int code) {
    json response;
    response["type"] = "DirectConnectRequest";
    response["code"] = code;
    return response;
}

// 4. Response from server to client (a)
json create_direct_connect_response_to_client(int code) {
    json response;
    response["type"] = "DirectConnect";
    response["code"] = code;
    return response;
}

// 5. Response from client (b) to client (a)
json create_direct_connect_response_to_client_from_peer(string passcode) {
    json response;
    response["type"] = "DirectConnect";
    response["passcode"] = passcode;
    return response;
}

// 6. DirectMessage
json create_direct_message(const string &message) {
    json request;
    request["type"] = "DirectMessage";
    request["message"] = message;
    return request;
}

// ----------------------- 7. file transfer ------------------------

// 1. Request from client a to b

json create_file_transfer_request(const string &filename) {
    json request;
    request["type"] = "TransferFileRequest";
    request["filename"] = filename;
    return request;
}

// 2. Response from client b to a

json create_file_transfer_response(int accept) {// 1: accept, 0: reject
    json response;
    response["type"] = "TransferFileResponse";
    response["accept"] = accept;
    return response;
}

// 3. File data from client a to b: send in chunks

// ----------------------- INVALID ------------------------

json create_invalid_response() {
    json response;
    response["type"] = "INVALID";
    response["code"] = 5;
    return response;
}
