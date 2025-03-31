#pragma once

#include <iostream>
#include <optional>
#include <curl/curl.h>
#include <string>

class GetCURL {
public:
    static GetCURL& getInstance();

    std::optional<std::string> getHtml(std::string url);
private:
    GetCURL();
    ~GetCURL();
    GetCURL(const GetCURL&) = delete;
    GetCURL& operator=(const GetCURL&) = delete;
};
