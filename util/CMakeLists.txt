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
# Because we do not set PARENT_SCOPE in the following definition,
#    CMake files in the parent directory won't be able to access this variable.
#
set(KENLM_UTIL_SOURCE 
		bit_packing.cc 
		ersatz_progress.cc 
		exception.cc 
		file.cc 
		file_piece.cc 
		float_to_string.cc
		integer_to_string.cc
		mmap.cc 
		murmur_hash.cc 
		parallel_read.cc
		pool.cc 
		read_compressed.cc 
		scoped.cc 
		string_piece.cc 
		usage.cc
	)

# This directory has children that need to be processed
add_subdirectory(double-conversion)
add_subdirectory(stream)


# Group these objects together for later use. 
#
# Given add_library(foo OBJECT ${my_foo_sources}),
# refer to these objects as $<TARGET_OBJECTS:foo>
#
add_library(kenlm_util OBJECT ${KENLM_UTIL_DOUBLECONVERSION_SOURCE} ${KENLM_UTIL_STREAM_SOURCE} ${KENLM_UTIL_SOURCE})



# Only compile and run unit tests if tests should be run
if(BUILD_TESTING)

    # Explicitly list the Boost test files to be compiled
    set(KENLM_BOOST_TESTS_LIST
      bit_packing_test
      file_piece_test
      joint_sort_test
      multi_intersection_test
      probing_hash_table_test
      read_compressed_test
      sorted_uniform_test
      tokenize_piece_test
    )

    # Iterate through the Boost tests list   
    foreach(test ${KENLM_BOOST_TESTS_LIST})

      # Compile the executable, linking against the requisite dependent object files
      add_executable(${test} ${test}.cc $<TARGET_OBJECTS:kenlm_util>)

      # Require the following compile flag
      set_target_properties(${test} PROPERTIES COMPILE_FLAGS -DBOOST_TEST_DYN_LINK)
  
      # Link the executable against boost
      target_link_libraries(${test} ${Boost_LIBRARIES} pthread)

      # file_piece_test requires an extra command line parameter
      if ("${test}" STREQUAL "file_piece_test")
        set(test_params 
            ${CMAKE_CURRENT_SOURCE_DIR}/file_piece.cc
        )
      else()
        set(test_params 
        )      
      endif()
            
      # Specify command arguments for how to run each unit test
      #
      # Assuming that foo was defined via add_executable(foo ...),
      #   the syntax $<TARGET_FILE:foo> gives the full path to the executable.
      #
      add_test(NAME ${test}_test 
               COMMAND $<TARGET_FILE:${test}> ${test_params})

      # Group unit tests together
      set_target_properties(${test} PROPERTIES FOLDER "unit_tests")

    # End for loop
    endforeach(test)

endif()
