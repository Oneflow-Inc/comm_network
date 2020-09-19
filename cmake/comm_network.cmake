find_package(Threads)
include (glog)
include (zlib)
include (protobuf)
include (openssl)
include (absl)
include (cares)
include (grpc)
include (proto2cpp)
include (python)

add_custom_target(formatter
  COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/run_clang_format.py --clang_format_binary clang-format --source_dir ${CMAKE_CURRENT_SOURCE_DIR}/comm_network --fix --quiet
)

set (comm_network_third_party_libs
  ${CMAKE_THREAD_LIBS_INIT}
  ${GRPC_LIBS}
  ${GLOG_LIBS}
  ${ZLIB_LIBS}
  ${CARES_LIBS}
  ${ABSL_LIBS}
  ${PROTOBUF_LIBS})

set (comm_network_third_party_includes
  ${GLOG_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIR}
  ${CARES_INCLUDE_DIR}
  ${ABSL_INCLUDE_DIR}
  ${GRPC_INCLUDE_DIR}
  ${PROTOBUF_INCLUDE_DIR})

# rdma supported
if(UNIX)
  include(CheckIncludeFiles)
  include(CheckLibraryExists)
  CHECK_INCLUDE_FILES(infiniband/verbs.h HAVE_VERBS_H)
  CHECK_LIBRARY_EXISTS(ibverbs ibv_create_qp "" HAVE_IBVERBS)
  if(HAVE_VERBS_H AND HAVE_IBVERBS)
    list(APPEND comm_network_third_party_libs -libverbs)
    add_definitions(-DWITH_RDMA)
  elseif(HAVE_VERBS_H)
    message(FATAL_ERROR "RDMA library not found")
  elseif(HAVE_IBVERBS)
    message(FATAL_ERROR "RDMA head file not found")
  else()
    message(FATAL_ERROR "RDMA library and head file not found")
  endif()
else()
  message(FATAL_ERROR "UNIMPLEMENTED")
endif()

# select all the source codes
file(GLOB_RECURSE comm_network_all_srcs "comm_network/*.*")
foreach(comm_network_single_file ${comm_network_all_srcs})
  if("${comm_network_single_file}" MATCHES "^${PROJECT_SOURCE_DIR}/comm_network/.*\\.cpp$")
    list(APPEND all_obj_cc ${comm_network_single_file})
  endif()
  if("${comm_network_single_file}" MATCHES "^${PROJECT_SOURCE_DIR}/comm_network/.*\\.proto$")
    list(APPEND all_proto ${comm_network_single_file})
    set(group_this ON)
  endif()
  if("${comm_network_single_file}" MATCHES "^${PROJECT_SOURCE_DIR}/comm_network/.*\\.h$")
    list(APPEND all_obj_cc ${comm_network_single_file})
  endif()
endforeach()

# transfer proto file
foreach(proto_name ${all_proto})
  file(RELATIVE_PATH proto_rel_name ${PROJECT_SOURCE_DIR} ${proto_name})
  list(APPEND all_rel_protos ${proto_rel_name})
endforeach()
RELATIVE_PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS
  ${PROJECT_SOURCE_DIR}
  ${all_rel_protos})

set (third_party_dependencies 
  grpc_create_includes_symlink
  grpc_copy_libs_to_destination
  grpc_create_bin_symlink
  zlib
  cares
  protobuf
  glog
)

# compile project code to shared library
add_custom_target(prepare_third_party ALL DEPENDS ${third_party_dependencies})
add_library(comm_network STATIC ${all_obj_cc} ${PROTO_SRCS} ${PROTO_HDRS})
add_dependencies(comm_network prepare_third_party)
if (USE_CLANG_FORMAT)
  add_dependencies(comm_network formatter)
endif()
target_include_directories(comm_network PUBLIC ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(comm_network PUBLIC ${comm_network_third_party_includes})
target_link_libraries(comm_network ${comm_network_third_party_libs})