cmake_minimum_required(VERSION 2.8.8)
#
# The KenLM cmake files make use of add_library(... OBJECTS ...)
# 
# This syntax allows grouping of source files when compiling
# (effectively creating "fake" libraries based on source subdirs).
# 
# This syntax was only added in cmake version 2.8.8
#
# see http://www.cmake.org/Wiki/CMake/Tutorials/Object_Library


# This CMake file was created by Lane Schwartz <dowobeha@gmail.com>


# Define a single cmake project
project(kenlm)

# Compile all executables into bin/
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# Compile all libraries into lib/
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

# Tell cmake that we want unit tests to be compiled
include(CTest)
enable_testing()

# We need boost
find_package(Boost 1.36.0 REQUIRED COMPONENTS
  program_options 
  system
  thread
  unit_test_framework
)




# Define where include files live
include_directories(
  ${PROJECT_SOURCE_DIR} 
  ${Boost_INCLUDE_DIRS}
)


# Process subdirectories
add_subdirectory(util)
add_subdirectory(lm)

