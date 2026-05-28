cmake_minimum_required(VERSION 3.16)
project(ezNet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(http_parser STATIC third_party/http-parser/http_parser.c)
target_include_directories(http_parser PUBLIC third_party/http-parser)

add_executable(ezNet
    src/main.cpp
    src/core/EventLoop.cpp
    src/core/TcpServer.cpp
    src/core/UdpServer.cpp
    src/core/Connection.cpp
    src/core/Buffer.cpp
    src/core/Timer.cpp
    src/http/HttpServer.cpp
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/http/Router.cpp
    src/udp/UdpEcho.cpp
    src/udp/CustomProtocol.cpp
    src/util/Logger.cpp
    src/util/Config.cpp
)

target_include_directories(ezNet PUBLIC src)
target_link_libraries(ezNet http_parser)

# tests
add_executable(eventloop_test
    test/eventloop_test.cpp
    src/core/EventLoop.cpp
    src/util/Logger.cpp
)
target_include_directories(eventloop_test PUBLIC src)
target_link_libraries(eventloop_test pthread)

add_executable(buffer_test
    test/buffer_test.cpp
    src/core/Buffer.cpp
)
target_include_directories(buffer_test PUBLIC src)

add_executable(tcp_server_test
    test/tcp_server_test.cpp
    src/core/EventLoop.cpp
    src/core/TcpServer.cpp
    src/core/Connection.cpp
    src/core/Buffer.cpp
    src/util/Logger.cpp
)
target_include_directories(tcp_server_test PUBLIC src)
target_link_libraries(tcp_server_test pthread)

add_executable(udp_server_test
    test/udp_server_test.cpp
    src/core/EventLoop.cpp
    src/core/UdpServer.cpp
    src/core/Buffer.cpp
    src/util/Logger.cpp
)
target_include_directories(udp_server_test PUBLIC src)
target_link_libraries(udp_server_test pthread)
