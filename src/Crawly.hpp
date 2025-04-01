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
#include <thread>
#include <chrono>

#include "FrontierInterface.hpp"
#include "GetURL.hpp"
#include "GetSSL.hpp"
#include "GetCURL.hpp"
#include "Parser.hpp"
#include "GatewayClient.cpp"
#include "ThreadPool.hpp"

class Crawly {
   public:
    Crawly(std::string serverIp, int serverPort, std::string outputDir, int startUrlNum);

    ~Crawly();

    void start();

   private:
    Client _client;

    // ThreadPool _threads;

    std::string _outputDir;

    std::ofstream _logFile;

    int _numSuccessful = 0;
    int _numReceived = 0;
    int _docNum = 0;
};

// Parse the html at url and add the new urls to the newUrls while holding the mutex
void parseHtml(std::string url,
               std::shared_ptr<std::vector<std::string>> newUrls,
               std::shared_ptr<std::vector<std::string>> robotsUrls,
               std::shared_ptr<std::unordered_map<std::string, bool>> success,
               int pageNum,
               std::mutex* m, std::string outputDir);
