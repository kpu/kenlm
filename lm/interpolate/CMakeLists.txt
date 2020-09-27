# Eigen3 less than 3.1.0 has a race condition: http://eigen.tuxfamily.org/bz/show_bug.cgi?id=466
find_package(Eigen3 3.1.0 CONFIG)

if(EIGEN3_FOUND)
  include_directories(${EIGEN3_INCLUDE_DIR})

  set(KENLM_INTERPOLATE_SOURCE
    backoff_reunification.cc
    bounded_sequence_encoding.cc
    merge_probabilities.cc
    merge_vocab.cc
    normalize.cc
    pipeline.cc
    split_worker.cc
    tune_derivatives.cc
    tune_instances.cc
    tune_weights.cc
    universal_vocab.cc)

  find_package(OpenMP)
  if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  else()
    message(STATUS "OpenMP support would be nice for parallelizing matrix operations.")
  endif()

  add_library(kenlm_interpolate ${KENLM_INTERPOLATE_SOURCE})
  target_link_libraries(kenlm_interpolate PUBLIC kenlm Eigen3::Eigen)

  set(KENLM_INTERPOLATE_EXES
    interpolate
    streaming_example)

  set(KENLM_INTERPOLATE_LIBS
    kenlm_interpolate)

  AddExes(EXES ${KENLM_INTERPOLATE_EXES}
    LIBRARIES ${KENLM_INTERPOLATE_LIBS})

  if(BUILD_TESTING)
    AddTests(TESTS backoff_reunification_test bounded_sequence_encoding_test merge_vocab_test normalize_test tune_derivatives_test
      LIBRARIES ${KENLM_INTERPOLATE_LIBS} Threads::Threads)

    # tune_instances_test needs an extra command line parameter
    KenLMAddTest(TEST tune_instances_test
      LIBRARIES ${KENLM_INTERPOLATE_LIBS}
      TEST_ARGS -- ${CMAKE_CURRENT_SOURCE_DIR}/../common/test_data)
  endif()
else()
  message(WARNING "Not building interpolation. Eigen3 was not found.")
  message(STATUS "To install Eigen3 in your home directory, copy paste this:\n"
      "export EIGEN3_ROOT=$HOME/eigen-3.3.7\n"
      "(cd $HOME; wget -O - https://gitlab.com/libeigen/eigen/-/archive/3.3.7/eigen-3.3.7.tar.bz2 |tar xj)\n"
      "rm CMakeCache.txt\n")
endif()
