#include <string>

// Array of response messages where the index represents the code
const std::string RESPONSE_MESSAGES[] = {
    "Success",                // Code 0
    "Already logged in",      // Code 1
    "Incorrect password",     // Code 2
    "Username not found",     // Code 3
    "Username already exists",// Code 4
    "Invalid command",         // Code 5 
    "Not logged in",            // Code 6
    "Recipient not found",      // Code 7
    "Recipient rejected connection", // Code 8
    "Cannot connect to peer" // Code 9
};

