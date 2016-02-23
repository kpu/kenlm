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
    spaces.cc
		string_piece.cc
		usage.cc
	)

# This directory has children that need to be processed
add_subdirectory(double-conversion)
add_subdirectory(stream)

# Group these objects together for later use.
add_library(kenlm_util ${KENLM_UTIL_DOUBLECONVERSION_SOURCE} ${KENLM_UTIL_STREAM_SOURCE} ${KENLM_UTIL_SOURCE})

AddExes(EXES probing_hash_table_benchmark
        LIBRARIES kenlm_util ${Boost_LIBRARIES} pthread ${TIMER_LINK})

# Only compile and run unit tests if tests should be run
if(BUILD_TESTING)
  set(KENLM_BOOST_TESTS_LIST
    bit_packing_test
    integer_to_string_test
    joint_sort_test
    multi_intersection_test
    pcqueue_test
    probing_hash_table_test
    read_compressed_test
    sized_iterator_test
    sorted_uniform_test
    string_stream_test
    tokenize_piece_test
  )

  AddTests(TESTS ${KENLM_BOOST_TESTS_LIST}
           LIBRARIES kenlm_util ${Boost_LIBRARIES} pthread)

  # file_piece_test requires an extra command line parameter
  KenLMAddTest(TEST file_piece_test
               LIBRARIES kenlm_util ${Boost_LIBRARIES} pthread
               TEST_ARGS ${CMAKE_CURRENT_SOURCE_DIR}/file_piece.cc)
endif()
