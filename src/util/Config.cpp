#include "util/Config.h"
#include "util/Logger.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace ezNet {

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

static std::string toLower(const std::string& s) {
    std::string result = s;
    for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}

Config::Config() {
    applyDefaults();
}

Config::Config(const std::string& filepath) {
    applyDefaults();
    if (!load(filepath)) {
        LOG_ERROR("Failed to load config file: %s", filepath.c_str());
    }else{
        LOG_INFO("Successfully loaded config file: %s", filepath.c_str());
    }
}

bool Config::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;
        if (trimmed[0] == '#' || trimmed[0] == ';') continue;

        size_t eqPos = trimmed.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eqPos));
        std::string value = trim(trimmed.substr(eqPos + 1));

        if (key.empty()) continue;

        values_[toLower(key)] = value;
    }

    httpPort = static_cast<uint16_t>(getInt("httpport", httpPort));
    udpPort = static_cast<uint16_t>(getInt("udpport", udpPort));
    echoTcpPort = static_cast<uint16_t>(getInt("echotcpport", echoTcpPort));
    logLevel = get("loglevel", logLevel);

    return true;
}

std::string Config::get(const std::string& key, const std::string& defaultValue) const {
    auto it = values_.find(toLower(key));
    if (it != values_.end()) return it->second;
    return defaultValue;
}

int Config::getInt(const std::string& key, int defaultValue) const {
    std::string val = get(key);
    if (val.empty()) return defaultValue;
    try {
        return std::stoi(val);
    } catch (...) {
        return defaultValue;
    }
}

void Config::applyDefaults() {
    httpPort = 8080;
    udpPort = 8081;
    echoTcpPort = 8082;
    logLevel = "INFO";
}

} // namespace ezNet
