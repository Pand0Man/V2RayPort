cmake_minimum_required(VERSION 3.20)
project(V2RayPort LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
  message(FATAL_ERROR "V2RayPort currently targets Windows only.")
endif()

add_executable(V2RayPort WIN32 src/main.cpp)

target_compile_definitions(V2RayPort PRIVATE UNICODE _UNICODE)

target_link_libraries(V2RayPort PRIVATE comctl32 shell32 msimg32)
