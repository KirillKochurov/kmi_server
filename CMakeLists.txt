cmake_minimum_required(VERSION 3.10)
project(kmi_server)

# Укажите стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Укажите файлы исходного кода
add_executable(kmi_server main.cpp config_parser.cpp)

# Линковка с библиотекой ws2_32
target_link_libraries(kmi_server PRIVATE ws2_32)
