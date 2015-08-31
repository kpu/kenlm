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
# In order to allow CMake files in the parent directory
#    to see this variable definition, we set PARENT_SCOPE.
#
# In order to set correct paths to these files
#    when this variable is referenced by CMake files in the parent directory,
#    we prefix all files with ${CMAKE_CURRENT_SOURCE_DIR}.
#
set(KENLM_UTIL_STREAM_SOURCE 
		${CMAKE_CURRENT_SOURCE_DIR}/chain.cc
		${CMAKE_CURRENT_SOURCE_DIR}/io.cc
		${CMAKE_CURRENT_SOURCE_DIR}/line_input.cc
		${CMAKE_CURRENT_SOURCE_DIR}/multi_progress.cc
		${CMAKE_CURRENT_SOURCE_DIR}/rewindable_stream.cc
	PARENT_SCOPE)



if(BUILD_TESTING)

    # Explicitly list the Boost test files to be compiled
    set(KENLM_BOOST_TESTS_LIST
      io_test
      sort_test
      stream_test
    )

    # Iterate through the Boost tests list   
    foreach(test ${KENLM_BOOST_TESTS_LIST})

      # Compile the executable, linking against the requisite dependent object files
      add_executable(${test} ${test}.cc $<TARGET_OBJECTS:kenlm_util>)

      # Require the following compile flag
      set_target_properties(${test} PROPERTIES COMPILE_FLAGS -DBOOST_TEST_DYN_LINK)
  
      # Link the executable against boost
      target_link_libraries(${test} ${Boost_LIBRARIES} pthread)
      
      # Specify command arguments for how to run each unit test
      #
      # Assuming that foo was defined via add_executable(foo ...),
      #   the syntax $<TARGET_FILE:foo> gives the full path to the executable.
      #
      add_test(NAME ${test}_test 
               COMMAND $<TARGET_FILE:${test}>)

      # Group unit tests together
      set_target_properties(${test} PROPERTIES FOLDER "unit_tests")
   
    # End for loop
    endforeach(test)

endif()
