# Simple HTTP Server

This repository contains a multi-threaded HTTP server written in C. The server handles basic HTTP GET requests, serving static files from the file system.

## Features

- **Multi-threaded:** Each incoming client connection is handled by a separate thread, allowing for concurrent request processing.
- **Dynamic Content Serving:** Serves files with various MIME types based on the file extension.
- **Graceful Error Handling:** Returns appropriate HTTP status codes for unsupported methods and missing files.

## Compilation

To compile the server, use the following command:

```bash
gcc -o http_server http_server.c -lpthread -lregex
```

## Usage
To start the HTTP server, run the compiled binary:

```bash
./http_server
```

By default, the server listens on port 8080 and serves files from the current directory.

## Example
```bash
./http_server
```
Visit http://localhost:8080/index.html in your web browser to access the default index.html file.

## Implementation Details
### Request Handling:

The server parses incoming HTTP requests and extracts the requested file name and extension.
If no file is specified, the server defaults to serving index.html.
### File Serving:

The server reads the requested file from disk and sends it as an HTTP response, along with the appropriate MIME type (e.g., text/html, image/jpeg).
If the file is not found, the server responds with a 404 Not Found error.
### Thread Management:

Each client connection is handled by a new thread, allowing the server to handle multiple clients concurrently.
The pthread library is used for thread creation and management.
### MIME Type Detection:

The server detects the MIME type based on the file extension. Supported extensions include .html, .css, .jpeg, .jpg, and .png.

## Future Improvements
Add support for more HTTP methods (e.g., POST, PUT).
Improve error handling and logging for better debugging and monitoring.
Support serving files from different directories or virtual hosts.
