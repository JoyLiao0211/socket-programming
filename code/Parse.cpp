// parse.cpp
#include "parse.h"
#include <sstream>
#include <iostream>

std::vector<std::string> parse_message(const std::string& msg, char delimiter) {
    using namespace std;  // Use the standard namespace locally within the function

    vector<string> tokens;
    stringstream ss(msg);
    string item;
    while (getline(ss, item, delimiter)) {
        if (item.length() > 0) {
            tokens.push_back(item);
            // cout << "<" << item << ">\n";
        }
    }
    // cout << "\n";
    return tokens;
}
