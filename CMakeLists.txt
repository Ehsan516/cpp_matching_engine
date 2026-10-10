cmake_minimum_required(VERSION 3.16)
project(matcher CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    # add_compile_options(-Werror)   too annoying while hacking. CI passes it instead
endif()

option(SANITIZE "asan + ubsan" OFF)
if(SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
    # ^ no-recover so ubsan actually fails the test instead of just printing
endif()

set(ENG_SRC src/book.cpp src/engine.cpp src/parser.cpp src/format.cpp)

add_library(engine ${ENG_SRC})
target_compile_definitions(engine PUBLIC ENGINE_INDEX=1)
# ^ PUBLIC so everything linking it sees the same OrderBook layout

# old linear scan cancel. kept so the two can be benchmarked side by side
add_library(engine_scan ${ENG_SRC})

add_executable(matcher app/main.cpp)
target_link_libraries(matcher engine)
# set_target_properties(matcher PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(bench bench/bench.cpp)
target_link_libraries(bench engine)
add_executable(bench_scan bench/bench.cpp)
target_link_libraries(bench_scan engine_scan)

# per command latency (p50/p99). separate exe so its numbers never get mixed up with batch throughput
add_executable(bench_lat bench/lat.cpp)
target_link_libraries(bench_lat engine)
add_executable(bench_lat_scan bench/lat.cpp)
target_link_libraries(bench_lat_scan engine_scan)

# bake build info into bench so every result says what it came from
# NOTE sha is grabbed at configure time, rerun cmake if you want it fresh
string(TOUPPER "${CMAKE_BUILD_TYPE}" _bt)
execute_process(COMMAND git describe --always --dirty
    # ^ -dirty = uncommitted changes, so that result cant be reproduced from the sha alone
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
foreach(b bench bench_scan bench_lat bench_lat_scan)
    target_compile_definitions(${b} PRIVATE
        BENCH_CXX="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
        BENCH_FLAGS="${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${_bt}}"
        BENCH_BT="${CMAKE_BUILD_TYPE}"
        BENCH_SHA="${GIT_SHA}")
    if(SANITIZE)
        target_compile_definitions(${b} PRIVATE BENCH_SAN=1)
    endif()
endforeach()

enable_testing()
add_executable(tests  tests/test_main.cpp)
target_link_libraries(tests engine)
add_test(NAME unit COMMAND tests)

# same tests vs the scan build, both versions have to behave identically
add_executable(tests_scan tests/test_main.cpp)
target_link_libraries(tests_scan engine_scan)
add_test(NAME unit_scan COMMAND tests_scan)

# cli fixtures. name / exit code
foreach(fx  example:0  ex_b:0  ex_c:1  errors:1)
    string(REPLACE ":" ";" parts ${fx})
    list(GET parts 0 nm)
    list(GET parts 1 code)
    add_test(NAME fx_${nm}
        COMMAND ${CMAKE_COMMAND} -DBIN=$<TARGET_FILE:matcher>
            -DIN=${CMAKE_SOURCE_DIR}/fixtures/${nm}.txt
            -DEXP=${CMAKE_SOURCE_DIR}/fixtures/${nm}.expected
            -DCODE=${code}
            -P ${CMAKE_SOURCE_DIR}/tests/run_fixture.cmake)
endforeach()
