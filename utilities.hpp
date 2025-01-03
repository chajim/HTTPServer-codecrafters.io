/*
Objects for HTTP server to be readily available once needed.
Kudos: https://medium.com/from-the-scratch/http-server-what-do-you-need-to-know-to-build-a-simple-http-server-from-scratch-d1ef8945e4fa
Kudos: https://www.youtube.com/watch?v=YwHErWJIh6Y

*/

#ifndef Net_utilities
#define Net_utilities

#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <stdio.h>
#include <vector>
#include <sstream>
#include <fstream>
#include "variables.hpp"

#include <algorithm>


#include <thread>

#include <zlib.h>
#include <map>

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
#include <netinet/in.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_CLOSE(sock) close(sock)
#endif

#define HOME "/index.html"

namespace NET{
    // The HTTPMethods enum lists the various HTTP methods for easy referral.
    enum HTTPMethods {
        CONNECT,
        DELETE,
        GET,
        HEAD,
        OPTIONS,
        PATCH,
        POST,
        PUT,
        TRACE
    };

    enum RegisteredRoutes {
        ROOT,
        ECHO,
        USERAGENT,
        FILES
    };

    class ServerSocket {
        private:
            struct sockaddr_in address;
            int sock;
            int connection;

        public:
            //constructor
            ServerSocket(int domain, int service, int protocol, int port, u_long interface);
            virtual int connect_to_network(int sock, struct sockaddr_in address) {
                return -1; // Default implementation does nothing.
            };
            void test_connection(int con);
            struct sockaddr_in get_address();
            //getters
            int get_sock();
            int get_connection();
            //setters
            void set_connection(int);

    };

    class BindingSocket: public ServerSocket {
        public:
            BindingSocket(int domain, int service, int protocol, int port, u_long interface);
            int connect_to_network(int sock, struct sockaddr_in address);
            
    };

    class ConnectingSocket: public ServerSocket {
        public:
            ConnectingSocket(int domain, int service, int protocol, int port, u_long interface);
            int connect_to_network(int sock, struct sockaddr_in address);
    };

    class ListeningSocket: public BindingSocket {
        private:
            int backlog;
            int listening;
        public:
            ListeningSocket(int domain, int service, int protocol, int port, u_long interface, int bklg);
            void start_listening();


    };

    class SimpleServer {
        private:
            ListeningSocket * socket;
            virtual int accepter() = 0;
            virtual void handler() = 0;
            virtual void responder(std::map<std::string, std::string> &request, int clientSocket) = 0;
            

        public:
            SimpleServer(int domain, int service, int protocol, int port, u_long interface, int bklg);
            virtual void launch() = 0;
            ListeningSocket * get_socket(); 
    };

    class HTTPServer: public SimpleServer {
        private:
            
            char buffer[30000] = {0};
            int new_socket;
            int return_code;
            bool encoding;

            std::string resourcePath;

            

            std::string verifyCompression(const std::string &compression_header) {
                std::vector<std::string> tokens;
                std::stringstream ss(compression_header);
                std::string token;

                // Tokenize the input string using ',' as the delimiter
                while (std::getline(ss, token, ',')) {
                    // Trim whitespace from the token
                    token.erase(0, token.find_first_not_of(" \t")); // Left trim
                    token.erase(token.find_last_not_of(" \t") + 1); // Right trim

                    tokens.push_back(token);
                }

                // Check each token against the valid compression methods
                for (const auto &t : tokens) {
                    if (std::find(CompressionMethods.begin(), CompressionMethods.end(), t) != CompressionMethods.end()) {
                        return t; // Return the first valid compression method
                    }
                }

                return "invalid"; // Return an empty string if no valid compression method is found
            }

            std::string compressBody(const std::string &input) {
                z_stream zs; // zlib stream
                memset(&zs, 0, sizeof(zs));

                // Initialize zlib for gzip compression
                if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
                    throw std::runtime_error("Failed to initialize zlib for gzip compression.");
                }

                zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.data()));
                zs.avail_in = input.size();

                int ret;
                char buffer[1024];
                std::string compressedData;

                // Compress the input string
                do {
                    zs.next_out = reinterpret_cast<Bytef *>(buffer);
                    zs.avail_out = sizeof(buffer);

                    ret = deflate(&zs, Z_FINISH);

                    if (compressedData.size() < zs.total_out) {
                        compressedData.append(buffer, zs.total_out - compressedData.size());
                    }
                } while (ret == Z_OK);

                // Clean up
                deflateEnd(&zs);

                if (ret != Z_STREAM_END) {
                    throw std::runtime_error("Compression failed.");
                }

                return compressedData;
            };

            int accepter();
            void handler();
            void responder(std::map<std::string, std::string> &request, int clientSocket);    
                    
            void handleConnection(int clientSocket);
            
            virtual std::map<std::string, std::string> HttpRequest(const std::string &request_to_tokenize);

            bool resourceExists(const std::string &path) {
                std::string actualPath = (path == "/") ? HOME : path;
                //std::cout << actualPath;
                if (actualPath == HOME){
                return true;
                }
                // Check if the file exists
                //std::ifstream file("." + actualPath); // Use '.' to refer to the current directory
                //return file.good();                   // Return true if the file exists
                return false;
            }          

            RegisteredRoutes resolveRoute(const std::string &resource) {
                std::string res;
                if (resource.length()>1){
                    int start = 1;
                    std::string temp = resource.substr(1, resource.length());
                    int end = temp.find("/");
                    if (end == std::string::npos){
                        end = resource.length();
                    }                    
                    res = resource.substr(start, end);
                } else {
                    res = resource; // resource is just slash - /
                }

                if (res == "/") {
                    return ROOT;
                } else if ( res == "echo") {
                    return ECHO;
                } else if ( res == "user-agent") {
                    return USERAGENT;
                } else if ( res == "files") {
                    return FILES;
                } else {
                    throw std::runtime_error("Route not found");
                }
            }

        public:
            HTTPServer();
            
            void launch();
            
        
    };
    
}
#endif /* Net_utilities */
