cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(comm_network CXX C)

option(WITH_TEST "Build with test" ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set (THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/third_party)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(comm_network_cmake_dir ${PROJECT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${comm_network_cmake_dir}/third_party)
list(APPEND CMAKE_MODULE_PATH ${comm_network_cmake_dir})

include (comm_network)

if (USE_TEST)
  enable_testing()
  add_subdirectory(test)
endif()
