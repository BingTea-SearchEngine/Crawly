#pragma once

#include <openssl/ssl.h>
#include <optional>
#include <string>
#include <vector>

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

    std::optional<std::string> getHtml();

    std::string getFilename();

    std::vector<std::string> getRobots();

   private:
    const ParsedUrl _parsedUrl;

    std::string _url;

    SSL_CTX* _ctx;
    SSL* _ssl;
    int _sockFd;
    bool _valid = true;
};
