include (zlib)
include (protobuf)
include (openssl)
include (absl)
include (cares)
include (grpc)

set (comm_network_third_party_libs
  ${GRPC_LIBS})

set (comm_network_third_party_includes
  ${GRPC_INCLUDE_DIR})

# select all the source codes
file(GLOB_RECURSE comm_network_all_srcs "comm_network/*.*")
foreach(comm_network_single_file ${comm_network_all_srcs})
  if("${comm_network_single_file}" MATCHES "^${PROJECT_SOURCE_DIR}/comm_network/.*\\.cpp$")
    list(APPEND of_all_obj_cc ${comm_network_single_file})
  endif()
  if("${comm_network_single_file}" MATCHES "^${PROJECT_SOURCE_DIR}/comm_network/.*\\.h$")
    list(APPEND of_all_obj_cc ${comm_network_single_file})
  endif()
endforeach()

set (third_party_dependencies 
  grpc_create_includes_symlink
  grpc_copy_libs_to_destination
)

# compile project code to shared library
add_custom_target(prepare_third_party ALL DEPENDS ${third_party_dependencies})
add_library(comm_network SHARED ${of_all_obj_cc})
add_dependencies(comm_network prepare_third_party)
target_include_directories(comm_network PUBLIC ${PROJECT_SOURCE_DIR})
target_include_directories(comm_network PUBLIC ${comm_network_third_party_includes})
target_link_libraries(comm_network ${comm_network_third_party_libs})