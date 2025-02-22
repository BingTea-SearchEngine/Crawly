#include <spdlog/fmt/bundled/ranges.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>

#include "Crawly.hpp"
#include "FrontierInterface.hpp"
#include "GetSSL.hpp"
#include "ThreadPool.hpp"

#include <pthread.h>

void parseHtml(std::string url,
               std::shared_ptr<std::vector<std::string>> newUrls,
               std::shared_ptr<std::vector<std::string>> robotsUrls,
               pthread_mutex_t* m, std::string outputDir) {
    // Establish connection with url
    GetSSL conn(url);
    // Get the html as a string
    std::string html = conn.getHtml();
    std::vector<std::string> robotsTxt = conn.getRobots();

    std::string temp = url;

    // Replace url / with ; for filename
    std::replace(url.begin(), url.end(), '/', ';');
    std::ofstream outFile(outputDir + "/" + url);
    if (!outFile) {
        std::cerr << "Error opening outfile " << outputDir + "/" + url << std::endl;
        return;
    }
    outFile << html;
    outFile.close();

    pthread_mutex_lock(m);
    robotsUrls->push_back(temp);
    newUrls->push_back(temp);
    pthread_mutex_unlock(m);
}

Crawly::Crawly(std::string socketPath, int numThreads, std::string outputDir)
    : _socketPath(socketPath),
      _threads(ThreadPool(numThreads)),
      _outputDir(outputDir) {
    _clientSock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_clientSock < 0) {
        spdlog::error("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Set up the server address structure
    _serverAddr.sun_family = AF_UNIX;
    strncpy(_serverAddr.sun_path, _socketPath.c_str(),
            sizeof(_serverAddr.sun_path) - 1);

    // Connect to the server
    if (connect(_clientSock, (struct sockaddr*)&_serverAddr,
                sizeof(_serverAddr)) < 0) {
        spdlog::error("Connect failed");
        exit(EXIT_FAILURE);
    }
}

std::string Crawly::getInfo() {
    return "Communicating on " + _socketPath;
}

void Crawly::start() {
    // Send message to get inital set of urls
    spdlog::info("Sent initial message");
    uint32_t messageLen = 0;
    send(_clientSock, &messageLen, sizeof(messageLen), 0);
    uint32_t responseLen = 1;

    while (responseLen != 0) {
        int bytes_received =
            recv(_clientSock, &responseLen, sizeof(responseLen), 0);
        responseLen = ntohl(responseLen);
        std::string response(responseLen, '\0');
        recv(_clientSock, response.data(), responseLen, 0);

        if (responseLen == 0) break;
        auto [type, urls] = FrontierInterface::Decode(response);
        assert(type!=MessageType::ROBOTS);
        // std::vector<std::string> newUrls;
        auto newUrls = std::make_shared<std::vector<std::string>>();
        auto robotsUrls = std::make_shared<std::vector<std::string>>();
        spdlog::info("Received {}", urls);
        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

        for (auto url : urls) {
            _threads.queue(Task(parseHtml, url, newUrls, robotsUrls, &mutex, _outputDir));
        }

        _threads.wait();

        if (robotsUrls->size() > 0) {
            // Send robots.txt
            spdlog::info("Sending robots {}", *robotsUrls);
            std::string robotsEncoded = FrontierInterface::Encode(Message{MessageType::ROBOTS, *robotsUrls});
            messageLen = htonl(robotsEncoded.size());
            send(_clientSock, &messageLen, sizeof(messageLen), 0);
            send(_clientSock, robotsEncoded.data(), robotsEncoded.size(), 0);
        }

        // Send URLS
        spdlog::info("Sending urls {}", *newUrls);
        std::string encoded = FrontierInterface::Encode(Message{MessageType::URLS, *newUrls});
        messageLen = htonl(encoded.size());
        send(_clientSock, &messageLen, sizeof(messageLen), 0);
        send(_clientSock, encoded.data(), encoded.size(), 0);
    }

    // Exiting
    _threads.wait();
}

void printUsage(const char* programName) {
    std::cerr << "Usage: " << programName
              << " <socketPath> <numThreads> <outputDir>\n"
              << "  <socketPath>   : Path to the Unix domain socket\n"
              << "  <numThreads>   : Maximum number of concurrent threads "
                 "running parser\n"
              << "  <outputDir>   : Directory to write output to\n";
}

int main(int argc, char** argv) {
    spdlog::info("======= Crawly Started =======");

    if (argc != 4) {
        spdlog::error("Error: Invalid number of arguments");
        printUsage(argv[0]);
    }

    std::string socketPath = std::string(argv[1]);
    int numThreads = std::stoi(argv[2]);
    std::string outputDir = PROJECT_ROOT + std::string(argv[3]);

    Crawly crawly(socketPath, numThreads, outputDir);
    spdlog::info(crawly.getInfo());

    crawly.start();
}
