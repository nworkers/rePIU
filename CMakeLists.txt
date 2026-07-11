cmake_minimum_required(VERSION 3.20)

project(rePIU LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(spdlog CONFIG QUIET)
if(NOT spdlog_FOUND)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

function(repiu_configure_win32_loader_host target_name)
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
    src/exe/dos16m_bound_module.cpp
    src/exe/dos4gw_loader.cpp
    src/exe/executable_headers.cpp
    src/hle/dos_file_system.cpp
    src/hle/glide_hle.cpp
    src/hle/hle_dispatcher.cpp
    src/hle/hle_profile.cpp
    src/hle/linexe_call_gate.cpp
    src/hle/privileged_instruction.cpp
    src/platform/win32/execution_trampoline.cpp
    src/platform/win32/glide_opengl_backend.cpp
    src/platform/win32/glide_opengl_shader.cpp
    src/platform/win32/x87_context.cpp
    src/platform/win32/runtime_memory_policy.cpp
    src/runtime/guest_context.cpp
    src/runtime/dos_low_memory.cpp
    src/runtime/image_address.cpp
    src/runtime/runtime_memory_arena.cpp
    src/runtime/runtime_memory.cpp
    src/runtime/selector_table.cpp
    src/target/target_profile.cpp
)

target_include_directories(repiu_exe
    PUBLIC
        include
)

add_executable(repiu_exe_analyzer
    src/tools/exe_analyzer/main.cpp
)

add_executable(repiu_loader_win32
    src/host/win32/main.cpp
)

if(WIN32)
    add_executable(repiu_supervisor_win32
        src/host/win32/supervisor_main.cpp
    )
    target_include_directories(repiu_supervisor_win32
        PRIVATE
            include
    )
endif()

target_link_libraries(repiu_exe_analyzer
    PRIVATE
        repiu_exe
)

target_link_libraries(repiu_loader_win32
    PRIVATE
        repiu_exe
        spdlog::spdlog_header_only
)

if(WIN32)
    target_link_libraries(repiu_exe
        PUBLIC
            opengl32
            gdi32
            user32
    )
endif()

repiu_configure_win32_loader_host(repiu_exe_analyzer)
repiu_configure_win32_loader_host(repiu_loader_win32)
