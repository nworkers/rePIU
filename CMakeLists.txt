cmake_minimum_required(VERSION 3.20)

project(rePIU LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(repiu_configure_win32_execution_host target_name)
    if(MSVC AND WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_link_options(${target_name}
            PRIVATE
                /BASE:0x10000000
                /DYNAMICBASE:NO
        )
        target_compile_definitions(${target_name}
            PRIVATE
                REPIU_WIN32_HOST_IMAGE_BASE=0x10000000
        )
    endif()
endfunction()

add_library(repiu_exe
    src/exe/dos4gw_loader.cpp
    src/exe/executable_headers.cpp
    src/hle/hle_profile.cpp
    src/platform/win32/runtime_memory_policy.cpp
    src/runtime/runtime_memory.cpp
    src/target/target_profile.cpp
)

target_include_directories(repiu_exe
    PUBLIC
        include
)

add_executable(repiu_exe_analyzer
    src/tools/exe_analyzer/main.cpp
)

add_executable(repiu_win32_execution_host
    src/tools/win32_execution_host/main.cpp
)

target_link_libraries(repiu_exe_analyzer
    PRIVATE
        repiu_exe
)

target_link_libraries(repiu_win32_execution_host
    PRIVATE
        repiu_exe
)

repiu_configure_win32_execution_host(repiu_exe_analyzer)
repiu_configure_win32_execution_host(repiu_win32_execution_host)
