#include <spdlog/fmt/bundled/ranges.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <memory>
#include <pthread.h>
#include <argparse/argparse.hpp>

#include "Crawly.hpp"

bool isEnglish(const std::string& text) {
    for (unsigned char c : text) {
        if (c > 127) {
            return false; // Non-ASCII character detected
        }
    }
    return true;
}

std::string get_base_url(const std::string& url) {
    std::regex base_url_regex(R"(^(\w+):\/\/([^\/]+))");
    std::smatch match;
    if (std::regex_search(url, match, base_url_regex) && match.size() >= 3) {
        return match.str(1) + "://" + match.str(2);
    }
    return "";
}

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
        if (link.url.compare(0, 5, "https") != 0 || link.url.size() > 30) {
            continue;
        }
        outFile << link.url << "\n";
    }
    outFile << "</links>\n";
}

void parseHtml(std::string url,
               std::shared_ptr<std::vector<std::string>> newUrls,
               std::shared_ptr<std::vector<std::string>> robotsUrls,
               std::shared_ptr<std::unordered_map<std::string, bool>> success,
               std::shared_ptr<std::unordered_map<std::string, bool>> tryAgain,
               int urlNum, 
               std::mutex* m, std::string outputDir) {
    GetCURL& curlConn = GetCURL::getInstance();
    // GetSSL sslConn(url);
    // Get the html as a string
    // std::optional<std::string> html = sslConn.getHtml();
    std::optional<std::string> html = curlConn.getHtml(url);
    if (!html) {
        // tryAgain->insert({url, true});
        success->insert({url, false});
        return;
    }
    Parser htmlParser(*html);
    std::string lang = htmlParser.getLanguage();
    if (lang != "en" && lang != "en-us" && lang != "en-US" && lang != "en-Us") {
        success->insert({url, false});
        return;
    }
    // std::vector<std::string> robotsTxt = conn.getRobots();
    std::vector<std::string> title = htmlParser.getTitle();
    if (title.size() == 0) {
        success->insert({url, false});
        return;
    }
    if (!isEnglish(title[0])) {
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

    m->lock();
    // pthread_mutex_lock(m);
    // robotsUrls->push_back(temp);
    for (auto newUrl : htmlParser.getUrls()) {
        std::string u = newUrl.url;
        if (u[0] == '/') {
            u = get_base_url(url) + u;
        }
        spdlog::info(u);

        if (u.compare(0, 5, "https") != 0) {
            continue;
        }
        newUrls->push_back(u);
    }
    // pthread_mutex_unlock(m);
    m->unlock();
    success->insert({url, true});
}

Crawly::Crawly(std::string serverIp, int serverPort, std::string outputDir, int startDocNum) : 
    _client(Client(serverIp, serverPort)),
    // _threads(ThreadPool(numThreads)),
    _frontierIp(serverIp),
    _frontierPort(serverPort),
    _outputDir(outputDir),
    _docNum(startDocNum) {
    _logFile.open(outputDir + "/logs.txt");
    if (!_logFile) {
        spdlog::error("Error opening logfile");
        return;
    }
    _logFile << "Error urls\n";
}

Crawly::~Crawly() {
    spdlog::info("{} successful out of {} received", _numSuccessful, _numReceived);
    spdlog::info("Left off at {}", _docNum);
    _logFile.flush();
    _logFile.close();
}


void Crawly::start() {
    // Send message to get inital set of urls
    spdlog::info("Sent initial message");
    FrontierMessage initMessage{FrontierMessageType::START, {}, {}};
    _client.SendMessage(FrontierInterface::Encode(initMessage));

    while (true) {
        std::optional<Message> response = _client.GetMessageBlocking();
        if (!response) {
            spdlog::info("Error contacting frontier");
            while (true) {
                try {
                    spdlog::info("Trying to connect to frontier");
                    _client = Client(_frontierIp, _frontierPort);
                    break;
                } catch (const std::runtime_error& e) {
                    spdlog::error("Failed to connect to frontier exiting");
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                    exit(1);
                }
            }
            spdlog::info("Connected to frontier, sending init messsage");
            _client.SendMessage(FrontierInterface::Encode(initMessage));
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }
        FrontierMessage decoded = FrontierInterface::Decode(response->msg);
        if (decoded.type == FrontierMessageType::END) {
            break;
        }

        // Set up for sending tasks to worker threads
        auto newUrls = std::make_shared<std::vector<std::string>>();
        auto robotsUrls = std::make_shared<std::vector<std::string>>();
        // pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        auto success =
            std::make_shared<std::unordered_map<std::string, bool>>();
        auto tryAgain = std::make_shared<std::unordered_map<std::string, bool>>();
        std::mutex m;
        std::vector<std::thread> threads;
        for (auto url : decoded.urls) {
            // _threads.queue(
            //     Task(parseHtml, url, newUrls, robotsUrls, success, _numReceived, &mutex, _outputDir));
            threads.emplace_back(parseHtml, url, newUrls, robotsUrls, success, tryAgain, _docNum, &m, _outputDir);
            _docNum++;
            ++_numReceived;
        }
        // _threads.wait();
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }

        int batchSuccessCount = 0;
        for (auto [url, success] : *success) {
            if (!success) {
                spdlog::error("Error getting {}", url);
                _logFile << url << "\n";
            } else {
                batchSuccessCount++;
                _numSuccessful++;
            }
        }

        std::vector<std::string> failed;
        // for (auto [url, again] : *tryAgain) {
        //     if (again) {
        //         failed.push_back(url);
        //     }
        // }

        _client.SendMessage(FrontierInterface::Encode(FrontierMessage{FrontierMessageType::URLS, *newUrls, failed}));
        _logFile.flush();

        spdlog::info("Batch success rate {}/{}", batchSuccessCount, decoded.urls.size());
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

    program.add_argument("-s", "--startdocnum")
        .default_value(0)
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
    int startDocumentNum = program.get<int>("-s");

    spdlog::info("Server IP {}", serverIp);
    spdlog::info("Server port {}", serverPort);
    spdlog::info("Output directory {}", outputDir);
    spdlog::info("Start url number {}", startDocumentNum);

    Crawly crawly(serverIp, serverPort, outputDir, startDocumentNum);

    spdlog::info("======= Crawly Started =======");
    crawly.start();
}
