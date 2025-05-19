#include "server.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::pair<std::string, int> Server::parse_instance(const std::string &instance)
{
    size_t colon = instance.find(':');
    std::string host = instance.substr(0, colon);
    int port = std::stoi(instance.substr(colon + 1));
    return {host, port};
}

Server::Server(int port, const std::string &instances_file)
    : port_(port), instances_file_(instances_file)
{
    self_host_ = "127.0.0.1:" + std::to_string(port_);
    std::ifstream f(instances_file_);
    json j;
    f >> j;
    for (const auto &inst : j["instances"]) {
        std::string instance_str = inst.get<std::string>();
        if (instance_str != self_host_) {
            instances_.push_back(instance_str);
        }
    }

    std::cout << "[INFO] Loaded instances:\n";
    for (const auto &inst : instances_) {
        std::cout << "  - " << inst << std::endl;
    }
    std::cout << "[INFO] Self host: " << self_host_ << std::endl;
}

void Server::run()
{
    setup_routes();
    fetch_existing_data();
    std::cout << "Server running on " << self_host_ << std::endl;
    svr_.listen("0.0.0.0", port_);
}

void Server::setup_routes()
{
    svr_.Get("/sync_data", [this](const httplib::Request &, httplib::Response &res) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        json data;
        for (const auto& [queue, messages] : queues_) {
            data[queue] = messages;
        }
        res.set_content(data.dump(), "application/json");
    });

    svr_.Post(R"(/(\w+))", [this](const httplib::Request &req, httplib::Response &res) {
    bool is_sync = req.has_header("X-Origin");
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::string queue = req.matches[1];
    queues_[queue].push_back(req.body);
    std::cout << "[POST] " << queue << " <= \"" << req.body << "\"" << std::endl;
    std::cout << "[DEBUG] Current state of queues_:" << std::endl;
    for (const auto& [key, values] : queues_) {
        std::cout << "  Queue: " << key << ", Values: ";
        for (const auto& value : values) {
            std::cout << value << " ";
        }
        std::cout << std::endl;
    }
    if (!is_sync) {
        sync_post(queue, req.body);
    }
    res.status = 204; 
    });

    svr_.Get(R"(/(\w+))", [this](const httplib::Request &req, httplib::Response &res) {
    bool is_sync = req.has_header("X-Origin");
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::string queue = req.matches[1];
    if (queues_[queue].empty()) {
        std::cout << "[GET] " << queue << " => EMPTY" << std::endl;
        res.status = 404;
        return;
    }
    std::string value = queues_[queue].front();
    queues_[queue].pop_front();
    std::cout << "[GET] " << queue << " => \"" << value << "\"" << std::endl;
    res.set_content(value, "text/plain");
    if (!is_sync) {
        sync_get(queue);
    } 
    });

    svr_.Delete(R"(/(\w+))", [this](const httplib::Request &req, httplib::Response &res) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        std::string queue = req.matches[1];
        std::cout << "[DELETE] " << queue << " removed" << std::endl;
        queues_.erase(queue);
        res.status = 204; 
    });
}

void Server::sync_post(const std::string &queue, const std::string &value)
{
    for (const auto &instance : instances_) {
        std::cout << "[SYNC_POST] Sending to " << instance << " /" << queue << std::endl;
        auto [host, port] = parse_instance(instance);
        httplib::Client cli(host, port);
        cli.set_connection_timeout(1, 0);
        cli.set_read_timeout(2, 0);  
        httplib::Headers headers = {{"X-Origin", self_host_}};
        auto res = cli.Post(("/" + queue).c_str(), headers, value, "text/plain");
        if (!res) {
            std::cout << "[SYNC_POST] Failed to reach " << instance << std::endl;
        }
    }
}

void Server::sync_get(const std::string &queue)
{
    for (const auto &instance : instances_) {
        std::cout << "[SYNC_GET] Informing " << instance << " about GET /" << queue << std::endl;
        auto [host, port] = parse_instance(instance);
        httplib::Client cli(host, port);
        cli.set_connection_timeout(1, 0);
        cli.set_read_timeout(2, 0);  
        httplib::Headers headers = {{"X-Origin", self_host_}};
        cli.Get(("/" + queue).c_str(), headers);
    }
}

void Server::fetch_existing_data()
{
    for (const auto &instance : instances_) {
        if (instance == self_host_) continue;

        auto [host, port] = parse_instance(instance);
        httplib::Client cli(host, port);
        cli.set_connection_timeout(1, 0);
        cli.set_read_timeout(2, 0);  
        auto res = cli.Get("/sync_data");

        if (res && res->status == 200) {
            try {
                auto data = json::parse(res->body);
                std::lock_guard<std::recursive_mutex> lock(mutex_);
                for (auto &[qname, messages] : data.items()) {
                    for (auto &msg : messages) {
                        queues_[qname].push_back(msg.get<std::string>());
                    }
                }
                std::cout << "[SYNC] Data synced from " << instance << std::endl;
            }
            catch (...) {
                std::cerr << "[SYNC] Failed to parse sync data from " << instance << std::endl;
            }
        }
    }
}