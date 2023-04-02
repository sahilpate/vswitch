cmake_minimum_required(VERSION 3.12)

project(vswitch)

find_package(FLEX 2.6.4 REQUIRED)
find_package(PcapPlusPlus REQUIRED)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE MAIN_SRC "*.cpp" "*.hpp")

set_source_files_properties(
  ${MAIN_SRC}
  PROPERTIES
  COMPILE_FLAGS "-Wall -Wextra"
)

set(LEXER_OUT "build/lex.yy.cc")
flex_target(LEXER "${CMAKE_CURRENT_SOURCE_DIR}/src/cli_lexer.ll" "${LEXER_OUT}" COMPILE_FLAGS "-+")

add_executable("${PROJECT_NAME}"
  main.cpp
  src/cli.cpp
  src/duplicate_manager.cpp
  src/mac_addr_table.cpp
  src/packet_queue.cpp
  src/vswitch_utils.cpp
  "${LEXER_OUT}")

target_include_directories("${PROJECT_NAME}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries("${PROJECT_NAME}" PUBLIC PcapPlusPlus::Pcap++)
set_target_properties("${PROJECT_NAME}" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable("test_orchestrator"
  tests/test_orchestrator.cpp
  src/duplicate_manager.cpp
  src/testing_utils.cpp)

target_include_directories("test_orchestrator" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries("test_orchestrator" PUBLIC PcapPlusPlus::Pcap++)
set_target_properties("test_orchestrator" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable("vswitch_testing"
  tests/tests.cpp
  src/duplicate_manager.cpp
  src/testing_utils.cpp
  src/vswitch_utils.cpp)

target_include_directories("vswitch_testing" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries("vswitch_testing" PUBLIC PcapPlusPlus::Pcap++)
set_target_properties("vswitch_testing" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
