cmake_minimum_required(VERSION 3.20)

project(rePIU LANGUAGES C CXX)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" REPIU_VERSION
    LIMIT_COUNT 1)
string(STRIP "${REPIU_VERSION}" REPIU_VERSION)
if(NOT REPIU_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must use major.minor.patch format")
endif()

include(FetchContent)

set(INSTALL_STATIC_LIBS OFF CACHE BOOL "" FORCE)
set(CHDR_WANT_RAW_DATA_SECTOR ON CACHE BOOL "" FORCE)
set(CHDR_WANT_SUBCODE OFF CACHE BOOL "" FORCE)
# libchdr already bundles miniz. Turning on its ZIP reader lets us extract MAME
# PIU10 ROM-set members without vendoring a second copy of miniz.
# The file-based ZIP APIs stay off, so archives are read through memory buffers.
set(MINIZ_ARCHIVE_APIS ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/libchdr EXCLUDE_FROM_ALL)
set(REPIU_MINIZ_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libchdr/deps/miniz-3.1.1")

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    sdl3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.10
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(sdl3)

FetchContent_Declare(
    minimp3
    GIT_REPOSITORY https://github.com/lieff/minimp3.git
    GIT_TAG ea99364f61c14656440e8d77e9c233ccf3124633
)
FetchContent_MakeAvailable(minimp3)

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

# Task 412: Release keeps every optimisation flag and gains debug information
# only, so code generation is unchanged while host-side samples in the guest
# position census can be resolved to function names. Declared before any target
# so it applies to all of them.
if(MSVC)
    add_compile_options($<$<CONFIG:Release>:/Zi>)
    add_link_options($<$<CONFIG:Release>:/DEBUG>
                     $<$<CONFIG:Release>:/OPT:REF>
                     $<$<CONFIG:Release>:/OPT:ICF>)
endif()

add_library(repiu_zydis STATIC
    third_party/zydis/Zydis.c
)
target_include_directories(repiu_zydis
    PUBLIC
        third_party/zydis
)
target_compile_definitions(repiu_zydis
    PUBLIC
        ZYDIS_STATIC_BUILD
)

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
    src/assets/piu_chd_mount.cpp
    src/assets/rom_zip_archive.cpp
    src/exe/dos16m_bound_module.cpp
    src/exe/dos4gw_loader.cpp
    src/exe/executable_headers.cpp
    src/hle/dos_file_system.cpp
    src/hle/dos_date.cpp
    src/hle/glide_fog.cpp
    src/hle/glide_hle.cpp
    src/hle/glide_implementation_issue.cpp
    src/hle/glide_lfb.cpp
    src/hle/glide_lfb_region.cpp
    src/hle/glide_texture_decode.cpp
    src/hle/glide_vertex.cpp
    src/hle/glide_vertex_depth_census.cpp
    src/hle/hle_dispatcher.cpp
    src/hle/hle_profile.cpp
    src/hle/linexe_call_gate.cpp
    src/hle/pit_timer.cpp
    src/hle/piu10_isa_board.cpp
    src/hle/privileged_instruction.cpp
    src/media/chd_cd_image.cpp
    src/platform/win32/cd_audio_wave_out.cpp
    src/platform/win32/aot_code_cache_win32.cpp
    src/platform/win32/aot_cache_address_index.cpp
    src/platform/win32/aot_inline_cache_site_index.cpp
    src/platform/win32/aot_return_dispatch_site_index.cpp
    src/platform/win32/aot_return_patch_policy.cpp
    src/platform/win32/aot/aot_worker_timing.cpp
    src/platform/win32/aot/aot_timer_source_profile.cpp
    src/platform/win32/aot_page_coherence_win32.cpp
    src/platform/win32/aot/aot_boundary_provenance.cpp
    src/platform/win32/aot/aot_boundary_reason.cpp
    src/platform/win32/aot/aot_dbt_call_return_trace.cpp
    src/platform/win32/aot/aot_dbt_call_step_probe.cpp
    src/platform/win32/aot/aot_dbt_dispatch.cpp
    src/platform/win32/aot/aot_dbt_direct_edge_dispatch.cpp
    src/platform/win32/aot/aot_dbt_hle_dispatch.cpp
    src/platform/win32/aot/aot_dbt_glide_gate_dispatch.cpp
    src/platform/win32/aot/aot_dbt_indirect_dispatch.cpp
    src/platform/win32/aot/aot_dbt_return_dispatch.cpp
    src/platform/win32/aot/aot_retired_trap_profile.cpp
    src/platform/win32/aot/aot_runtime_dispatch.cpp
    src/platform/win32/bios/bios_keyboard_services.cpp
    src/platform/win32/execution/execution_probe_memory_dump.cpp
    src/platform/win32/execution/execution_trampoline.cpp
    src/platform/win32/execution/single_step_hotspot_profile.cpp
    src/platform/win32/dos/dos_int21_services.cpp
    src/platform/win32/dos/dpmi_mscdex_services.cpp
    src/platform/win32/exception/breakpoint_evidence_win32.cpp
    src/platform/win32/exception/exception_rescue_win32.cpp
    src/platform/win32/exception/host_crash_report.cpp
    src/platform/win32/cpu_emul/guest_memory_access.cpp
    src/platform/win32/cpu_emul/instruction_emulation.cpp
    src/platform/win32/cpu_emul/low_memory_string_access.cpp
    src/platform/win32/boundary/linexe_glide_boundary.cpp
    src/platform/win32/boundary/timer_interrupt_boundary.cpp
    src/platform/win32/telemetry/live_telemetry_snapshot.cpp
    src/platform/win32/telemetry/execution_time_profile.cpp
    src/platform/win32/telemetry/cd_audio_position_census.cpp
    src/platform/win32/telemetry/mscdex_command_trace.cpp
    src/platform/win32/telemetry/guest_position_census.cpp
    src/platform/win32/telemetry/glide_gate_timing.cpp
    src/platform/win32/telemetry/glide_ordinal_timing.cpp
    src/platform/win32/telemetry/glide_buffer_swap_timing.cpp
    src/platform/win32/telemetry/glide_setter_state_model.cpp
    src/platform/win32/telemetry/glide_setter_state_census.cpp
    src/platform/win32/telemetry/glide_async_present.cpp
    src/platform/win32/telemetry/glide_draw_batch.cpp
    src/platform/win32/telemetry/glide_setter_state_cache.cpp
    src/platform/win32/telemetry/glide_setter_phase_timing.cpp
    src/platform/win32/telemetry/timer_tick_delivery.cpp
    src/platform/win32/telemetry/aot_residency_sample.cpp
    src/platform/win32/aot/aot_boundary_opcode_census.cpp
    src/platform/win32/io/port_io_emulator.cpp
    src/platform/win32/io/piu10_mp3_frame_batch.cpp
    src/platform/win32/io/port_io_delay_loop.cpp
    src/platform/win32/io/eeprom_93c46.cpp
    src/platform/win32/io/piu10_sound_port.cpp
    src/platform/win32/ymz280b_audio_out.cpp
    src/platform/win32/piu10_mp3_audio_out.cpp
    src/sound/ymz280b_device.cpp
    src/sound/ymz280b_sample_rom.cpp
    src/sound/dac3350a_control.cpp
    src/sound/decoder_input_fifo.cpp
    src/sound/mpeg_audio_frame.cpp
    src/sound/spsc_byte_ring.cpp
    src/sound/stream_chunk_audit.cpp
    src/platform/win32/native_fast_path.cpp
    src/platform/win32/native_linear_span.cpp
    src/platform/win32/native_phase_sampler.cpp
    src/platform/win32/verified_region_analyzer.cpp
    src/platform/win32/glide_gl_error_policy.cpp
    src/platform/win32/glide_swap_interval_policy.cpp
    src/platform/win32/glide_texture_census.cpp
    src/platform/win32/out_of_arena_step_census.cpp
    src/platform/win32/glide_opengl_backend.cpp
    src/platform/win32/glide_opengl_shader.cpp
    src/platform/win32/x87_context.cpp
    src/platform/win32/runtime_memory_policy.cpp
    src/runtime/guest_context.cpp
    src/runtime/aot_code_cache.cpp
    src/runtime/aot_translation_plan.cpp
    src/runtime/dos_low_memory.cpp
    src/runtime/env_toggle.cpp
    src/runtime/execution_backend.cpp
    src/runtime/execution_timeout.cpp
    src/runtime/image_address.cpp
    src/runtime/runtime_memory_arena.cpp
    src/runtime/runtime_memory.cpp
    src/runtime/selector_table.cpp
    src/target/target_profile.cpp
)

target_include_directories(repiu_exe
    PUBLIC
        include
    PRIVATE
        # win32 execution_trampoline decomposition: local module headers live in
        # subsystem subdirectories but are included by short name ("foo.h").
        src/platform/win32
        src/platform/win32/execution
        src/platform/win32/exception
        src/platform/win32/io
        src/platform/win32/dos
        src/platform/win32/bios
        src/platform/win32/cpu_emul
        src/platform/win32/aot
        src/platform/win32/boundary
        src/platform/win32/telemetry
        # miniz ships inside libchdr, which links it PRIVATE, so the headers do
        # not propagate even though the symbols do.
        ${REPIU_MINIZ_INCLUDE_DIR}
        ${minimp3_SOURCE_DIR}
)

target_compile_definitions(repiu_exe
    PRIVATE
        REPIU_VERSION="${REPIU_VERSION}"
)

target_link_libraries(repiu_exe PRIVATE repiu_zydis chdr-static
    SDL3::SDL3-static)

add_executable(repiu_exe_analyzer
    src/tools/exe_analyzer/main.cpp
)

add_executable(repiu_chd_cd_probe
    src/tools/chd_cd_probe/main.cpp
)

add_executable(repiu_aot_probe
    src/tools/aot_probe/aot_timer_source_profile_probe.cpp
    src/tools/aot_probe/append_phase_benchmark_probe.cpp
    src/tools/aot_probe/arena_view_probe.cpp
    src/tools/aot_probe/boundary_provenance_probe.cpp
    src/tools/aot_probe/dbt_call_return_trace_probe.cpp
    src/tools/aot_probe/dbt_call_step_probe.cpp
    src/tools/aot_probe/dbt_indirect_dispatch_probe.cpp
    src/tools/aot_probe/direct_edge_dispatch_probe.cpp
    src/tools/aot_probe/dbt_return_fallback_probe.cpp
    src/tools/aot_probe/env_toggle_probe.cpp
    src/tools/aot_probe/execution_backend_probe.cpp
    src/tools/aot_probe/execution_timeout_probe.cpp
    src/tools/aot_probe/inline_cache_probe.cpp
    src/tools/aot_probe/jump_table_guard_probe.cpp
    src/tools/aot_probe/native_linear_span_probe.cpp
    src/tools/aot_probe/plan_build_benchmark_probe.cpp
    src/tools/aot_probe/pit_timer_probe.cpp
    src/tools/aot_probe/piu10_isa_board_probe.cpp
    src/tools/aot_probe/retired_trap_profile_probe.cpp
    src/tools/aot_probe/single_step_hotspot_profile_probe.cpp
    src/tools/aot_probe/exception_transition_calibration_probe.cpp
    src/tools/aot_probe/aot_cache_address_index_probe.cpp
    src/tools/aot_probe/aot_inline_cache_site_index_probe.cpp
    src/tools/aot_probe/aot_return_dispatch_site_index_probe.cpp
    src/tools/aot_probe/aot_return_patch_policy_probe.cpp
    src/tools/aot_probe/execution_time_profile_probe.cpp
    src/tools/aot_probe/glide_gate_timing_probe.cpp
    src/tools/aot_probe/glide_ordinal_timing_probe.cpp
    src/tools/aot_probe/glide_buffer_swap_timing_probe.cpp
    src/tools/aot_probe/glide_setter_state_census_probe.cpp
    src/tools/aot_probe/glide_async_present_probe.cpp
    src/tools/aot_probe/glide_draw_batch_probe.cpp
    src/tools/aot_probe/glide_lfb_region_probe.cpp
    src/tools/aot_probe/glide_texture_table_stack_probe.cpp
    src/tools/aot_probe/dos_file_create_probe.cpp
    src/tools/aot_probe/dos_date_probe.cpp
    src/tools/aot_probe/glide_setter_state_cache_probe.cpp
    src/tools/aot_probe/glide_setter_phase_timing_probe.cpp
    src/tools/aot_probe/glide_gl_error_policy_probe.cpp
    src/tools/aot_probe/glide_swap_interval_policy_probe.cpp
    src/tools/aot_probe/veh_exception_gap_probe.cpp
    src/tools/aot_probe/dos_file_handle_cache_probe.cpp
    src/tools/aot_probe/glide_texture_census_probe.cpp
    src/tools/aot_probe/out_of_arena_step_census_probe.cpp
    src/tools/aot_probe/timer_tick_delivery_probe.cpp
    src/tools/aot_probe/aot_boundary_opcode_census_probe.cpp
    src/tools/aot_probe/aot_worker_timing_probe.cpp
    src/tools/aot_probe/selector_guard_probe.cpp
    src/tools/aot_probe/main.cpp
)

add_executable(repiu_glide_issue_probe
    src/tools/glide_issue_probe/main.cpp
)

add_executable(repiu_glide_render_probe
    src/tools/glide_render_probe/main.cpp
)

add_executable(repiu
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

target_link_libraries(repiu_chd_cd_probe
    PRIVATE
        repiu_exe
)

target_link_libraries(repiu_aot_probe
    PRIVATE
        repiu_exe
)

target_link_libraries(repiu_glide_issue_probe
    PRIVATE
        repiu_exe
)

target_link_libraries(repiu_glide_render_probe
    PRIVATE
        repiu_exe
)
target_include_directories(repiu_aot_probe
    PRIVATE
        src/platform/win32
        src/platform/win32/execution
        third_party/zydis
)

target_link_libraries(repiu
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
            # Task 412: symbolises the guest position census's host call sites
            # at teardown. Without a PDB it resolves nothing and the report
            # falls back to module+offset, so it is safe in every build.
            dbghelp
    )
endif()

repiu_configure_win32_loader_host(repiu_exe_analyzer)
repiu_configure_win32_loader_host(repiu)
