#include "config_parser.h"
#include <fstream>
#include <sstream>
#include <cctype>

// Утилитная функция для удаления пробелов в начале и в конце строки
std::string ConfigParser::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t");
    size_t end = str.find_last_not_of(" \t");
    return (start == std::string::npos || end == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

// Загружает конфигурацию из файла
bool ConfigParser::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';') {
            // Пропускаем пустые строки и комментарии
            continue;
        }
        if (line[0] == '[' && line.back() == ']') {
            // Обрабатываем секции
            current_section = trim(line.substr(1, line.size() - 2));
        }
        else {
            // Обрабатываем ключи и значения
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = trim(line.substr(0, pos));
                std::string value = trim(line.substr(pos + 1));
                config_[current_section][key] = value;
            }
        }
    }
    return true;
}

// Получает значение из конфигурации
std::string ConfigParser::get(const std::string& section, const std::string& key) const {
    auto section_it = config_.find(section);
    if (section_it != config_.end()) {
        auto key_it = section_it->second.find(key);
        if (key_it != section_it->second.end()) {
            return key_it->second;
        }
    }
    return "";
}
