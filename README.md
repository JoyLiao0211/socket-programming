# Socket Programming Project

By 廖禹喬(b11902007) & 賴昭勳(b11902107)

## Environment Setup

- This prject is intended fr linux envirnment
- nlohmann-json: no installation needed
- PortAudio for audio streaming:
    ```shell
    sudo apt-get install libasound-dev
    sudo apt-get install portaudio19-dev
    sudo apt-get install libmpg123-dev
    ```
- ffmpeg fr vide streaming
    ```shell
    sudo apt install ffmpeg
    ```

## Building

- Execute:
    ```shell
    > cd code
    code> make
    ```
- Run Server (note: server must be on all time)
    ```shell
    code> ./Server
    ```
- Run each Client in a separate shell
    ```shell
    code> ./Client
    ```
- Clean up
    ```shell
    code> make clean
    ```

## Usage

### Server

- displayed video files must be under `server_data/video_files` with `.mp4` type
- displayed audio files must be under `server_data/audio_files` with `.mp3` type
- you can add new files in runtime

### Client

- transfered file size must be less than 10 MB


<!-- <details>
<summary><span>Detailed Usage</span></summary>

- Login / Logout


</details> -->

## More details can be found in `Documentation.md`