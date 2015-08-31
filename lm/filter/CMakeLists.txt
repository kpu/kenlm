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

# Explicitly list the source files for this subdirectory
#
# If you add any source files to this subdirectory
#    that should be included in the kenlm library,
#        (this excludes any unit test files)
#    you should add them to the following list:
#
# In order to set correct paths to these files
#    in case this variable is referenced by CMake files in the parent directory,
#    we prefix all files with ${CMAKE_CURRENT_SOURCE_DIR}.
#
set(KENLM_FILTER_SOURCE 
		${CMAKE_CURRENT_SOURCE_DIR}/arpa_io.cc
		${CMAKE_CURRENT_SOURCE_DIR}/phrase.cc
		${CMAKE_CURRENT_SOURCE_DIR}/vocab.cc
	)


# Group these objects together for later use. 
#
# Given add_library(foo OBJECT ${my_foo_sources}),
# refer to these objects as $<TARGET_OBJECTS:foo>
#
add_library(kenlm_filter OBJECT ${KENLM_FILTER_SOURCE})


# Explicitly list the executable files to be compiled
set(EXE_LIST
  filter
  phrase_table_vocab
)


# Iterate through the executable list   
foreach(exe ${EXE_LIST})

  # Compile the executable, linking against the requisite dependent object files
	add_executable(${exe} ${exe}_main.cc $<TARGET_OBJECTS:kenlm> $<TARGET_OBJECTS:kenlm_filter> $<TARGET_OBJECTS:kenlm_util>)

  # Link the executable against boost
  target_link_libraries(${exe} ${Boost_LIBRARIES} pthread)

  # Group executables together
  set_target_properties(${exe} PROPERTIES FOLDER executables)

# End for loop
endforeach(exe)

