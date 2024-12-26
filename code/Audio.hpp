#ifndef AUDIO_HPP
#define AUDIO_HPP
#include <mpg123.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <portaudio.h>
using namespace std;
#define OUT_BUF_SIZE 4096

#define AMPLITUDE 0.5

struct Audio{
    string filename;
    mpg123_handle * mh;
    char output_buffer[OUT_BUF_SIZE];
    size_t done;
    int err;
    long rate;
    int channels, encoding;

    Audio(string _filename): filename(_filename) {}
    bool initialize() {
        if (mpg123_init() != MPG123_OK) {
            fprintf(stderr, "Failed to initialize mpg123 library\n");
            return 0;
        }
        mh = mpg123_new(NULL, &err);
        if (!mh) {
            fprintf(stderr, "Unable to create mpg123 handle: %s\n", mpg123_plain_strerror(err));
            mpg123_exit();
            return 0;
        }
        if (mpg123_open(mh, filename.c_str()) != MPG123_OK) {
            fprintf(stderr, "Error opening file: %s\n", mpg123_strerror(mh));
            mpg123_delete(mh);
            mpg123_exit();
            return 0;
        }
        if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
            fprintf(stderr, "Error getting audio format: %s\n", mpg123_strerror(mh));
            mpg123_close(mh);
            mpg123_delete(mh);
            mpg123_exit();
            return 0;
        }
        mpg123_format(mh, rate, channels, MPG123_ENC_SIGNED_16);
        return 1;
    }

    bool read(){
        if (mpg123_read(mh, output_buffer, OUT_BUF_SIZE, &done) != MPG123_OK) {
            return 0; 
        }
        return done > 0;
    }
    ~Audio(){
        mpg123_close(mh);
        mpg123_delete(mh);
        mpg123_exit();
    }
};


#define QUEUE_SIZE (1<<16)
struct AudioData{
    char pcm_data[QUEUE_SIZE];
    int start, end; //[start, end)
    int channels;
};
static int audioCallback(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo *timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData) {
    int16_t *out = static_cast<int16_t *>(outputBuffer);
    AudioData *audioData = (AudioData *)userData;

    size_t bytesToWrite = framesPerBuffer * sizeof(int16_t) * audioData->channels; // Stereo output
    size_t avail = (audioData->end - audioData->start + QUEUE_SIZE) % QUEUE_SIZE;
    if (avail < bytesToWrite) {
        memset(out, 0, bytesToWrite);
        return paContinue;
    }
    if (audioData->start + bytesToWrite > QUEUE_SIZE) {
        size_t firstPart = QUEUE_SIZE - audioData->start;
        memcpy(out, audioData->pcm_data + audioData->start, firstPart);
        memcpy(out + firstPart, audioData->pcm_data, bytesToWrite - firstPart);
    } else {
        memcpy(out, audioData->pcm_data + audioData->start, bytesToWrite);
    }

    audioData->start = (audioData->start + bytesToWrite) % QUEUE_SIZE;
    return paContinue;
    /*
    if ((audioData->end - audioData->start + QUEUE_SIZE) % QUEUE_SIZE < framesPerBuffer*2)  {
        size_t remaining = (audioData->end - audioData->start + QUEUE_SIZE) % QUEUE_SIZE;

        if (audioData->end < audioData->start) {
            memcpy(out, audioData->pcm_data + audioData->start, (QUEUE_SIZE - audioData->start));
            memcpy(out, audioData->pcm_data, audioData->end);
        } else {
            memcpy(out, audioData->pcm_data + audioData->start, remaining);
        }
        memset(out + remaining, 0, (framesPerBuffer*2 - remaining));
        audioData->start = audioData->end;
        return 0;
    }
    if (audioData->start + framesPerBuffer*2 >= QUEUE_SIZE) {
        memcpy(out, audioData->pcm_data + audioData->start, (QUEUE_SIZE - audioData->start));
        memcpy(out, audioData->pcm_data, (framesPerBuffer*2 - (QUEUE_SIZE - audioData->start)));
    } else {
        memcpy(out, audioData->pcm_data + audioData->start, framesPerBuffer*2);
    }
    audioData->start = (audioData->start + framesPerBuffer*2) % QUEUE_SIZE;
    */
}
struct AudioPlayer{
    PaStream *stream;
    AudioData data;
    AudioPlayer() {}
    PaError err;
    bool initialize(int rate, int channels) {
        memset(data.pcm_data, 0, QUEUE_SIZE);
        data.start = 0;
        data.end = 0;
        data.channels = channels;

        err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio initialization error: " << Pa_GetErrorText(err) << std::endl;
            return 0;
        }
        err = Pa_OpenDefaultStream(&stream,
                                   0,
                                   channels,
                                   paInt16,
                                   rate,
                                   512,
                                   audioCallback,
                                   &data);
        if (err != paNoError) {
            std::cerr << "PortAudio open stream error: " << Pa_GetErrorText(err) << std::endl;
            Pa_Terminate();
            return 0;
        }
        return 1;
    }
    bool play() {
        err = Pa_StartStream(stream);
        if (err != paNoError) {
            std::cerr << "PortAudio start stream error: " << Pa_GetErrorText(err) << std::endl;
            Pa_CloseStream(stream);
            Pa_Terminate();
            return 0;
        }
        return 1;
    }
    void stop() {
        err = Pa_StopStream(stream);
        if (err != paNoError) {
            std::cerr << "PortAudio stop stream error: " << Pa_GetErrorText(err) << std::endl;
        }
        Pa_CloseStream(stream);
        Pa_Terminate();
    }
};

#endif
