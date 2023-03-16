cmake_minimum_required(VERSION 3.12)

set(CMAKE_BUILD_TYPE Debug)

project(vswitch)

add_executable("${PROJECT_NAME}" vswitch.cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra")

find_package(PcapPlusPlus REQUIRED)

# We want to have the binary compiled in the same folder as the code
set_target_properties("${PROJECT_NAME}" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries("${PROJECT_NAME}" PUBLIC PcapPlusPlus::Pcap++)
