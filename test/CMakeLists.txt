cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(BytraTests LANGUAGES CXX)

# ---- Options ----

option(ENABLE_TEST_COVERAGE "Enable test coverage" OFF)

# --- Import tools ----

include(../cmake/tools.cmake)

# ---- Dependencies ----

include(../cmake/CPM.cmake)

CPMAddPackage(
  NAME doctest
  GITHUB_REPOSITORY onqtam/doctest
  GIT_TAG 2.3.7
)

CPMAddPackage(NAME Bytra SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../bytra)

CPMAddPackage(
  NAME Format.cmake
  GITHUB_REPOSITORY TheLartians/Format.cmake
  VERSION 1.6
  OPTIONS # enable cmake formatting
          "FORMAT_CHECK_CMAKE ON"
          # skip CPM.cmake
          "CMAKE_FORMAT_EXCLUDE cmake/CPM.cmake"
)

# ---- Create binary ----

file(GLOB sources2 CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
list(REMOVE_ITEM sources "${CMAKE_CURRENT_SOURCE_DIR}/../bytra/source/main.cpp")
list(APPEND sources ${sources2})

add_executable(BytraTests ${sources})
target_include_directories(
  BytraTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include" "${Boost_INCLUDE_DIR}"
)
target_link_libraries(
  BytraTests
  doctest
  cxxopts
  spdlog
  simdjson
  cpr
  ta_lib
  ${Boost_LIBRARIES}
)

set_target_properties(BytraTests PROPERTIES CXX_STANDARD 17)

# ---- Add BytraTests ----

enable_testing()

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(BytraTests)
