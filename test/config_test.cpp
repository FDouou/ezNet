#include "util/Config.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>

using namespace ezNet;

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << msg << std::endl; g_fail++; } \
    else { g_pass++; } \
} while(0)

void test_default_values() {
    Config config;
    EXPECT(config.httpPort == 8080, "default: httpPort=8080");
    EXPECT(config.udpPort == 8081, "default: udpPort=8081");
    EXPECT(config.echoTcpPort == 8082, "default: echoTcpPort=8082");
    EXPECT(config.logLevel == "INFO", "default: logLevel=INFO");
}

void test_get_with_default() {
    Config config;
    EXPECT(config.get("nonexistent", "fallback") == "fallback", "get: returns default for missing key");
}

void test_get_int_with_default() {
    Config config;
    EXPECT(config.getInt("nonexistent", 42) == 42, "getInt: returns default for missing key");
}

void test_load_missing_file() {
    Config config;
    bool ok = config.load("/tmp/eznet_nonexistent_12345.ini");
    EXPECT(!ok, "load: returns false for missing file");
}

void test_load_basic() {
    std::string path = "/tmp/eznet_test_basic.ini";
    std::ofstream ofs(path);
    ofs << "# comment line\n";
    ofs << "httpPort = 9090\n";
    ofs << "udpPort = 9091\n";
    ofs << "logLevel = DEBUG\n";
    ofs.close();

    Config config(path);
    EXPECT(config.httpPort == 9090, "load: httpPort from file");
    EXPECT(config.udpPort == 9091, "load: udpPort from file");
    EXPECT(config.logLevel == "DEBUG", "load: logLevel from file");
    EXPECT(config.echoTcpPort == 8082, "load: echoTcpPort uses default");
}

void test_load_case_insensitive() {
    std::string path = "/tmp/eznet_test_case.ini";
    std::ofstream ofs(path);
    ofs << "HTTPPORT = 7070\n";
    ofs << "LogLevel = TRACE\n";
    ofs.close();

    Config config(path);
    EXPECT(config.httpPort == 7070, "load: case insensitive key");
    EXPECT(config.logLevel == "TRACE", "load: case insensitive logLevel");
}

void test_load_comments_and_blanks() {
    std::string path = "/tmp/eznet_test_comments.ini";
    std::ofstream ofs(path);
    ofs << "\n";
    ofs << "# this is a comment\n";
    ofs << "; also a comment\n";
    ofs << "httpPort = 6060\n";
    ofs << "\n";
    ofs.close();

    Config config(path);
    EXPECT(config.httpPort == 6060, "load: skip comments and blanks");
}

void test_load_whitespace_trimming() {
    std::string path = "/tmp/eznet_test_trim.ini";
    std::ofstream ofs(path);
    ofs << "  httpPort  =  5050  \n";
    ofs << "logLevel =  WARN \n";
    ofs.close();

    Config config(path);
    EXPECT(config.httpPort == 5050, "load: trim whitespace around key/value");
    EXPECT(config.logLevel == "WARN", "load: trim whitespace around logLevel");
}

void test_get_after_load() {
    std::string path = "/tmp/eznet_test_get.ini";
    std::ofstream ofs(path);
    ofs << "myKey = myValue\n";
    ofs << "myInt = 123\n";
    ofs.close();

    Config config(path);
    EXPECT(config.get("mykey") == "myValue", "get: loaded value");
    EXPECT(config.getInt("myint") == 123, "getInt: loaded integer");
}

void test_get_int_invalid() {
    std::string path = "/tmp/eznet_test_invalid.ini";
    std::ofstream ofs(path);
    ofs << "badInt = notanumber\n";
    ofs.close();

    Config config(path);
    EXPECT(config.getInt("badint", 99) == 99, "getInt: invalid number returns default");
}

int main() {
    std::cout << "=== Config Tests ===" << std::endl;

    test_default_values();
    test_get_with_default();
    test_get_int_with_default();
    test_load_missing_file();
    test_load_basic();
    test_load_case_insensitive();
    test_load_comments_and_blanks();
    test_load_whitespace_trimming();
    test_get_after_load();
    test_get_int_invalid();

    std::cout << "Passed: " << g_pass << ", Failed: " << g_fail << std::endl;
    return g_fail > 0 ? 1 : 0;
}
