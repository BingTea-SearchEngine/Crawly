#include <spdlog/fmt/bundled/ranges.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <memory>
#include <pthread.h>

#include "Crawly.hpp"
#include "FrontierInterface.hpp"
#include "GetURL.hpp"
#include "GetSSL.hpp"
#include "Parser.hpp"
#include "ThreadPool.hpp"


void writeParsedHtml(std::ofstream& outFile, std::string url, int pageNum,
                     Parser& htmlParser) {
    outFile << "URL: " << url << " Batch number: " << pageNum <<"\n";
    outFile << "<title>\n";
    for (auto w : htmlParser.getTitle())
        outFile << w << " ";
    outFile << "\n</title>\n";
    outFile << "<words>\n";
    for (auto w : htmlParser.getWords())
        outFile << w << " ";
    outFile << "\n</words>\n";
    outFile << "<links>\n";
    for (auto link : htmlParser.getUrls()) {
        outFile << "<link>\n";
        outFile << link.url << "\n";
        for (auto w : link.anchorText)
            outFile << w << " ";
        outFile << "\n</link>\n";
    }
    outFile << "</links>\n";
}

void parseHtml(std::string url,
               std::shared_ptr<std::vector<std::string>> newUrls,
               std::shared_ptr<std::vector<std::string>> robotsUrls,
               std::shared_ptr<std::unordered_map<std::string, bool>> success,
               int batchNum, 
               pthread_mutex_t* m, std::string outputDir) {
    GetSSL sslConn(url);
    // Get the html as a string
    std::optional<std::string> html = sslConn.getHtml();
    if (!html) {
        // Try again with http
        GetURL conn(url);
        html = conn.getHtml();
        if (!html) {
            success->insert({url, false});
            return;
        }
    }
    Parser htmlParser(*html);
    // std::vector<std::string> robotsTxt = conn.getRobots();
    std::vector<std::string> title = htmlParser.getTitle();
    if (title.size() == 0) {
        success->insert({url, false});
        return;
    }
    if (title.size() == 2 && title[0] == "Not" && title[1] == "Found") {
        success->insert({url, false});
        return;
    }
    if (title.size() < 3 || (title[0] != "301" || title[1] != "Moved" || title[2] != "Permanently")) {
        std::string filename = url;

        // Replace url / with ; for filename
        std::replace(filename.begin(), filename.end(), '/', ';');
        std::ofstream outFile(outputDir + "/" + std::to_string(batchNum) + "-" + filename);
        if (!outFile) {
            std::cerr << "Error opening outfile " << outputDir + "/" + url
                      << std::endl;
            success->insert({url, false});
            return;
        }
        writeParsedHtml(outFile, url, batchNum, htmlParser);
        outFile.close();
    }

    pthread_mutex_lock(m);
    // robotsUrls->push_back(temp);
    for (auto newUrl : htmlParser.getUrls()) {
        if (newUrl.url[0] == '/') {
            newUrl.url = url + newUrl.url;
        } else if (newUrl.url.compare(0, 4, "http") != 0) {
            continue;
        }
        newUrls->push_back(newUrl.url);
    }
    pthread_mutex_unlock(m);
    success->insert({url, true});
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

    _logFile.open(outputDir + "/logs");
    _logFile << "Error urls\n";
}

Crawly::~Crawly() {
    spdlog::info("{} successful out of {} received", _numSuccessful, _numReceived);
    close(_clientSock);
    _logFile.flush();
    _logFile.close();
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

        if (responseLen == 0)
            break;
        auto [type, urls] = FrontierInterface::Decode(response);
        assert(type != MessageType::ROBOTS);
        // std::vector<std::string> newUrls;
        auto newUrls = std::make_shared<std::vector<std::string>>();
        auto robotsUrls = std::make_shared<std::vector<std::string>>();
        spdlog::info("Received {}", urls);
        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        auto success =
            std::make_shared<std::unordered_map<std::string, bool>>();
        _numReceived+=urls.size();
        _batchNum++;

        for (auto url : urls) {
            _threads.queue(
                Task(parseHtml, url, newUrls, robotsUrls, success, _batchNum, &mutex, _outputDir));
        }

        _threads.wait();

        for (auto [url, success] : *success) {
            if (!success) {
                spdlog::error("Error getting {}", url);
                _logFile << url << "\n";
            } else {
                _numSuccessful++;
            }
        }

        if (robotsUrls->size() > 0) {
            // Send robots.txt
            // spdlog::info("Sending robots {}", *robotsUrls);
            std::string robotsEncoded = FrontierInterface::Encode(
                Message{MessageType::ROBOTS, *robotsUrls});
            messageLen = htonl(robotsEncoded.size());
            send(_clientSock, &messageLen, sizeof(messageLen), 0);
            send(_clientSock, robotsEncoded.data(), robotsEncoded.size(), 0);
        }

        // Send URLS
        // spdlog::info("Sending urls {}", *newUrls);
        std::string encoded =
            FrontierInterface::Encode(Message{MessageType::URLS, *newUrls});
        messageLen = htonl(encoded.size());
        if (send(_clientSock, &messageLen, sizeof(messageLen), 0) <= 0) {
            spdlog::error("Error sending message length");
            return;
        }
        if (send(_clientSock, encoded.data(), encoded.size(), 0) <= 0) {
            spdlog::error("Error sending message");
            return;
        }
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
    signal(SIGPIPE, SIG_IGN);

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
