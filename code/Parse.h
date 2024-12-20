// Parse.h
#include <vector>
#include <string>

std::vector<std::string> parse_message(std::string& msg, char delimiter = '$', int num_tokens = 3);
