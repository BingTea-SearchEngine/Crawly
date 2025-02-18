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


FetchContent_Declare(
    Frontier
    GIT_REPOSITORY https://github.com/BingTea-SearchEngine/Frontier.git
    GIT_TAG main
)
FetchContent_MakeAvailable(Frontier)

FetchContent_Declare(
    Hive
    GIT_REPOSITORY https://github.com/BingTea-SearchEngine/Hive.git
    GIT_TAG main
)
FetchContent_MakeAvailable(Hive)

set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")

find_package(OpenSSL REQUIRED)

add_library(GetSSL STATIC ${LIB_DIR}/GetSSL/GetSSL.cpp)
target_include_directories(GetSSL PRIVATE ${LIB_DIR}/GetSSL ${OPENSSL_INCLUDE_DIR})
target_link_libraries(GetSSL INTERFACE OpenSSL::SSL OpenSSL::Crypto)

set(FRONTIER_SOURCE_DIR ${frontier_SOURCE_DIR})
set(FRONTIER_INTERFACE_INCLUDE_DIR "${frontier_SOURCE_DIR}/lib/FrontierInterface")
message(STATUS "Frontier project source directory: ${FRONTIER_SOURCE_DIR}")
message(STATUS "FrontierInterface include directory: ${FRONTIER_INTERFACE_INCLUDE_DIR}")

set(HIVE_SOURCE_DIR ${hive_SOURCE_DIR})
set(HIVE_INCLUDE_DIR "${hive_SOURCE_DIR}/lib")
message(STATUS "Hive project source directory: ${HIVE_SOURCE_DIR}")
message(STATUS "Hive include directory: ${HIVE_INCLUDE_DIR}")

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_definitions(-DPROJECT_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}/\")
add_executable(${THIS} ${SRC_DIR}/Crawly.cpp)
target_link_libraries(${THIS} PUBLIC spdlog::spdlog FrontierInterface ThreadPoolLib pthread GetSSL)
target_include_directories(${THIS} PRIVATE ${FRONTIER_INTERFACE_INCLUDE_DIR} ${HIVE_INCLUDE_DIR}
    ${LIB_DIR}/GetSSL)

# Specify build type
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
