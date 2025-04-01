#include "GetCURL.hpp"

GetCURL& GetCURL::getInstance() {
    static GetCURL instance;
    return instance;
}

GetCURL::GetCURL() {
    curl_global_init(CURL_GLOBAL_ALL);
}

GetCURL::~GetCURL() {
    curl_global_cleanup();
}

size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::optional<std::string> GetCURL::getHtml(std::string url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Error curl easy init\n";
        return std::nullopt;
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); // 10 seconds
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    
    CURLcode res = curl_easy_perform(curl);

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_easy_cleanup(curl);

    if (response_code == 404) {
        std::cerr << "Page not found (404) " << url << "\n";
        return std::nullopt;
    }

    if (response_code >= 400) {
        std::cerr << "HTTP error: " << response_code << " for " << url << "\n";
        return std::nullopt;
    }

    if (res != CURLE_OK) {
        std::cerr << "CURL error for " << url << ": " << curl_easy_strerror(res) << "\n";
        return std::nullopt;
    }

    if (response == "") {
        std::cerr << "Response html is empty " << url << " \n";
        return std::nullopt;
    }
    return response;
}
