# Eigen3 less than 3.1.0 has a race condition: http://eigen.tuxfamily.org/bz/show_bug.cgi?id=466

if(ENABLE_INTERPOLATE)
  find_package(Eigen3 3.1.0 CONFIG REQUIRED)
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

  add_library(kenlm_interpolate ${KENLM_INTERPOLATE_SOURCE})
  target_link_libraries(kenlm_interpolate PUBLIC kenlm Eigen3::Eigen)

  find_package(OpenMP)
  if (OPENMP_CXX_FOUND)
    target_link_libraries(kenlm_interpolate PUBLIC OpenMP::OpenMP_CXX)
  endif()


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
endif()
