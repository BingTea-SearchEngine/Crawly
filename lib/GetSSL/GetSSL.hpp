#pragma once

#include <string>
#include <openssl/ssl.h>

class ParsedUrl {
   public:
    const char* CompleteUrl;
    char *Service, *Host, *Port, *Path;

    ParsedUrl(const char* url);

    ~ParsedUrl();

   private:
    char* pathBuffer;
};

class GetSSL {
public:
    GetSSL(std::string url);

    ~GetSSL();

    std::string getHtml();

    std::string getFilename();
private:
    const ParsedUrl _parsedUrl;

    SSL_CTX* _ctx;
    SSL* _ssl;
    int _sockFd;
    bool _valid = true;
};
