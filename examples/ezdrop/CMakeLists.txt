add_executable(ezdrop
    main.cpp
    EzdropServer.cpp
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

target_include_directories(ezdrop PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ezdrop http_parser pthread)

# 构建时将 static/ 目录复制到输出目录（便于运行时定位）
add_custom_command(TARGET ezdrop POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/static
    $<TARGET_FILE_DIR:ezdrop>/static
    COMMENT "Copying static files to output directory"
)