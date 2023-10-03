#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#define MYPORT 8080
#define BACKLOG 10

const int BUF_SIZE = 1024;

std::string getFilePath(char buffer[BUF_SIZE]) {
    int start = strstr(buffer, "GET") - buffer + 5;
    int end = strstr(buffer, "HTTP") - buffer - 1;
    std::string path = "";
    for (int i = start; i < end; i++) {
        path += buffer[i];
    }
    for (char& c : path) {
        c = std::tolower(c);
    }
    
    // handle spaces
    int pos = 0;
    while ((pos = path.find("%20", pos)) != std::string::npos) {
        path.replace(pos, 3, " ");
        pos++;
    }
    
    // handle %
    pos = 0;
    while ((pos = path.find("%25", pos)) != std::string::npos) {
        path.replace(pos, 3, "%");
        pos++;
    }
    
    return path;
}

std::string getContentType(std::string path) {
    for (int i = path.length()-1; i >= 0; i--) {
        if (path[i] == '.') {
            std::string type = path.substr(i+1);
            if (type == "html" || type == "htm") {
                return "text/html";
            }
            if (type == "txt") {
                return "text/plain";
            }
            if (type == "jpg" || type == "jpeg") {
                return "image/jpeg";
            }
            if (type == "png") {
                return "image/png";
            }
            break;
        }
    }
    return "application/octet-stream";
}

void sendResponse(int fd, std::string path, std::string contentType) {
    std::string statusCode = "404 Not Found";
    path = "./" + path;
    int length = 0;
    char* data;
    
    for (const auto &entry : std::filesystem::directory_iterator(".")) {
        std::string dirEntry = entry.path().string();
        if (strcasecmp(path.c_str(), dirEntry.c_str()) == 0) {
            std::ifstream file(dirEntry);
            if (file.is_open()) {
                statusCode = "200 OK";
                file.seekg(0, std::ios::end);
                length = file.tellg();
                file.seekg(0, std::ios::beg);
                data = new char[length];
                file.read(data, length);
                file.close();
            }
            break;
        }
    }
    
    std::string lines = "HTTP/1.1 " + statusCode + "\r\n";
    lines.append("Content-Length: " + std::to_string(length) + "\r\n");
    lines.append("Content-Type: " + contentType + "\r\n\r\n");
    
    int headerLen = lines.length();
    const char* header = lines.c_str();
    char* response = new char[length + headerLen];
    
    memcpy(response, header, headerLen);
    if (statusCode == "200 OK") {
        memcpy(response + headerLen, data, length);
        delete[] data;
    }
    
    if (statusCode[0] == '2') {
        std::cout << header << std::endl << std::endl;
    }
    
    send(fd, response, length + headerLen, 0);
    delete[] response;
}

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t sin_size;
    int bytes_read;
    char buffer[BUF_SIZE] = {0};
    
    // create socket
    if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Could not create socket");
        exit(1);
    }
    
    // set address info
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(MYPORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));
    
    // bind socket
    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr)) == -1) {
        perror("Could not bind socket");
        exit(1);
    }
    
    // listen
    if (listen(server_fd, BACKLOG) == -1) {
        perror("Could not listen for connections");
        exit(1);
    }
    
    // accept
    while (1) {
        sin_size = sizeof(struct sockaddr_in);
        if ((client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &sin_size)) == -1) {
            perror("Could not connect to client");
            continue;
        }
        
        printf("Server: received connection from %s\n", inet_ntoa(client_addr.sin_addr));
        
        // read
        if ((bytes_read = recv(client_fd, buffer, BUF_SIZE, 0)) == -1) {
            perror("Could not read request");
            close(client_fd);
            continue;
        }
        
        //write
        std::string path = getFilePath(buffer);
        std::string contentType = getContentType(path);
        
        sendResponse(client_fd, path, contentType);
        
        close(client_fd);
    }
    return 0;
}
