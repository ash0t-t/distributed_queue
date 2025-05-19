#pragma once
#include <unordered_map>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <cpp-httplib/httplib.h>

class Server {
public:
    Server(int port, const std::string& instances_file);
    void run();

private:
    void setup_routes();
    void sync_post(const std::string& queue, const std::string& value);
    void sync_get(const std::string& queue);
    void fetch_existing_data();
    
    static std::pair<std::string, int> parse_instance(const std::string& instance);
    int port_;
    std::string self_host_;
    std::string instances_file_;
    std::vector<std::string> instances_;
    std::unordered_map<std::string, std::deque<std::string>> queues_;
    std::recursive_mutex mutex_;
    httplib::Server svr_;
};
