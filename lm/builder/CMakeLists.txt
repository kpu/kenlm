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
set(KENLM_BUILDER_SOURCE 
		${CMAKE_CURRENT_SOURCE_DIR}/adjust_counts.cc
		${CMAKE_CURRENT_SOURCE_DIR}/corpus_count.cc
		${CMAKE_CURRENT_SOURCE_DIR}/initial_probabilities.cc
		${CMAKE_CURRENT_SOURCE_DIR}/interpolate.cc
		${CMAKE_CURRENT_SOURCE_DIR}/output.cc
		${CMAKE_CURRENT_SOURCE_DIR}/pipeline.cc
	)


# Group these objects together for later use. 
#
# Given add_library(foo OBJECT ${my_foo_sources}),
# refer to these objects as $<TARGET_OBJECTS:foo>
#
add_library(kenlm_builder OBJECT ${KENLM_BUILDER_SOURCE})


# Compile the executable, linking against the requisite dependent object files
add_executable(lmplz lmplz_main.cc $<TARGET_OBJECTS:kenlm> $<TARGET_OBJECTS:kenlm_common> $<TARGET_OBJECTS:kenlm_builder> $<TARGET_OBJECTS:kenlm_util>)

# Link the executable against boost
target_link_libraries(lmplz ${Boost_LIBRARIES} pthread)

# Group executables together
set_target_properties(lmplz PROPERTIES FOLDER executables)

if(BUILD_TESTING)

    # Explicitly list the Boost test files to be compiled
    set(KENLM_BOOST_TESTS_LIST
      adjust_counts_test
      corpus_count_test
    )

    # Iterate through the Boost tests list   
    foreach(test ${KENLM_BOOST_TESTS_LIST})

      # Compile the executable, linking against the requisite dependent object files
      add_executable(${test} ${test}.cc $<TARGET_OBJECTS:kenlm> $<TARGET_OBJECTS:kenlm_common> $<TARGET_OBJECTS:kenlm_builder> $<TARGET_OBJECTS:kenlm_util>)

      # Require the following compile flag
      set_target_properties(${test} PROPERTIES COMPILE_FLAGS "-DBOOST_TEST_DYN_LINK -DBOOST_PROGRAM_OPTIONS_DYN_LINK")
  
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
