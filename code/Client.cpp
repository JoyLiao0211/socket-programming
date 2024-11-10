#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>  // For close()
#include"Code.h"
#include"Parse.h"
using namespace std;

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cerr << "Socket creation error! errno: " << errno << "\n";
        return -1;
    }
    cout << "Socket created successfully: " << sock << "\n";

    // Server address setup
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);  // Port to connect to (convert to network byte order)
    
    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        cerr << "Invalid address / Address not supported\n";
        close(sock);
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cerr << "Connection Failed! errno: " << errno << "\n";
        close(sock);
        return -1;
    }
    cout << "Connected to server!\n";
    bool logged_in=0;
    string username;

    // Client interaction loop
    while (true) {
        if(logged_in)cout<<"Logged in as "<<username<<"!\n";
        else cout << "Enter 1 to log in\nEnter 2 to register\n";
        cout<<"Enter 0 to exit\n";
        string cmd;
        cin >> cmd;

        if (cmd == "0") {
            cout << "Exiting...\n";
            break;
        }

        string usr, pwd;
        string message;

        if (cmd == "1") {
            // Login request
            cout << "Username: ";
            cin >> usr;
            cout << "Password: ";
            cin >> pwd;

            // Format message as "$1$<username>$<password>$"
            message = "$1$" + usr + "$" + pwd + "$";
            send(sock, message.c_str(), message.size(), 0);
        }
        else if (cmd == "2") {
            // Registration request
            cout << "Username: ";
            cin >> usr;
            cout << "Password: ";
            cin >> pwd;

            // Format message as "$2$<username>$<password>$"
            message = "$2$" + usr + "$" + pwd + "$";
            send(sock, message.c_str(), message.size(), 0);
        }
        else {
            cout << "Invalid command. Try again.\n";
            continue;
        }

        // Receive a response from the server
        char buffer[1024] = {0};
        int bytes_received = read(sock, buffer, sizeof(buffer) - 1);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';  // Null-terminate the received data
            vector<string>mes=parse_message(buffer);
            for(string s:mes){
                cout << "Message received from server: " << RESPONSE_MESSAGES[stoi(s)] << "\n";
                if(cmd=="1"&&stoi(s)==0){
                    logged_in=1;
                    username=usr;
                }
            }
            

        } else {
            cout << "No response from server or connection lost.\n";
            break;
        }
    }

    // Close the socket
    close(sock);
    cout << "Disconnected from server.\n";
    return 0;
}
