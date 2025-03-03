#include "GetURL.hpp"

#include <netdb.h>
#include <openssl/ssl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <vector>

GetURL::GetURL(std::string url)
    : _parsedUrl(ParsedUrl(url.c_str())), _url(url) {
    // Get the host address.
    struct addrinfo hints, *res;
    char ipstr[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(_parsedUrl.Host, _parsedUrl.Port, &hints, &res);
    if (status != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        _valid = false;
        return;
    }

    // Create a TCP/IP socket.
    _sockFd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (_sockFd == -1) {
        std::cerr << "Error creating socket\n";
        freeaddrinfo(res);
        return;
    }

    struct timeval timeout;
    timeout.tv_sec = 15;
    timeout.tv_usec = 0;
    setsockopt(_sockFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(_sockFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // Connect the socket to the host address.
    if (connect(_sockFd, res->ai_addr, res->ai_addrlen) == -1) {
        std::cerr << "Error connecting to server\n";
        close(_sockFd);
        freeaddrinfo(res);
        _valid = false;
        return;
    }
}

GetURL::~GetURL() {
    if (!_valid)
        return;
    close(_sockFd);
}

std::optional<std::string> GetURL::getHtml() {
    if (!_valid) {
        return std::nullopt;
    }

    // Create messag
    std::string request = "GET /" + std::string(_parsedUrl.Path) +
                          " HTTP/1.1\r\n"
                          "Host: " +
                          std::string(_parsedUrl.Host) +
                          "\r\n"
                          "Connection: close\r\n"
                          "User-Agent: wbjin@umich.edu\r\n"
                          "\r\n";
    // Send message
    int totalSent = 0;
    int requestLength = request.size();
    while (totalSent < requestLength) {
        ssize_t bytesSent = send(_sockFd, request.c_str() + totalSent, requestLength - totalSent, 0);
        if (bytesSent < 0) {
            std::cerr << "Error sending request: " << strerror(errno) << "\n";
            close(_sockFd);
            exit(EXIT_FAILURE);
        }
        totalSent += bytesSent;
    }

    char buffer[10240];
    int bytesReceived;
    std::string response;
    std::string html;
    bool foundHeader = false;

    while ((bytesReceived = recv(_sockFd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        response += buffer;

        if (!foundHeader) {
            size_t pos = response.find("\r\n\r\n");
            if (pos != std::string::npos) {
                foundHeader = true;
                html += response.substr(pos + 4);
            }
        } else {
            html += buffer;
        }
    }

    return html;
}

