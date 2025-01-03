#include "utilities.hpp"
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
#include <sys/stat.h>
#include <filesystem>

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


void Echo(std::string &argument, std::string &response, std::string &return_body_content){
    std::string to_echo = argument.substr(6,argument.length());
           
    return_body_content = to_echo;
}

void UserAgent(std::map<std::string, std::string> &request, std::string &response, std::string &return_body_content){
    std::string useragent = request["User-Agent"];
        
    return_body_content = useragent;
}

void Files(std::map<std::string, std::string> &request, std::string &response, std::string &directory, std::string &return_body_content){
    std::string filename = request["Resource"];
    std::string method = request["Method"];

    filename = filename.substr(6); // remove /files/ on the beginning
    int filename_length = filename.length();
    std::string fullpath = directory+filename;

    if (method == "GET"){
        bool file_exists = std::filesystem::exists( fullpath );

        if (file_exists){
            response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: application/octet-stream\r\n";        
            
            // read the file
            std::ifstream file(fullpath, std::ios::binary | std::ios::ate);
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<char> buffer(size);
            response += "Content-Length: " + std::to_string(size) + "\r\n\r\n";
            
            // set the body to be the data from the file
            if (file.read(buffer.data(), size)) {
                response += buffer.data();
            }
        } else {
            response = "HTTP/1.1 404 Not Found\r\n";
            response += "Content-Type: text/html\r\n\r\n";
            response += "<html><body><h1>404 Not Found</h1><p>Route not found.</p></body></html>";
        }

    } else if (method == "POST"){
        std::ofstream WriteToFile(fullpath);
        WriteToFile << request["Body"];
        WriteToFile.close();
        response += "HTTP/1.1 201 Created\r\n\r\n";
        //response += "Content-Type: application/octet-stream\r\n"; 


    }
    

    
}