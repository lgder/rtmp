cmake_minimum_required(VERSION 3.4.1)

# 引入指定目录下的CMakeLists.txt
add_subdirectory(${CMAKE_SOURCE_DIR}/librtmp)

include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/core)

set(jnilibs ${CMAKE_SOURCE_DIR}/../jniLibs)
link_directories(
        ${jnilibs}/arm64-v8a)


file(GLOB core_srcs ${CMAKE_SOURCE_DIR}/core/*.cpp ${CMAKE_SOURCE_DIR}/core/*.h)

# 编译成以 native-lib 库
add_library(
        native-lib
        SHARED
        va_lib.cpp
        ${core_srcs})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${jnilibs}/arm64-v8a")

target_link_libraries(
        native-lib
        rtmp
        x264
        faac
        log)