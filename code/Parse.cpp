#include "Parse.h"
#include <sstream>
#include <iostream>

std::vector<std::string> parse_message(std::string& msg, char delimiter, int num_tokens) {
    using namespace std;

    vector<string> tokens;   // To store up to 3 tokens
    size_t pos = 0, last_pos = 0;

    // Extract tokens up to a maximum of 3
    while (tokens.size() < num_tokens && (pos = msg.find(delimiter, last_pos)) != string::npos) {
        string item = msg.substr(last_pos, pos - last_pos);
        if (!item.empty()) {
            tokens.push_back(item);
        }
        last_pos = pos + 1;  // Move past the delimiter
    }

    // If fewer than 3 tokens were found, do nothing and return an empty vector
    if (tokens.size() < num_tokens) {
        tokens.clear();
        return tokens;
    }

    // If exactly 3 tokens are found, modify the input string to the remainder
    if (last_pos < msg.size()) {
        msg = msg.substr(last_pos);  // Set to the remainder of the string
    } else {
        msg.clear();  // No remainder left
    }

    return tokens;
}
