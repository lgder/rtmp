cmake_minimum_required(VERSION 3.10)
project(rtmp_server)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)

# for vscode clangd extension
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -g -fsanitize=leak
set(CMAKE_CXX_FLAGS "-O3 -Wall -D_DEBUG")

include_directories(
        #        /usr/include
        ${PROJECT_SOURCE_DIR}/src/net
        ${PROJECT_SOURCE_DIR}/src/rtmp
        ${PROJECT_SOURCE_DIR}/src/utils
)

file(GLOB SOURCES src/rtmp/*.cc src/utils/*.cc src/net/*.cc)

add_executable(${PROJECT_NAME}
        src/main.cc
        ${SOURCES})

target_link_libraries(${PROJECT_NAME} # 这里是要链接的可执行文件名
        PRIVATE
        pthread
        rt
        dl
        m
        )


### gtest的内容 start

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
# set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

enable_testing()

SET(TEST_ALL test_all)
add_executable(
        ${TEST_ALL}
        gtest/test_all.cc
        ${SOURCES}
)
target_link_libraries(
        ${TEST_ALL}
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(${TEST_ALL})

### gtest end