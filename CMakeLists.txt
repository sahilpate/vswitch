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

set(LEXER_OUT "lex.yy.cc")
flex_target(LEXER "cli_lexer.ll" "${LEXER_OUT}" COMPILE_FLAGS "-+")

add_executable("${PROJECT_NAME}" vswitch.cpp "${LEXER_OUT}")

# We want to have the binary compiled in the same folder as the code
set_target_properties("${PROJECT_NAME}" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries("${PROJECT_NAME}" PUBLIC PcapPlusPlus::Pcap++)
