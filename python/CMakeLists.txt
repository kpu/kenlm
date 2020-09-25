find_package(PythonInterp REQUIRED)
find_package(PythonLibs ${PYTHON_VERSION_STRING} EXACT REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

add_library(kenlm_python MODULE kenlm.cpp score_sentence.cc)
set_target_properties(kenlm_python PROPERTIES OUTPUT_NAME kenlm)
set_target_properties(kenlm_python PROPERTIES PREFIX "")

if(APPLE)
  set_target_properties(kenlm_python PROPERTIES SUFFIX ".so")
elseif(WIN32)
  set_target_properties(kenlm_python PROPERTIES SUFFIX ".pyd")
endif()

target_link_libraries(kenlm_python PUBLIC kenlm)
if(WIN32)
  target_link_libraries(kenlm_python PUBLIC ${PYTHON_LIBRARIES})
elseif(APPLE)
  set_target_properties(kenlm_python PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif()

if (WIN32)
  set (PYTHON_SITE_PACKAGES Lib/site-packages)
else ()
  set (PYTHON_SITE_PACKAGES lib/python${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}/site-packages)
endif ()

install(TARGETS kenlm_python DESTINATION ${PYTHON_SITE_PACKAGES})
