#include <spdlog/fmt/bundled/ranges.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <memory>
#include <pthread.h>
#include <argparse/argparse.hpp>

#include "Crawly.hpp"

void writeParsedHtml(std::ofstream& outFile, std::string url, int pageNum,
                     Parser& htmlParser) {
    outFile << "URL: " << url << " Doc number: " << pageNum <<"\n";
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
               int urlNum, 
               pthread_mutex_t* m, std::string outputDir) {
    GetCURL& curlConn = GetCURL::getInstance();
    // GetSSL sslConn(url);
    // Get the html as a string
    // std::optional<std::string> html = sslConn.getHtml();
    std::optional<std::string> html = curlConn.getHtml(url);
    if (!html) {
        success->insert({url, false});
        return;
    }
    Parser htmlParser(*html);
    // std::vector<std::string> robotsTxt = conn.getRobots();
    std::vector<std::string> title = htmlParser.getTitle();
    if (title.size() == 0) {
        success->insert({url, false});
        return;
    }

    std::ofstream outFile(outputDir + "/" + std::to_string(urlNum) + ".parsed");
    if (!outFile) {
        spdlog::error("Error opening file {}", outputDir+"/"+std::to_string(urlNum) + ".parsed");
        success->insert({url, false});
        return;
    }
    writeParsedHtml(outFile, url, urlNum, htmlParser);
    outFile.close();

    pthread_mutex_lock(m);
    // robotsUrls->push_back(temp);
    for (auto newUrl : htmlParser.getUrls()) {
        if (newUrl.url.compare(0, 5, "https") != 0 || newUrl.url.size() > 30) {
            continue;
        }
        newUrls->push_back(newUrl.url);
    }
    pthread_mutex_unlock(m);
    success->insert({url, true});
}

Crawly::Crawly(std::string serverIp, int serverPort, int numThreads, std::string outputDir) : 
    _client(Client(serverIp, serverPort)),
    _threads(ThreadPool(numThreads)),
    _outputDir(outputDir) {
    _logFile.open(outputDir + "/logs.txt");
    if (!_logFile) {
        spdlog::error("Error opening logfile");
        return;
    }
    _logFile << "Error urls\n";
}

Crawly::~Crawly() {
    spdlog::info("{} successful out of {} received", _numSuccessful, _numReceived);
    _logFile.flush();
    _logFile.close();
}


void Crawly::start() {
    // Send message to get inital set of urls
    spdlog::info("Sent initial message");
    FrontierMessage initMessage{FrontierMessageType::START, {}};
    _client.SendMessage(FrontierInterface::Encode(initMessage));

    while (true) {
        std::optional<Message> response = _client.GetMessageBlocking();
        if (!response) {
            spdlog::info("Error contacting frontier");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            _client.SendMessage(FrontierInterface::Encode(initMessage));
            continue;
        }
        FrontierMessage decoded = FrontierInterface::Decode(response->msg);
        if (decoded.type == FrontierMessageType::END) {
            break;
        }

        // Set up for sending tasks to worker threads
        auto newUrls = std::make_shared<std::vector<std::string>>();
        auto robotsUrls = std::make_shared<std::vector<std::string>>();
        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        auto success =
            std::make_shared<std::unordered_map<std::string, bool>>();
        for (auto url : decoded.urls) {
            _threads.queue(
                Task(parseHtml, url, newUrls, robotsUrls, success, _numReceived, &mutex, _outputDir));
            ++_numReceived;
        }
        _threads.wait();

        for (auto [url, success] : *success) {
            if (!success) {
                spdlog::error("Error getting {}", url);
                _logFile << url << "\n";
            } else {
                spdlog::info("Success getting {}", url);
                _numSuccessful++;
            }
        }

        _client.SendMessage(FrontierInterface::Encode(FrontierMessage{FrontierMessageType::URLS, *newUrls}));
        _logFile.flush();
    }
}

int main(int argc, char** argv) {
    argparse::ArgumentParser program("crawly");
    program.add_argument("-a", "--ip")
        .help("IP address of the server");

    program.add_argument("-p", "--serverport")
        .required()
        .help("Port server is running on")
        .scan<'i', int>();

    const char* homeDir = std::getenv("HOME");
    std::string dir = std::string(homeDir) + "/index/input";
    program.add_argument("-o", "--output")
        .default_value(dir)
        .help("Output directory");

    program.add_argument("-t", "--threads")
        .default_value(4)
        .help("Port server is running on")
        .scan<'i', int>();

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }
    signal(SIGPIPE, SIG_IGN);

    std::string serverIp = program.get<std::string>("-a");
    int serverPort = program.get<int>("-p");
    std::string outputDir = program.get<std::string>("-o");
    int numThreads = program.get<int>("-t");

    spdlog::info("Server IP {}", serverIp);
    spdlog::info("Server port {}", serverPort);
    spdlog::info("Thread count {}", numThreads);
    spdlog::info("Output directory {}", outputDir);

    Crawly crawly(serverIp, serverPort, numThreads, outputDir);

    spdlog::info("======= Crawly Started =======");
    crawly.start();
}
