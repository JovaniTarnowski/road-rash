cmake_minimum_required(VERSION 3.22)
option(RR_BUNDLE_SDL "Build a standalone macOS ARM64 app with pinned static SDL" OFF)
if(RR_BUNDLE_SDL AND NOT ANDROID)
    set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "Standalone application architecture" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0 CACHE STRING "Minimum macOS version" FORCE)
endif()
project(RoadRashNative LANGUAGES C CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_target(rr-build-provenance
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/build_info.py"
        --header "${CMAKE_BINARY_DIR}/generated/rr_build_info.hpp"
        --manifest "${CMAKE_BINARY_DIR}/build-info.json"
        --config "$<CONFIG>" --compiler "${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    BYPRODUCTS "${CMAKE_BINARY_DIR}/generated/rr_build_info.hpp"
    VERBATIM)
option(RR_DEV_ASSETS "Allow developer-only packaged Android game assets" OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(EMSCRIPTEN)
    include(cmake/emscripten.cmake)
elseif(RR_BUNDLE_SDL)
    if(NOT APPLE OR ANDROID)
        message(FATAL_ERROR "RR_BUNDLE_SDL is the standalone macOS build option")
    endif()
    include(cmake/sdl3_static.cmake)
else()
    find_package(SDL3 3.2 REQUIRED CONFIG)
endif()
add_library(rr-resource STATIC src/resource.cpp src/course.cpp src/road.cpp src/assets.cpp src/framebuffer.cpp src/bike.cpp src/race.cpp src/input.cpp src/bob.cpp src/session.cpp src/iso9660.cpp src/sha256.cpp src/importer.cpp src/wav.cpp src/mixer.cpp src/ima_adpcm.cpp src/avi.cpp src/cinepak.cpp src/video_player.cpp src/raster.cpp src/terrain.cpp src/scene.cpp src/polygon.cpp src/texture.cpp src/cel.cpp src/world_renderer.cpp src/road_panels.cpp src/road_materials.cpp src/exe_tables.cpp src/pe_strings.cpp src/career.cpp src/text.cpp src/settings.cpp src/rri.cpp src/frontend.cpp src/sound.cpp src/explode.cpp src/rider_sprites.cpp)
target_link_libraries(rr-resource PUBLIC SDL3::SDL3)
target_sources(rr-resource PRIVATE src/family.cpp src/terrain_panels.cpp src/building_panels.cpp src/terrain_ground.cpp src/course_scene.cpp src/hud.cpp src/animation.cpp src/actor_animation.cpp src/actor_descriptors.cpp src/actor_render.cpp src/actor_nodes.cpp src/crash.cpp src/police.cpp src/scenery.cpp src/traffic.cpp)
set_target_properties(rr-resource PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(rr-resource PUBLIC src)
target_include_directories(rr-resource PRIVATE third_party)
if(ANDROID)
    # The Android build ships the playable game.
    add_library(main SHARED src/game_main.cpp src/road_draw.cpp src/import_ui.cpp)
    set(RR_VIEWER main)
else()
    # Web (plan 23 Task 1): rr-resource and the tests only. The game
    # executables keep blocking main loops and return with Task 3.
    if(NOT EMSCRIPTEN)
    add_executable(rr-import src/import_main.cpp)
    target_link_libraries(rr-import PRIVATE rr-resource)
    if(RR_BUNDLE_SDL)
        set(RR_GAME_BUNDLE MACOSX_BUNDLE)
    endif()
    add_executable(rr-game ${RR_GAME_BUNDLE} src/game_main.cpp src/road_draw.cpp src/import_ui.cpp)
    target_link_libraries(rr-game PRIVATE rr-resource SDL3::SDL3)
    if(RR_BUNDLE_SDL)
        set_target_properties(rr-game PROPERTIES
            OUTPUT_NAME "Road Rash Native"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.plist.in")
        add_custom_command(TARGET rr-game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:rr-game>/Contents/Resources"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/third_party/SDL-LICENSE.txt"
                "${CMAKE_SOURCE_DIR}/third_party/LICENSE"
                "${CMAKE_SOURCE_DIR}/macos/README.txt"
                "$<TARGET_BUNDLE_DIR:rr-game>/Contents/Resources"
            COMMAND /usr/bin/codesign --force --sign - "$<TARGET_BUNDLE_DIR:rr-game>"
            VERBATIM)
    endif()
    add_executable(rr-viewer src/viewer.cpp src/road_draw.cpp)
    set(RR_VIEWER rr-viewer)
    add_executable(rr-inspect src/inspect.cpp)
    target_link_libraries(rr-inspect PRIVATE rr-resource)
    add_executable(rr-course src/course_inspect.cpp)
    target_link_libraries(rr-course PRIVATE rr-resource)
    endif()
    include(CTest)
    add_compile_definitions(RR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    if(BUILD_TESTING)
        if(NOT EMSCRIPTEN)
        add_test(NAME first-run-smoke COMMAND rr-game --first-run-smoke)
        set_tests_properties(first-run-smoke PROPERTIES ENVIRONMENT "RR_PREF_OVERRIDE=${CMAKE_BINARY_DIR}/first-run-test-pref")
        endif()
        add_executable(oracle-patch-test oracle/patch_test.c)
        add_test(NAME oracle-patch COMMAND oracle-patch-test)
        set_tests_properties(oracle-patch PROPERTIES SKIP_RETURN_CODE 77)
        add_executable(oracle-sim-test tests/oracle_sim_test.cpp)
        target_link_libraries(oracle-sim-test PRIVATE rr-resource SDL3::SDL3)
        # Diagnostic gate accepts only a pinned trace hash + exact divergence.
        # Strict fidelity is a separate verify-sim target; never invert all failures.
        add_test(NAME oracle-sim COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/verify_sim.py" --mode diagnostic
            --binary "$<TARGET_FILE:oracle-sim-test>"
            --report "${CMAKE_BINARY_DIR}/oracle-diagnostic.json")
        set_tests_properties(oracle-sim PROPERTIES SKIP_RETURN_CODE 77)
        add_test(NAME agent-tooling COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tests/agent_tooling_test.py")
        add_custom_target(verify-sim
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/verify_sim.py"
                --mode acceptance --binary "$<TARGET_FILE:oracle-sim-test>"
                --report "${CMAKE_BINARY_DIR}/oracle-acceptance.json"
            DEPENDS oracle-sim-test VERBATIM)
        add_executable(oracle-road-test tests/oracle_road_test.cpp)
        target_link_libraries(oracle-road-test PRIVATE rr-resource)
        add_test(NAME oracle-road COMMAND oracle-road-test)
        set_tests_properties(oracle-road PROPERTIES SKIP_RETURN_CODE 77)
        add_executable(oracle-scene-test tests/oracle_scene_test.cpp)
        target_link_libraries(oracle-scene-test PRIVATE rr-resource)
        add_test(NAME oracle-scene COMMAND oracle-scene-test)
        set_tests_properties(oracle-scene PROPERTIES SKIP_RETURN_CODE 77)
        add_executable(oracle-raster-test tests/oracle_raster_test.cpp)
        target_link_libraries(oracle-raster-test PRIVATE rr-resource)
        add_test(NAME oracle-raster COMMAND oracle-raster-test)
        set_tests_properties(oracle-raster PROPERTIES SKIP_RETURN_CODE 77)
        add_executable(resource-test tests/resource_test.cpp)
        target_link_libraries(resource-test PRIVATE rr-resource)
        add_test(NAME resource-bounds COMMAND resource-test)
        add_executable(road-test tests/road_test.cpp)
        target_link_libraries(road-test PRIVATE rr-resource)
        add_test(NAME road-projection COMMAND road-test)
        foreach(t framebuffer assets bike race input bob session golden sha256 iso9660 importer wav mixer ima_adpcm avi cinepak raster view terrain scene polygon texture cel family course course_scene hud animation actor_animation actor_descriptors actor_render actor_nodes crash police scenery building_panels terrain_ground world_renderer road_panels road_materials exe_tables pe_strings career text frontend settings sound traffic actor_traffic sim_fields)
            add_executable(${t}-test tests/${t}_test.cpp)
            target_link_libraries(${t}-test PRIVATE rr-resource SDL3::SDL3)
            add_test(NAME ${t} COMMAND ${t}-test)
        endforeach()
        set_tests_properties(polygon PROPERTIES SKIP_RETURN_CODE 77)
        add_test(NAME pe-strings-corpus COMMAND pe_strings-test --corpus)
        set_tests_properties(pe-strings-corpus PROPERTIES SKIP_RETURN_CODE 77)
        add_test(NAME career-corpus COMMAND career-test --corpus)
        set_tests_properties(career-corpus PROPERTIES SKIP_RETURN_CODE 77)
        set_tests_properties(text PROPERTIES SKIP_RETURN_CODE 77)
        set_tests_properties(frontend PROPERTIES SKIP_RETURN_CODE 77)
        set_tests_properties(sound PROPERTIES SKIP_RETURN_CODE 77)
        add_test(NAME wav-corpus COMMAND wav-test --corpus)
        set_tests_properties(wav-corpus PROPERTIES SKIP_RETURN_CODE 77)
        add_test(NAME mixer-device COMMAND mixer-test --device)
        set_tests_properties(mixer-device PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
    endif()
endif()
if(RR_VIEWER)
    add_dependencies(${RR_VIEWER} rr-build-provenance)
    target_include_directories(${RR_VIEWER} PRIVATE third_party)
    target_link_libraries(${RR_VIEWER} PRIVATE rr-resource SDL3::SDL3)
endif()
if(TARGET rr-game)
    add_dependencies(rr-game rr-build-provenance)
    target_include_directories(rr-game PRIVATE "${CMAKE_BINARY_DIR}/generated")
elseif(TARGET main)
    target_include_directories(main PRIVATE "${CMAKE_BINARY_DIR}/generated")
endif()
if(ANDROID AND RR_DEV_ASSETS)
    target_compile_definitions(main PRIVATE RR_DEV_ASSETS=1)
endif()
# Web-planning microbenchmark (plan 23, Task 1, Step 4). Opt-in so the
# default build is unchanged; the web toolchain file enables it.
option(RR_BENCH_WEB "Build the rr-bench-web render/codec microbenchmark" OFF)
if(RR_BENCH_WEB AND NOT ANDROID)
    add_executable(rr-bench-web tests/bench_web.cpp)
    target_link_libraries(rr-bench-web PRIVATE rr-resource)
endif()
