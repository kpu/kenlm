cmake_minimum_required(VERSION 2.6)
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

# Add our CMake helper functions
include(cmake/KenLMFunctions.cmake)

# And our helper modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)

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

