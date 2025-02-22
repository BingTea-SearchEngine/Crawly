#include "GetSSL.hpp"

#include <netdb.h>
#include <openssl/ssl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <vector>

using std::cout, std::endl;

ParsedUrl::ParsedUrl(const char* url) {
    // Assumes url points to static text but
    // does not check.

    CompleteUrl = url;

    pathBuffer = new char[strlen(url) + 1];
    const char* f;
    char* t;
    for (t = pathBuffer, f = url; *t++ = *f++;)
        ;

    Service = pathBuffer;

    const char Colon = ':', Slash = '/';
    char* p;
    for (p = pathBuffer; *p && *p != Colon; p++)
        ;

    if (*p) {
        // Mark the end of the Service.
        *p++ = 0;

        if (*p == Slash)
            p++;
        if (*p == Slash)
            p++;

        Host = p;

        for (; *p && *p != Slash && *p != Colon; p++)
            ;

        if (*p == Colon) {
            // Port specified.  Skip over the colon and
            // the port number.
            *p++ = 0;
            Port = +p;
            for (; *p && *p != Slash; p++)
                ;
        } else
            Port = (char*)"443";

        if (*p)
            // Mark the end of the Host and Port.
            *p++ = 0;

        // Whatever remains is the Path.
        Path = p;
    } else
        Host = Path = p;
}

ParsedUrl::~ParsedUrl() {
    delete[] pathBuffer;
}

GetSSL::GetSSL(std::string url) : _parsedUrl(ParsedUrl(url.c_str())), _url(url) {
    SSL_library_init();
    _ctx = SSL_CTX_new(TLS_method());
    _ssl = SSL_new(_ctx);

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

    // Connect the socket to the host address.
    if (connect(_sockFd, res->ai_addr, res->ai_addrlen) == -1) {
        std::cerr << "Error connecting to server\n";
        close(_sockFd);
        freeaddrinfo(res);
        _valid = false;
        return;
    }

    // Build an SSL layer and set it to read/write
    // to the socket we've connected.
    SSL_set_fd(_ssl, _sockFd);
    if (SSL_connect(_ssl) != 1) {
        std::cerr << "SSL handshake failed\n";
        SSL_free(_ssl);
        close(_sockFd);
        SSL_CTX_free(_ctx);
        _valid = false;
        return;
    }
}

GetSSL::~GetSSL() {
    if (!_valid)
        return;
    SSL_shutdown(_ssl);
    SSL_free(_ssl);
    close(_sockFd);
    SSL_CTX_free(_ctx);
}

std::string GetSSL::getHtml() {
    if (!_valid) {
        return "";
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
    if (SSL_write(_ssl, request.c_str(), request.length()) <= 0) {
        _valid = false;
        std::cerr << "Error sending request\n";
        SSL_shutdown(_ssl);
        SSL_free(_ssl);
        close(_sockFd);
        SSL_CTX_free(_ctx);
        return "";
    }

    char buffer[10240];
    int bytesReceived;
    std::string response;
    std::string html;
    bool foundHeader = false;

    while ((bytesReceived = SSL_read(_ssl, buffer, sizeof(buffer) - 1)) > 0) {
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

std::vector<std::string> GetSSL::getRobots() {
    if (strlen(_parsedUrl.Path) > 0) {
        return {};
    }
    std::string robotsUrl = _url+"/robots.txt";
    std::string request = std::string("GET /robots.txt") +
                          " HTTP/1.1\r\n"
                          "Host: " +
                          std::string(_parsedUrl.Host) +
                          "\r\n"
                          "Connection: close\r\n"
                          "User-Agent: wbjin@umich.edu\r\n"
                          "\r\n";
    cout << request << endl;

    if (SSL_write(_ssl, request.c_str(), request.length()) <= 0) {
        _valid = false;
        std::cerr << "Error sending request\n";
        SSL_shutdown(_ssl);
        SSL_free(_ssl);
        close(_sockFd);
        SSL_CTX_free(_ctx);
        return {};
    }

    char buffer[10240];
    int bytesReceived;
    std::string response;

    int ssl_error = SSL_get_error(_ssl, 0);
    if (ssl_error == SSL_ERROR_ZERO_RETURN) {
        std::cerr << "SSL connection closed cleanly by the server\n";
    } else if (ssl_error == SSL_ERROR_SYSCALL) {
        std::cerr << "SSL syscall error: " << strerror(errno) << std::endl;
    } else {
        std::cerr << "SSL error: " << ssl_error << std::endl;
    }

    while ((bytesReceived = SSL_read(_ssl, buffer, sizeof(buffer) - 1)) > 0) {
        cout << bytesReceived << endl;
        buffer[bytesReceived] = '\0';
        response.append(buffer, bytesReceived);
    }
    cout << response << endl;
    return {};
}
