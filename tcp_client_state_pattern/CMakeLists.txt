cmake_minimum_required(VERSION 3.10)
project(TCPClientStatePattern)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 源文件
set(SOURCES
    src/TCPClient.cpp
    src/ConcreteStates.cpp
)

# 创建库
add_library(tcp_client_lib ${SOURCES})

# 创建可执行程序（示例）
add_executable(tcp_client_demo main.cpp)
target_link_libraries(tcp_client_demo tcp_client_lib pthread)
