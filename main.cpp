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

std::unordered_map<std::string, int> request_count;
std::mutex request_count_mutex;

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

        // Очищаем счетчики запросов
        {
            std::lock_guard<std::mutex> guard(request_count_mutex);
            request_count.clear();
        }
    }
}

std::string render_home_page() {
    std::ostringstream html;

    // HTML страница, подобная той, что показана на изображении
    html << "<!DOCTYPE html>\n<html>\n<head>\n<title>KMI</title>\n<style>\n"
        "body { background-color: #2E3440; color: #D8DEE9; font-family: monospace; }\n"
        "h1 { text-align: center; }\n"
        ".content { margin: 20px; }\n"
        "pre { background-color: #3B4252; padding: 15px; border-radius: 5px; }\n"
        "</style>\n</head>\n<body>\n"
        "<h1>KMI</h1>\n<div class='content'>\n<pre>\n"
        "KMI(1)                       KMI                       kmi(1)\n"
        "\n"
        "NAME\n"
        "    kmi: command line pastebin — anonymous, fast, secure\n"
        "\n"
        "SYNOPSIS\n"
        "    <command> | curl -F 'kmi=<-' https://kmi.unknoown.ru/\n"
        "\n"
        "DESCRIPTION\n"
        "    running on https://unknownhostinglab.ru/ hosting.\n"
        "    has the following features —\n"
        "    * you can also find out your ip address or ping (microseconds) from the following links\n"
        "        https://kmi.unknoown.ru/ip\n"
        "        https://kmi.unknoown.ru/ping\n"
        "    * KMI also available through IPv6\n"
        "\n"
        "LIMITS\n"
        "    storage time unlimited\n"
        "    data can be pruned at any time\n"
        "    max post size limit 512kb\n"
        "    limit uploads 10 per minute\n"
        "    posts are kept for 12 hours\n"
        "\n"
        "EXAMPLES\n"
        "    ~$ cat hello-world.c | curl -F 'kmi=<-' https://kmi.unknoown.ru\n"
        "    ~$ echo 'Hello World' | curl - F 'kmi=<-' https://kmi.unknoown.ru\n"
        "\n"
        "</pre>\n"
        "CREATE PASTE\n"
        "<form method='POST' action='/' enctype='multipart/form-data'>\n"
        "<textarea name='kmi' rows='10' cols='60'></textarea><br>\n"
        "<input type='submit' value='Submit'>\n"
        "</form>\n</div>\n</body>\n</html>";

    return html.str();
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

    // Корневой маршрут для рендера HTML страницы
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(render_home_page(), "text/html");
        });

    // Страница для возврата IP-адреса клиента
    svr.Get("/ip", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(req.remote_addr, "text/plain");
        });

    // Страница для возврата времени отклика сервера в микросекундах
    svr.Get("/ping", [](const httplib::Request& req, httplib::Response& res) {
        auto start = std::chrono::high_resolution_clock::now();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        res.set_content(std::to_string(duration) + " microseconds", "text/plain");
        });

    svr.Post("/", [&time_to_expired](const httplib::Request& req, httplib::Response& res) {
        std::string text;

        // Проверяем, пришли ли данные через файл (curl -F)
        auto it = req.files.find("kmi");
        if (it != req.files.end()) {
            text = it->second.content;
        }
        // Если нет, проверяем, пришли ли данные как текст (через форму)
        else if (!req.has_param("kmi")) {
            res.status = 400;
            res.set_content("No 'kmi' field provided", "text/plain");
            return;
        }
        else {
            text = req.get_param_value("kmi");
        }

        // Генерация уникального ID
        std::string id = generate_id();
        auto expiry_time = std::chrono::steady_clock::now() + std::chrono::seconds(time_to_expired);

        // Сохраняем данные в хранилище
        {
            std::lock_guard<std::mutex> guard(storage_mutex);
            storage[id] = { text, expiry_time };
        }

        // Формируем ответ с ссылкой на сохраненные данные
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
