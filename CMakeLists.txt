cmake_minimum_required(VERSION 3.10)
project(WebSocketClient)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找OpenSSL
find_package(OpenSSL REQUIRED)

# 查找zlib（可选）
find_package(ZLIB)

# 设置编译选项
if(ZLIB_FOUND)
    add_definitions(-DUSE_ZLIB)
    message(STATUS "Zlib found, compression enabled")
else()
    message(STATUS "Zlib not found, compression disabled")
endif()

# 设置源文件
set(EXAMPLE_SOURCES example.cpp)
set(TEST_SOURCES test.cpp)
set(PERFORMANCE_SOURCES performance_test.cpp)

# 创建示例可执行文件
add_executable(websocket_example ${EXAMPLE_SOURCES})

# 创建测试可执行文件
add_executable(websocket_test ${TEST_SOURCES})

# 创建性能测试可执行文件
add_executable(websocket_performance ${PERFORMANCE_SOURCES})

# 链接库
target_link_libraries(websocket_example 
    OpenSSL::SSL 
    OpenSSL::Crypto
)

target_link_libraries(websocket_test 
    OpenSSL::SSL 
    OpenSSL::Crypto
)

target_link_libraries(websocket_performance 
    OpenSSL::SSL 
    OpenSSL::Crypto
)

if(ZLIB_FOUND)
    target_link_libraries(websocket_example ZLIB::ZLIB)
    target_link_libraries(websocket_test ZLIB::ZLIB)
    target_link_libraries(websocket_performance ZLIB::ZLIB)
endif()

# 在Windows上链接ws2_32库
if(WIN32)
    target_link_libraries(websocket_example ws2_32)
    target_link_libraries(websocket_test ws2_32)
    target_link_libraries(websocket_performance ws2_32)
endif()

# 设置包含目录
target_include_directories(websocket_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(websocket_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(websocket_performance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 编译选项
if(MSVC)
    target_compile_options(websocket_example PRIVATE /W4)
    target_compile_options(websocket_test PRIVATE /W4)
    target_compile_options(websocket_performance PRIVATE /W4)
else()
    target_compile_options(websocket_example PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(websocket_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(websocket_performance PRIVATE -Wall -Wextra -Wpedantic)
endif()