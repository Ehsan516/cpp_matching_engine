cmake_minimum_required(VERSION 3.16)
project(matcher CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    # add_compile_options(-Werror)   too annoying while hacking, maybe for CI
endif()

option(SANITIZE "asan + ubsan" OFF)
if(SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
    # ^ no-recover so ubsan actually fails the test instead of just printing
endif()

add_library(engine src/book.cpp src/engine.cpp src/parser.cpp src/format.cpp)

add_executable(matcher app/main.cpp)
target_link_libraries(matcher engine)
# set_target_properties(matcher PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

enable_testing()
add_executable(tests  tests/test_main.cpp)
target_link_libraries(tests engine)
add_test(NAME unit COMMAND tests)

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
