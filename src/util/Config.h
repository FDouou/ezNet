#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace ezNet {

class Config {
public:
    Config();
    explicit Config(const std::string& filepath);

    bool load(const std::string& filepath);

    std::string get(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;

    uint16_t httpPort;
    uint16_t udpPort;
    uint16_t echoTcpPort;
    std::string logLevel;

private:
    void applyDefaults();

    std::unordered_map<std::string, std::string> values_;
};

} // namespace ezNet
