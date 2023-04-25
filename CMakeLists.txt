#project(xrtcserver)
#
#set(CMAKE_CXX_STANDARD 14)
#
#add_executable(xrtcserver
#    src/main.cpp)
cmake_minimum_required(VERSION 3.24)
project(xrtcserver)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
file(GLOB all_src "./src/*.cpp"
        "./src/base/*.cpp"
        "./src/server/*.cpp"
        "./src/stream/*.cpp"
        "./src/pc/*.cpp"
        "./src/ice/*.cpp")
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
add_executable(xrtcserver ${all_src} src/base/log.h
        src/base/log.cpp
        src/server/signaling_server.cpp
        src/server/signaling_server.h
        src/base/socket.h
        src/base/socket.cpp
        src/base/event_loop.cpp
        src/base/event_loop.h
        src/server/signaling_worker.cpp
        src/server/signaling_worker.h src/base/lock_free_queue.cpp src/base/lock_free_queue.h src/base/lock_free_queue.cpp src/server/tcp_connection.cpp src/server/tcp_connection.h src/base/xhead.cpp src/base/xhead.h src/server/rtc_server.cpp src/server/rtc_server.h src/server/rtc_worker.cpp src/server/rtc_worker.h src/xrtcserver_def.h src/stream/rtc_stream_manager.cpp src/stream/rtc_stream_manager.h src/stream/rtc_stream.cpp src/stream/rtc_stream.h src/stream/push_stream.cpp src/stream/push_stream.h src/pc/peer_connection.cpp src/pc/peer_connection.h src/pc/session_description.cpp src/pc/session_description.h src/pc/codec_info.cpp src/pc/codec_info.h src/ice/ice_credentials.cpp src/ice/ice_credentials.h src/ice/ice_agent.cpp src/ice/ice_agent.h src/ice/ice_transport_channel.cpp src/ice/ice_transport_channel.h src/pc/transport_controller.cpp src/pc/transport_controller.h src/base/network.cpp src/base/network.h src/ice/port_allocator.cpp src/ice/port_allocator.h src/ice/udp_port.cpp src/ice/udp_port.h src/ice/candidate.cpp src/ice/candidate.h src/base/async_udp_socket.cpp src/base/async_udp_socket.h src/ice/ice_connection.cpp src/ice/ice_controller.h src/ice/stun_request.h src/pc/dtls_transport.h)

target_link_libraries(xrtcserver libyaml-cpp.a librtcbase.a
        libabsl_strings.a libabsl_throw_delegate.a libev.a libjsoncpp.a libssl.a libcrypto.a -lpthread -ldl
        libabsl_bad_optional_access.a
        )





