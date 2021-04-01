cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# ---- Project ----

# Note: update this to your new project's name and version
project(
  Bytra
  VERSION 1.2.0
  LANGUAGES CXX
)

# --- Import tools ----

include(../cmake/tools.cmake)

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(../cmake/CPM.cmake)

# PackageProject.cmake will be used to make our target installable
CPMAddPackage(
  NAME PackageProject.cmake
  GITHUB_REPOSITORY TheLartians/PackageProject.cmake
  VERSION 1.3
)

CPMAddPackage(
  NAME cxxopts
  GITHUB_REPOSITORY jarro2783/cxxopts
  VERSION 2.2.0
  OPTIONS "CXXOPTS_BUILD_EXAMPLES Off" "CXXOPTS_BUILD_TESTS Off"
)

CPMAddPackage(
  NAME spdlog
  GITHUB_REPOSITORY gabime/spdlog
  VERSION 1.8.1
)

CPMAddPackage(
  NAME simdjson
  GITHUB_REPOSITORY simdjson/simdjson
  VERSION 0.6.0
)

find_package(ta_lib)
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)

# ---- Add source and header files ----

file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

# ---- Create executable ----

add_executable(Bytra ${sources} ${headers})

set_target_properties(Bytra PROPERTIES CXX_STANDARD 17)
target_include_directories(
  Bytra PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include" "${Boost_INCLUDE_DIR}" "${OPENSSL_INCLUDE_DIRS}"
)
target_link_libraries(
  Bytra
  cxxopts
  spdlog
  simdjson
  ta_lib
  ${Boost_LIBRARIES}
  ${OPENSSL_LIBRARIES}
)
