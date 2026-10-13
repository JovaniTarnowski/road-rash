# WebAssembly build (plan 23, Task 1): Emscripten + SDL 3.4.8.
#
# Included from the top-level CMakeLists when EMSCRIPTEN is set (i.e. the
# project is configured through emcmake). SDL comes from the same pinned
# release archive (URL and SHA-256) as the macOS bundle build; see
# cmake/sdl3_static.cmake.
#
# Task 1 scope: rr-resource and the synthetic tests only. The game
# executables keep blocking main loops and are enabled once the SDL
# main-callback rework (Task 3) lands; CMakeLists skips them for EMSCRIPTEN.

# SDL3 for Emscripten: same pinned archive as the standalone macOS build.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
include(cmake/sdl3_static.cmake)

# Release optimisation used for the Task 1 speed measurements.
# -sDISABLE_EXCEPTION_CATCHING=0 must be a COMPILE option as well as a link
# option: em++ only emits landing pads (references to __cxa_begin_catch /
# __cxa_find_matching_catch) for translation units compiled with it. With the
# flag at link time only, every catch block is dead code and any thrown
# std::runtime_error escapes to JS as an unhandled rejection.
add_compile_options(-O3 -msimd128 -sDISABLE_EXCEPTION_CATCHING=0)
# The sim's value-type call chains (Race construction + stepping) need more
# than Emscripten's 64 KiB default stack: race-test overruns the stack cookie
# under Node but passes natively and with 1 MiB (plan 23 Task 2, Oct 2026).
# The future game target steps the same paths every frame, so this is set
# for all web links, not just the tests.
add_link_options(-O3 -flto -msimd128 -sSTACK_SIZE=1MB -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=node -sDISABLE_EXCEPTION_CATCHING=0)

# CTest runs the cross-built tests under the Node.js bundled with the pinned
# emsdk (.tools/emsdk). The glob keeps working across emsdk updates; the
# exact version is recorded in docs/TOOLING.md.
file(GLOB RR_WEB_NODE_CANDIDATES "${CMAKE_SOURCE_DIR}/.tools/emsdk/node/*/bin/node")
list(SORT RR_WEB_NODE_CANDIDATES)
list(GET RR_WEB_NODE_CANDIDATES -1 RR_WEB_NODE)
set(CMAKE_CROSSCOMPILING_EMULATOR "${RR_WEB_NODE}" CACHE FILEPATH "Run wasm tests under emsdk Node")
message(STATUS "Web tests run under ${CMAKE_CROSSCOMPILING_EMULATOR}")

# Plan 23 Task 2 Step 1: tests read the real disc (../cdrom/ROADRASH) and
# the analysis/oracle/ goldens through Node's filesystem. Native tests use
# host paths (RR_SOURCE_DIR, std::filesystem), which are invisible inside
# the default wasm sandbox. NODERAWFS is applied per test executable, never
# globally: the future browser game target must use the sandboxed/IDBFS
# filesystem, and NODERAWFS only works under Node.
function(rr_web_hostfs target)
    target_link_options(${target} PRIVATE -sNODERAWFS=1)
endfunction()

# Web-planning microbenchmark for the Task 1 speed measurements.
set(RR_BENCH_WEB ON CACHE BOOL "Build the rr-bench-web microbenchmark" FORCE)
