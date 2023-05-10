#project(xrtcserver)
#
#set(CMAKE_CXX_STANDARD 14)
#
#add_executable(xrtcserver
#    src/main.cpp)
cmake_minimum_required(VERSION 3.16)
project(xrtcserver)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
file(GLOB all_src
        "./src/*.cpp"
        "./src/base/*.cpp"
        "./src/server/*.cpp"
        "./src/stream/*.cpp"
        "./src/pc/*.cpp"
        "./src/ice/*.cpp"
        "./src/module/rtp_rtcp/*.cpp"
        )
include_directories("./src"
        "./third_party/include"
        "../rtcbase/src")

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/out")

link_directories(
        "./third_party/lib"
        "../rtcbase/out"
)
#set(CMAKE_CXX_STANDARD 14)
add_definitions("-g -pipe -W -Wall -fPIC -std=gnu++14 -DWEBRTC_POSIX
-DWEBRTC_LINUX")
SET(CMAKE_EXE_LINKER_FLAGS " -no-pie")
add_executable(xrtcserver ${all_src} src/module/rtp_rtcp/rtp_utils.cpp)

target_link_libraries(xrtcserver libyaml-cpp.a librtcbase.a
    libabsl_strings.a libabsl_throw_delegate.a libabsl_bad_optional_access.a
    libev.a libjsoncpp.a libssl.a libcrypto.a libsrtp2.a
    -lpthread -ldl
        )





