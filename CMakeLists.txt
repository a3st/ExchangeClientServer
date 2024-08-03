cmake_minimum_required(VERSION 3.25.1)

project(Exchange VERSION 1.0.0 LANGUAGES CXX)

find_package(botan CONFIG REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(argh CONFIG REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

#
#   Server
#
add_executable(server 
    server/modules/exchange.cpp
    server/modules/wallet.cpp
    server/modules/login.cpp
    server/core.cpp
    server/session.cpp
    server/server.cpp
    server/main.cpp)

target_include_directories(server PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/server)

target_link_libraries(server PRIVATE
    Boost::system
    Botan::Botan-static
    SQLiteCpp
    spdlog::spdlog
    argh)

target_precompile_headers(server PRIVATE ${PROJECT_SOURCE_DIR}/precompiled.hpp)

#
#   Client
#
add_executable(client
    client/packets/exchange.cpp
    client/packets/login.cpp
    client/packets/wallet.cpp
    client/packet.cpp
    client/client.cpp
    client/main.cpp)

target_include_directories(client PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/client)

target_link_libraries(client PRIVATE
    Boost::system
    Botan::Botan-static
    argh)

target_precompile_headers(client PRIVATE ${PROJECT_SOURCE_DIR}/precompiled.hpp)

#
#   Tests
#
add_executable(exchange_test
    server/modules/exchange.cpp
    server/modules/wallet.cpp
    tests/exchange_test.cpp)

target_include_directories(exchange_test PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/server)

target_link_libraries(exchange_test PRIVATE
    Boost::system
    SQLiteCpp
    spdlog::spdlog
    GTest::gtest)