#include<iostream>
#include<string>
#define window_width 40
#define min(a,b) ((a)<(b)?(a):(b))

void print_message_with_padding(const std::string &message, const char padding_char = '=') {
    int padding_left = (window_width - message.length()) / 2 - 1;
    int padding_right = window_width - message.length() - padding_left - 2;
    std::cout << std::string(padding_left, '=') << " " << message << " " << std::string(padding_right, '=') << "\n";
}

void print_message_box_with_padding(const std::string &message, const std::string &sender) {
    // ╚ ╔ ╗ ╝ ═ ║
    //print ╔══ sender ══╗
    int left_padding = (window_width - sender.length() - 4) / 2;
    int right_padding = window_width - sender.length() - 4 - left_padding;
    std::cout << "╔";
    while(left_padding--)std::cout << "═";
    std::cout << " " << sender << " ";
    while(right_padding--)std::cout << "═";
    std::cout << "╗\n";
    //print ║ message    ║
    int max_message_len = window_width - 4;
    for(int start = 0; start < message.length(); start += max_message_len){
        int cur_message_len = min(max_message_len, (int)message.length() - start);
        std::cout << "║ " << message.substr(start, cur_message_len) << std::string(max_message_len - message.substr(start, max_message_len).length(), ' ') << " ║\n";
    }
    //print ╚════════════╝
    std::cout << "╚";
    for(int i = 0; i < window_width - 2; i++)std::cout << "═";
    std::cout << "╝\n";
}