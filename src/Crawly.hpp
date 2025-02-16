#pragma once

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string>
#include <vector>

#include "ThreadPool.hpp"

class Crawly {
public:
    Crawly(std::string socketPath, int numThreads, std::string outputDir);

    std::string getInfo();

    void start();

private:
    const std::string _socketPath;

    struct sockaddr_un _serverAddr;
    int _clientSock;

    ThreadPool _threads;

    std::string _outputDir;
};

// Parse the html at url and add the new urls to the newUrls while holding the mutex
void parseHtml(std::string url, std::shared_ptr<std::vector<std::string>> newUrls, pthread_mutex_t* m, std::string outputDir);
