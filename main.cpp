#include "httplib.h"
#include "config_parser.h"
#include <unordered_map>
#include <string>
#include <sstream>
#include <random>
#include <chrono>
#include <mutex>
#include <thread>
#include <iostream>

struct StoredData {
    std::string text;
    std::chrono::steady_clock::time_point expiry_time;
};

std::unordered_map<std::string, StoredData> storage;
std::mutex storage_mutex;

std::string generate_id() {
    static std::mt19937 gen{ std::random_device{}() };
    std::uniform_int_distribution<> dist(0, 61);
    std::string id;
    for (int i = 0; i < 8; ++i) {
        int r = dist(gen);
        if (r < 26)
            id += ('a' + r);
        else if (r < 52)
            id += ('A' + (r - 26));
        else
            id += ('0' + (r - 52));
    }
    return id;
}

void cleanup_expired_data() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::minutes(1));
        std::lock_guard<std::mutex> guard(storage_mutex);
        auto now = std::chrono::steady_clock::now();
        for (auto it = storage.begin(); it != storage.end();) {
            if (it->second.expiry_time < now) {
                it = storage.erase(it);
            }
            else {
                ++it;
            }
        }
    }
}

int main() {
    ConfigParser config;
    if (!config.load("kmi.config")) {
        std::cerr << "Error loading configuration file." << std::endl;
        return 1;
    }

    std::string host = config.get("KMI.WebServer", "Host");
    int port = std::stoi(config.get("KMI.WebServer", "Port"));
    int time_to_expired = std::stoi(config.get("KMI.Settings", "TimeToExpired"));

    httplib::Server svr;

    // Запуск потока для очистки устаревших данных
    std::thread(cleanup_expired_data).detach();

    // Корневой маршрут для сохранения текста и генерации ссылки
    svr.Post("/", [&time_to_expired](const httplib::Request& req, httplib::Response& res) {
        auto it = req.files.find("kmi");
        if (it == req.files.end()) {
            res.status = 400;
            res.set_content("No 'kmi' field provided", "text/plain");
            return;
        }

        std::string text = it->second.content;
        std::string id = generate_id();
        auto expiry_time = std::chrono::steady_clock::now() + std::chrono::seconds(time_to_expired);

        {
            std::lock_guard<std::mutex> guard(storage_mutex);
            storage[id] = { text, expiry_time };
        }

        std::ostringstream os;
        os << "http://" << req.get_header_value("Host") << "/" << id;
        res.set_content(os.str(), "text/plain");
        });

    // Маршрут для получения текста по ID
    svr.Get(R"(/(\w+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        std::lock_guard<std::mutex> guard(storage_mutex);
        auto it = storage.find(id);
        if (it != storage.end()) {
            res.set_content(it->second.text, "text/plain");
        }
        else {
            res.status = 404;
            res.set_content("Not found", "text/plain");
        }
        });

    std::cout << "Server is running on " << host << ":" << port << std::endl;
    svr.listen(host.c_str(), port);
}
