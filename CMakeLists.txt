cmake_minimum_required(VERSION 2.8.12)

project(FileExchanger)

find_package(LibWebRTC REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Sql REQUIRED)
find_package(Qt5Network REQUIRED)

include(${LIBWEBRTC_USE_FILE})

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)


add_executable(${PROJECT_NAME} "main.cpp" "sources/message.cpp" "sources/client.cpp" "sources/tcp_chat_client_lib.cpp" "sources/connectionfactory.cpp" "sources/connection.cpp" "sources/linker.cpp" "sources/interface.cpp")

target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Sql Qt5::Network ${LIBWEBRTC_LIBRARIES})
