# Minimum CMake version required
cmake_minimum_required(VERSION 3.26)

# Project name and version
project(coherence)

# Specify the C++ standard (adjust if needed)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB_RECURSE HEADER_FILES "*.h")

set(SOURCE_FILES
  ${CMAKE_SOURCE_DIR}/architecture.cpp
  ${CMAKE_SOURCE_DIR}/cache.cpp
  ${CMAKE_SOURCE_DIR}/processor.cpp
)

# Add an executable (for a simple project with main.cpp)
add_executable(coherence
  ${HEADER_FILES}
  ${SOURCE_FILES}
  main.cpp
)

# Optionally add include directories
# include_directories(include)

# Optionally link libraries (e.g., linking to libraries like pthread)
# target_link_libraries(MyProject pthread)

# Optionally, add additional source files
# add_executable(MyProject main.cpp src/file1.cpp src/file2.cpp)