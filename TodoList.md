# Todo list

- [x] multithread
- [x] send chat message
    - [x] relay mode
    - [x] direct mode
        - [x] Server: create mutex lock for every client socket
- [x] Message Encryption with OpenSSL
- [x] transfer file with encryption
    - [ ] very big files
        1. disallow sending file size more than some threshold?
        2. read and send chunks at the same time?
- [ ] streaming
    - [x] audio
        - [x] file selection
    - [ ] video
    - [ ] start playing only when the buffer has enough data to play
- [ ] README
- [ ] 5~10 min demo video
    - go over packages
    - make
    - run server
    - run one client
    - register + login
    - run client 2
    - register + login
    - see online users
    - send relay message
    - send direct message
    - send text.txt file, show accept file & reject file
    - put the chill.mp3 and late_for_work.mp4 in dir on runtime
    - play audio: chill.mp3
    - play video: late_for_work.mp4


## Bugs
- [ ] remove error codes, use 1 for all errors and add error message field
- [x] login -> exit, server need to logout that user
- [x] a b client, a exit, b read eof (its a feature not a bug)
- [x] when server closes connection, client process terminates
- [x] cert stuffs
    - [ ] arg: where cert
    - [x] auto create cert
- [ ] client can press enter to stop playing audio

## bonus

- [ ] webcam and microphone
- [ ] GUI interface

![alt text](image.png)
