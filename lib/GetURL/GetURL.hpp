#pragma once

#include <openssl/ssl.h>
#include <optional>
#include <string>
#include "GetSSL.hpp"

class GetURL {
   public:
    GetURL(std::string url);

    ~GetURL();

    std::optional<std::string> getHtml();

    std::string getFilename();

   private:
    const ParsedUrl _parsedUrl;

    std::string _url;

    int _sockFd;
    bool _valid = true;
};
