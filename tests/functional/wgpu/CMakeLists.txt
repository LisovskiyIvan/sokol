# LLM maintained.
#
# Headless WGPU GPU-timing callback test. Standalone only: it needs
# <webgpu/webgpu.h>, so it is skipped unless WEBGPU_INCLUDE_DIR points at
# the directory containing it. Normal presets never set it. Only two
# WebGPU symbols are stubbed in the test source; the rest of the backend
# is dead-stripped at link.

if (DEFINED WEBGPU_INCLUDE_DIR AND EXISTS "${WEBGPU_INCLUDE_DIR}/webgpu/webgpu.h")
    add_executable(sokol-wgpu-timing-test sokol_gfx_wgpu_timing_test.c)
    target_compile_definitions(sokol-wgpu-timing-test PRIVATE SOKOL_WGPU)
    target_include_directories(sokol-wgpu-timing-test PRIVATE
        ${CMAKE_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/../util
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${WEBGPU_INCLUDE_DIR})
    target_compile_options(sokol-wgpu-timing-test PRIVATE
        -Wall -Wextra -Werror -Wsign-conversion -Wstrict-prototypes
        -ffunction-sections -fdata-sections)
    if (APPLE)
        target_link_options(sokol-wgpu-timing-test PRIVATE -Wl,-dead_strip)
    elseif (UNIX)
        target_link_options(sokol-wgpu-timing-test PRIVATE -Wl,--gc-sections)
    endif()
endif()
