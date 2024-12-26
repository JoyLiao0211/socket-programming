# Socket Programming Project

## Phase 1

This project implements a simple client-server communication system using socket programming.

### Getting Started

[demo video](https://youtu.be/eVZX76FQUVY)

### Prerequisites
This project is designed to run in a Linux environment. Ensure you have the following tools installed:
- C++ Compiler
- Make

### Building the Project

To build the server and client:

```bash
cd code
make
```

This will compile the source files and produce executables for the `server` and `client`.

### Cleaning Build Files

To remove all compiled files and reset the build:

```bash
make clean
```

This cleans up all compiled binaries and object files.

### Running the Project

1. **Start the Server**  
   Open a terminal and run:

   ```bash
   ./server
   ```

   The server will run continuously and retain all registered account data while active.

2. **Run the Client**  
   In a new terminal, navigate to the `code` directory and execute:

   ```bash
   ./client
   ```

   The client will connect to the server, enabling user interactions as defined in your project.

### Important Notes

- **Server Persistence**: The server must remain active to retain account data. If the server is stopped, all registered data will be lost.
- **Multiple Clients**: This setup supports multiple clients interacting with the server simultaneously, provided that socket connections are handled correctly.

## Phase 2

### json: (no install needed)
```
nlohmann-json
```

### opencv
```
sudo apt install libopencv-dev
```
<!-- ```
sudo apt install -y libopencv-dev
sudo apt install build-essential cmake git libgtk2.0-dev pkg-config libavcodec-dev libavformat-dev libswscale-dev
sudo apt install python-dev python-numpy libtbb2 libtbb-dev libjpeg-dev libpng-dev libtiff-dev libdc1394-22-dev
```
- in `code/opencv_stuff`:
```
git clone https://github.com/opencv/opencv.git
git clone https://github.com/opencv/opencv_contrib.git
```
- in `code/opencv_stuff/`:
```
cd opencv
mkdir build && cd build
cmake -D CMAKE_BUILD_TYPE=RELEASE \
      -D CMAKE_INSTALL_PREFIX=/usr/local \
      -D INSTALL_C_EXAMPLES=ON \
      -D INSTALL_PYTHON_EXAMPLES=ON \
      -D OPENCV_GENERATE_PKGCONFIG=ON \
      -D OPENCV_EXTRA_MODULES_PATH=$(pwd)/../../opencv_contrib/modules \
      -D BUILD_EXAMPLES=ON ..
```
- in `code/opencv_stuff/opencv/build`:
```
sudo make install
sudo ldconfig
``` -->

### PortAudio
```
sudo apt-get install libasound-dev
sudo apt-get install portaudio19-dev
sudo apt-get install libmpg123-dev
```
