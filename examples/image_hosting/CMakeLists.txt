add_executable(image_hosting
    main.cpp
    ImageHosting.cpp
    ${CMAKE_SOURCE_DIR}/src/core/EventLoop.cpp
    ${CMAKE_SOURCE_DIR}/src/core/TcpServer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Connection.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/TimeWheel.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpServer.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpRequest.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpResponse.cpp
    ${CMAKE_SOURCE_DIR}/src/http/Router.cpp
    ${CMAKE_SOURCE_DIR}/src/util/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/util/ThreadPool.cpp
)

target_include_directories(image_hosting PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(image_hosting http_parser pthread)
