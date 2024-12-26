#ifndef VIDEO_HPP
#define VIDEO_HPP

#include <string>
#include <iostream>
#include <nlohmann/json.hpp>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <cstdio> // For popen and pclose
#include <cstdlib> // For system calls
#include "SocketIO.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUFFER_SIZE 4096 // Size of buffer for streaming video

void stream_video(SSL* receiver_ssl, const std::string& filepath, int clientSocket) {
    // Launch FFmpeg to stream video
    std::string inputCommand = "ffmpeg -i " + filepath + " -f mpegts -";
    FILE* pipe = popen(inputCommand.c_str(), "r");
    if (!pipe) {
        std::cerr << "Server: Failed to execute FFmpeg." << std::endl;
        return;
    }
    json video_start = create_video_response(0, vector<char>(), 1, 0);
    send_json(receiver_ssl, video_start);
    vector<char> buffer(BUFFER_SIZE);
    size_t bytesRead;
    while ((bytesRead = fread(buffer.data(), 1, BUFFER_SIZE, pipe)) > 0) {
        buffer.resize(bytesRead);
        json video_data = create_video_response(0, buffer, 0, 0);
        send_json(receiver_ssl, video_data);
        buffer.resize(BUFFER_SIZE);
    }
    json video_end = create_video_response(0, vector<char>(), 0, 1);
    send_json(receiver_ssl, video_end);
    pclose(pipe);
}


void receive_play_video(std::vector<char> data, bool start, bool end) {
    const char* outputCommand = "ffplay -autoexit -i -";
    static FILE* pipe = NULL;

    // Handle 'end' signal: Close pipe and reset state
    if (end && pipe != NULL) {
        pclose(pipe);    // Close FFplay process
        pipe = NULL;     // Reset static pointer
        std::cout << "Client: Closed FFplay." << std::endl;
        return;
    }

    // Handle 'start' signal: Open or reopen FFplay
    if (start) {
        // Close existing pipe if already open
        if (pipe != NULL) {
            pclose(pipe);
            pipe = NULL;
        }

        // Open a new FFplay process
        pipe = popen(outputCommand, "w");
        if (!pipe) {
            std::cerr << "Client: Failed to execute FFplay." << std::endl;
            return;
        }
        std::cout << "Client: Started FFplay." << std::endl;
    }

    // Write video data if pipe is open
    if (pipe != NULL && !data.empty()) {
        size_t bytesWritten = fwrite(data.data(), 1, data.size(), pipe);

        // Handle case where FFplay process is closed (e.g., 'X' button)
        if (bytesWritten < data.size()) {
            std::cerr << "Client: FFplay closed. Restarting..." << std::endl;
            pclose(pipe);    // Close the broken pipe
            pipe = NULL;     // Reset pointer for restart
        }
    }
}



#endif // VIDEO_HPP
