# Get all the test files
SET(TEST_SOURCES
  machine0.cpp
	machine1.cpp
)

foreach(test_file ${TEST_SOURCES})
  get_filename_component(test_name ${test_file} NAME_WE)
  add_executable(${test_name} ${test_file})
  target_link_libraries(${test_name} comm_network)
endforeach()
