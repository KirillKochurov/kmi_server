#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <string>
#include <unordered_map>

class ConfigParser {
public:
    // Загружает конфигурацию из файла
    bool load(const std::string& filename);

    // Получает значение из конфигурации
    std::string get(const std::string& section, const std::string& key) const;

private:
    // Хранит конфигурацию в виде секций и ключей
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> config_;

    // Утилитные функции для обработки строки
    std::string trim(const std::string& str) const;
};

#endif // CONFIG_PARSER_H
