# Socket Programming Project

## Phase 2

### Environment Setup

- This prject is intended fr linux envirnment
- nlohmann-json: no installation needed
- PortAudio for audio streaming:
   ```bash
   sudo apt-get install libasound-dev
   sudo apt-get install portaudio19-dev
   sudo apt-get install libmpg123-dev
   ```
- ffmpeg fr vide streaming
   ```bash
   sudo apt install ffmpeg
   ```

### Building

- Execute:

```
> cd code
code> make
```

- Run Server (note: server must be on all time)

```
code> ./Server
```

- Run each Client in a separate shell

```
code> ./Client
```

- Clean up

```
code> make clean
```

### Server usage

- displayed video files must be under `server_data/video_files` with .mp4 type
- displayed audio files must be under `server_data/audio_files` with .mp3 type
- you can add new files in runtime

### Client usage

- transfered file size must be less than 10 MB
- 