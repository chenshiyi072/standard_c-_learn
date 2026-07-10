cmake_minimum_required(VERSION 3.10)
project(TCPClientStatePattern LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件
set(SOURCES
    src/TCPClient.cpp
    src/ConcreteStates.cpp
)

# 创建库并设置包含目录
add_library(tcp_client_lib ${SOURCES})
target_include_directories(tcp_client_lib PUBLIC ${PROJECT_SOURCE_DIR}/include)

# 使用 CMake 的 Threads 目标（在 Windows 下对 MSVC 是空操作，在 MinGW 等会找到 pthreads）
find_package(Threads REQUIRED)
target_link_libraries(tcp_client_lib PUBLIC Threads::Threads)

# Windows 下需要链接 Winsock 库
if (WIN32)
  target_compile_definitions(tcp_client_lib PUBLIC WIN32_LEAN_AND_MEAN)
  target_link_libraries(tcp_client_lib PUBLIC ws2_32)
endif()

# 可执行文件
add_executable(tcp_client_demo main.cpp)
target_link_libraries(tcp_client_demo PRIVATE tcp_client_lib)
