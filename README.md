# Socket Programming Project - Phase 1

This project implements a simple client-server communication system using socket programming.

## Getting Started

### Prerequisites
This project is designed to run in a Linux environment. Ensure you have the following tools installed:
- C++ Compiler
- Make

## Building the Project

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

## Running the Project

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

## Important Notes

- **Server Persistence**: The server must remain active to retain account data. If the server is stopped, all registered data will be lost.
- **Multiple Clients**: This setup supports multiple clients interacting with the server simultaneously, provided that socket connections are handled correctly.

