cmake_minimum_required(VERSION 3.19)

include(FetchContent)

# fetch zmq + official zmq cpp bindings
# Make sure zmq is imported without tests
set(ZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    zmq
    GIT_REPOSITORY https://github.com/zeromq/libzmq.git
    GIT_TAG v4.3.5

)
FetchContent_Declare(
    cppzmq
    GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
    GIT_TAG v4.11.0
)

# Fetch GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zmq cppzmq googletest)

# toolchain
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# project
project(zmq_example)

set(APP_SRCS
    src/ExampleService.cpp
    main.cpp
)

add_executable(zmq_example ${APP_SRCS})
target_link_libraries(zmq_example PRIVATE libzmq-static cppzmq)
target_include_directories(zmq_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Enable testing
enable_testing()

# Add test executable
add_executable(client_tests
    tests/client_test.cpp
    src/ExampleService.cpp
)
target_link_libraries(client_tests PRIVATE libzmq-static cppzmq GTest::gtest_main)
target_include_directories(client_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Discover tests
include(GoogleTest)
gtest_discover_tests(client_tests)
