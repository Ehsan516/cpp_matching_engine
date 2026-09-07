cmake_minimum_required(VERSION 3.16)
project(matcher CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_library(engine src/book.cpp src/engine.cpp)

add_executable(matcher app/main.cpp)
target_link_libraries(matcher engine)
# set_target_properties(matcher PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

enable_testing()
add_executable(tests  tests/test_main.cpp)
target_link_libraries(tests engine)
add_test(NAME unit COMMAND tests)
