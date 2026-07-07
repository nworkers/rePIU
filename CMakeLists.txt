cmake_minimum_required(VERSION 3.20)

project(rePIU LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(repiu_exe
    src/exe/dos4gw_loader.cpp
    src/exe/executable_headers.cpp
    src/hle/hle_profile.cpp
    src/target/target_profile.cpp
)

target_include_directories(repiu_exe
    PUBLIC
        include
)

add_executable(repiu_exe_analyzer
    src/tools/exe_analyzer/main.cpp
)

target_link_libraries(repiu_exe_analyzer
    PRIVATE
        repiu_exe
)
