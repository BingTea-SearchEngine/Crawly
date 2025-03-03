#pragma once

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ThreadPool.hpp"

class Crawly {
   public:
    Crawly(std::string socketPath, int numThreads, std::string outputDir);

    ~Crawly();

    std::string getInfo();

    void start();

   private:
    const std::string _socketPath;

    struct sockaddr_un _serverAddr;
    int _clientSock;

    ThreadPool _threads;

    std::string _outputDir;

    std::ofstream _logFile;

    int _numSuccessful = 0;
    int _numReceived = 0;
    int _batchNum = 0;
};

// Parse the html at url and add the new urls to the newUrls while holding the mutex
void parseHtml(std::string url,
               std::shared_ptr<std::vector<std::string>> newUrls,
               std::shared_ptr<std::vector<std::string>> robotsUrls,
               std::shared_ptr<std::unordered_map<std::string, bool>> success,
               int pageNum,
               pthread_mutex_t* m, std::string outputDir);
