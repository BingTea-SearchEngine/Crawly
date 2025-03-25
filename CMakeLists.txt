cmake_minimum_required(VERSION 3.31)

set(THIS Crawly)

project(${THIS} CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -std=c++17")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

include(FetchContent)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.12.0
)
FetchContent_MakeAvailable(spdlog)


set(FRONTIER_LOCAL_DIR "" CACHE PATH "Use local version of Frontier")
if(FRONTIER_LOCAL_DIR)
    message(STATUS "Using local Frontier from ${FRONTIER_LOCAL_DIR}")
    FetchContent_Declare(
        Frontier
        SOURCE_DIR ${FRONTIER_LOCAL_DIR}
    )
else()
    message(STATUS "Cloning Frontier from GitHub")
    FetchContent_Declare(
        Frontier
        GIT_REPOSITORY https://github.com/BingTea-SearchEngine/Frontier.git
        GIT_TAG wbjin-dev
    )
endif()
FetchContent_MakeAvailable(Frontier)

FetchContent_Declare(
    Hive
    GIT_REPOSITORY https://github.com/BingTea-SearchEngine/Hive.git
    GIT_TAG main
)
FetchContent_MakeAvailable(Hive)

FetchContent_Declare(
    HtmlParser
    GIT_REPOSITORY https://github.com/BingTea-SearchEngine/HtmlParser.git
    GIT_TAG main
)
FetchContent_MakeAvailable(HtmlParser)

FetchContent_Declare(
    Gateway
    GIT_REPOSITORY https://github.com/BingTea-SearchEngine/Gateway.git
    GIT_TAG main
)
FetchContent_MakeAvailable(Gateway)

FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)
FetchContent_MakeAvailable(argparse)

set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")

find_package(OpenSSL REQUIRED)

add_library(GetSSL STATIC ${LIB_DIR}/GetSSL/GetSSL.cpp)
target_include_directories(GetSSL PRIVATE ${LIB_DIR}/GetSSL ${OPENSSL_INCLUDE_DIR})
target_link_libraries(GetSSL INTERFACE OpenSSL::SSL OpenSSL::Crypto)

add_library(GetURL STATIC ${LIB_DIR}/GetURL/GetURL.cpp)
target_include_directories(GetURL PRIVATE ${LIB_DIR}/GetURL ${LIB_DIR}/GetSSL)
target_link_libraries(GetURL PRIVATE GetSSL)

set(FRONTIER_SOURCE_DIR ${frontier_SOURCE_DIR})
set(FRONTIER_INTERFACE_INCLUDE_DIR "${frontier_SOURCE_DIR}/lib/FrontierInterface")
message(STATUS "Frontier project source directory: ${FRONTIER_SOURCE_DIR}")
message(STATUS "FrontierInterface include directory: ${FRONTIER_INTERFACE_INCLUDE_DIR}")

set(HIVE_SOURCE_DIR ${hive_SOURCE_DIR})
set(HIVE_INCLUDE_DIR "${hive_SOURCE_DIR}/lib")
message(STATUS "Hive project source directory: ${HIVE_SOURCE_DIR}")
message(STATUS "Hive include directory: ${HIVE_INCLUDE_DIR}")

set(PARSER_SOURCE_DIR ${htmlparser_SOURCE_DIR})
set(PARSER_INCLUDE_DIR "${htmlparser_SOURCE_DIR}/lib")
message(STATUS "HTML Parser project source directory: ${PARSER_SOURCE_DIR}")
message(STATUS "HTML PARSER include directory: ${PARSER_INCLUDE_DIR}")

set(GATEWAY_SOURCE_DIR ${gateway_SOURCE_DIR})
set(GATEWAY_INCLUDE_DIR "${gateway_SOURCE_DIR}/lib")
message(STATUS "Gateway project source directory: ${GATEWAY_SOURCE_DIR}")
message(STATUS "Gateway include directory: ${GATEWAY_INCLUDE_DIR}")

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_definitions(-DPROJECT_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}/\")
add_executable(${THIS} ${SRC_DIR}/Crawly.cpp)
target_link_libraries(${THIS} PUBLIC spdlog::spdlog FrontierInterface Hive pthread GetSSL
    HtmlParser GetURL GatewayClient argparse)
target_include_directories(${THIS} PRIVATE ${FRONTIER_INTERFACE_INCLUDE_DIR} ${HIVE_INCLUDE_DIR}
    ${LIB_DIR}/GetSSL ${LIB_DIR}/GetURL ${PARSER_INCLUDE_DIR} ${GATEWAY_INCLUDE_DIR})

# Specify build type
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
