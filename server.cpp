#include "utilities.hpp"
#include "routes.hpp"

#include <map>
#include <vector>
#include <sstream>
#include <fstream>
#include <map>

#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <WS2spi.h>
#define SOCKET_CLOSE(sock) closesocket(sock)
#else
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_CLOSE(sock) close(sock)
#endif



#define PORT 4221


NET::ServerSocket::ServerSocket(int domain, int service, int protocol, int port, u_long interface) {
    address.sin_family = domain;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(interface);

    sock = socket(domain, service, protocol);
    test_connection(sock);
    // Allow reuse of address and port
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

void NET::ServerSocket::test_connection(int item_to_test){
  if (item_to_test < 0) {
    perror("Failed to connect...");
    exit(EXIT_FAILURE);
  }
}

struct sockaddr_in NET::ServerSocket::get_address(){
  return address;
}

int NET::ServerSocket::get_sock(){
  return sock;
}

int NET::ServerSocket::get_connection(){
  return connection;
}

void NET::ServerSocket::set_connection(int con){
  connection = con;
}

// constructor
NET::BindingSocket::BindingSocket(int domain, int service, int protocol, int port, u_long interface) : ServerSocket(domain, service, protocol, port, interface){
  set_connection(connect_to_network(get_sock(), get_address()));
  test_connection(get_connection());
}

// Definition of connect_toi_network virtual fction
int NET::BindingSocket::connect_to_network(int sock, struct sockaddr_in address) {
  return bind(sock, (struct sockaddr *)&address, sizeof(address));
}

NET::ConnectingSocket::ConnectingSocket (int domain, int service, int protocol, int port, u_long interface) : ServerSocket(domain, service, protocol, port, interface){
  set_connection(connect_to_network(get_sock(), get_address()));
  test_connection(get_connection());
}

int NET::ConnectingSocket::connect_to_network(int sock, struct sockaddr_in address){
  return connect(sock, (struct sockaddr *)&address, sizeof(address) );
}

NET::ListeningSocket::ListeningSocket(int domain, int service, int protocol, int port, u_long interface, int bklg): BindingSocket(domain, service, protocol, port, interface){
  backlog = bklg;
  start_listening();
  test_connection(listening);
}

void NET::ListeningSocket::ListeningSocket::start_listening(){
  listening = listen(get_sock(), backlog);
}

NET::SimpleServer::SimpleServer(int domain, int service, int protocol, int port, u_long interface, int bklg){
  socket = new ListeningSocket(domain, service, protocol, port, interface, bklg); // memory leak warning
  //delete socket;
}
NET::ListeningSocket * NET::SimpleServer::get_socket(){
  return socket;
}

NET::HTTPServer::HTTPServer(): SimpleServer(AF_INET, SOCK_STREAM, 0, PORT, INADDR_ANY, 10) {
  launch();
}

int NET::HTTPServer::accepter() {
    struct sockaddr_in address = get_socket()->get_address();
    int addrlen = sizeof(address);

    // Accept a new connection
    int clientSocket = accept(get_socket()->get_sock(), (struct sockaddr *)&address, (socklen_t *)&addrlen);

    if (clientSocket < 0) {
        perror("Failed to accept connection...");
        throw std::runtime_error("Connection accept failed");
    }

    return clientSocket;
}

void NET::HTTPServer::handler(){
    // Extract the resource from the parsed HTTP request
    std::map<std::string, std::string> request = HttpRequest(buffer);
    resourcePath = request["Resource"];
    
    
    // Default to "/" if no resource is provided
    if (resourcePath.empty()) {
        resourcePath = HOME;
    }
    
    //std::cout << buffer << std::endl;
}

void NET::HTTPServer::responder(std::map<std::string, std::string> &request, int clientSocket) {
    std::string resourcePath = request["Resource"];
    std::string return_body_content;
    std::string compressed_body;
    std::string verified_compression = verifyCompression(request["Accept-Encoding"]);
    encoding = false;
    if (request.count("Accept-Encoding") > 0) {    
        if ( verified_compression != "invalid") {
            encoding = true;
        }
    } else {
        encoding = false;
    }

    if (resourcePath.empty()) {
        resourcePath = "/";
    }

    std::string response;
    try {
        RegisteredRoutes route = resolveRoute(resourcePath);

        switch (route) {
            case ROOT:
                if (!encoding){
                    response = "HTTP/1.1 200 OK\r\n";
                    response += "Content-Type: text/html\r\n\r\n";
                    response += "<html><body><h1>Welcome to the Home Page</h1></body></html>";
                    break;
                } else {
                    response = "HTTP/1.1 200 OK\r\n";
                    response += "Content-Encoding: " + verified_compression + "\r\n";
                    response += "Content-Type: text/plain\r\n\r\n";
                    break;
                }
                

            case ECHO:                
                
                if (!encoding){
                    Echo(resourcePath, response, return_body_content);
                    response = "HTTP/1.1 200 OK\r\n";
                    response += "Content-Type: text/plain\r\n";
                    response += "Content-Length: " + std::to_string(return_body_content.length()) + "\r\n\r\n";
                    response += return_body_content;
                    break;
                } else {
                    Echo(resourcePath, response, return_body_content);
                    response = "HTTP/1.1 200 OK\r\n";
                    response += "Content-Encoding: " + verified_compression + "\r\n";
                    response += "Content-Type: text/plain\r\n";

                    //compress the body
                    compressed_body = compressBody(return_body_content);
                    response += "Content-Length: " + std::to_string(compressed_body.length()) + "\r\n\r\n";
                    response += compressed_body;
                    break;
                }

            case USERAGENT:
                if (!encoding){
                    UserAgent(request, response, return_body_content);
                    response = "HTTP/1.1 200 OK\r\n";
                    response += "Content-Type: text/plain\r\n";
                    response += "Content-Length: " + std::to_string(return_body_content.length()) + "\r\n\r\n";
                    response += return_body_content;
                    break;
                } else {
                    UserAgent(request, response, return_body_content);
                    response = "HTTP/1.1 200 OK\r\n";
                    response += "Content-Encoding: " + verified_compression + "\r\n";
                    response += "Content-Type: text/plain\r\n";

                    //compress the body
                    compressed_body = compressBody(return_body_content);
                    response += "Content-Length: " + std::to_string(compressed_body.length()) + "\r\n\r\n";
                    response += compressed_body;
                    break;
                }
                
            
            case FILES:
                Files(request, response, filesDirectory, return_body_content);
                break;

            default:
                throw std::runtime_error("Unhandled route");
        }
    } catch (const std::runtime_error &e) {
        response = "HTTP/1.1 404 Not Found\r\n";
        response += "Content-Type: text/html\r\n\r\n";
        response += "<html><body><h1>404 Not Found</h1><p>Route not found.</p></body></html>";
    }

    send(clientSocket, response.c_str(), response.length(), 0);
}

void NET::HTTPServer::handleConnection(int clientSocket) {
    char buffer[30000] = {0}; // Local buffer for this connection

    try {
        // Read data from the client
        int bytesRead = read(clientSocket, buffer, sizeof(buffer));
        if (bytesRead <= 0) {
            throw std::runtime_error("Failed to read from client socket");
        }

        // Process the request
        std::map<std::string, std::string> request = HttpRequest(buffer);
        std::string response;

        // Respond to the client
        responder(request, clientSocket);
    } catch (const std::exception &e) {
        std::cerr << "Error handling connection: " << e.what() << std::endl;
    }

    // Close the socket after processing
    SOCKET_CLOSE(clientSocket);
}



void NET::HTTPServer::launch() {
    while (true) {
        try {
            // Accept a new connection
            int clientSocket = accepter();
            // Spawn a thread to handle the connection
            std::thread(&NET::HTTPServer::handleConnection, this, clientSocket).detach();
        } catch (const std::exception &e) {
            std::cerr << "Error in launch: " << e.what() << std::endl;
        }
    }

    // Close the main listening socket when the server shuts down
    close(get_socket()->get_sock());
}


std::map<std::string, std::string> NET::HTTPServer::HttpRequest(const std::string &request) {
    std::map<std::string, std::string> parsedRequest;

    // Split the request by CRLF into lines
    std::istringstream requestStream(request);
    std::string line;

    // Extract the request line (e.g., "GET /apple HTTP/1.1")
    if (std::getline(requestStream, line)) {
        std::istringstream lineStream(line);
        std::string method, resource, protocol;

        // Parse the request line
        if (lineStream >> method >> resource >> protocol) {
            parsedRequest["Method"] = method;
            parsedRequest["Resource"] = resource;
            parsedRequest["Protocol"] = protocol;
        } else {
            throw std::runtime_error("Malformed request line: " + line);
        }
    }

    // Extract headers (lines until a blank line)
    std::string contentLengthKey = "Content-Length";
    size_t contentLength = 0;
    while (std::getline(requestStream, line) && !line.empty() && line != "\r") {
        auto colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            // Extract key and value from "Key: Value"
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);

            // Trim whitespace from key and value
            key.erase(key.begin(), std::find_if(key.begin(), key.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            key.erase(std::find_if(key.rbegin(), key.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), key.end());
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), value.end());

            parsedRequest[key] = value;

            // Check if the header is Content-Length
            if (key == contentLengthKey) {
                contentLength = std::stoul(value);
            }
        }
    }

    // Read the body if Content-Length is provided
    if (contentLength > 0) {
        std::string body(contentLength, '\0');
        requestStream.read(&body[0], contentLength);
        parsedRequest["Body"] = body;
    }

    return parsedRequest;
}





void parseArguments(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--directory") == 0 && i + 1 < argc) {
            filesDirectory = argv[i + 1];
            return;
        }
    }
    throw std::runtime_error("Missing or invalid --directory flag.");
}

int main(int argc, char* argv[]) {
    try {
        if (argc>1){
          parseArguments(argc, argv);
        }
        //std::cout << "Serving files from directory: " << filesDirectory << std::endl;

        // Start your HTTP server
        NET::HTTPServer server;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}