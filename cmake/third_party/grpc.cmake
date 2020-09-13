include (ExternalProject)
include(GNUInstallDirs)

set(GRPC_INCLUDE_DIR ${THIRD_PARTY_DIR}/grpc/include)
set(GRPC_LIBS_DIR ${THIRD_PARTY_DIR}/grpc/lib)
set(GRPC_BUILD_DIR ${THIRD_PARTY_DIR}/grpc/src/grpc)

SET(GRPC_TAR_URL https://github.com/grpc/grpc/archive/v1.27.3.tar.gz)
set(GRPC_URL_HASH 0c6c3fc8682d4262dd0e5e6fabe1a7e2)
SET(GRPC_SRC ${THIRD_PARTY_DIR}/grpc)

set (PROTOBUF_CONFIG_DIR ${PROTOBUF_SRC}/${CMAKE_INSTALL_LIBDIR}/cmake/protobuf)
set (ABSL_CONFIG_DIR ${ABSL_SRC}/${CMAKE_INSTALL_LIBDIR}/cmake/absl)
set (CARES_CONFIG_DIR ${CARES_SRC}/lib/cmake/c-ares)

if(WIN32)
  set(GRPC_LIBRARY_NAMES grpc++_unsecure.lib
  grpc_unsecure.lib gpr.lib upb.lib address_sorting.lib)
elseif(APPLE AND ("${CMAKE_GENERATOR}" STREQUAL "Xcode"))
  set(GRPC_LIBRARY_NAMES libgrpc++_unsecure.a
    libgrpc_unsecure.a libgpr.a libupb.a libaddress_sorting.a)
else()
  set(GRPC_LIBRARY_NAMES libgrpc++_unsecure.a
    libgrpc_unsecure.a libgpr.a libupb.a libaddress_sorting.a)
endif()

file(GLOB_RECURSE GRPC_HDRS "${GRPC_BUILD_DIR}/include/*.h")
foreach(LIBRARY_NAME ${GRPC_LIBRARY_NAMES})
  list(APPEND GRPC_LIBS ${GRPC_BUILD_DIR}/${LIBRARY_NAME})
endforeach()

ExternalProject_Add(grpc
  PREFIX ${GRPC_SRC}
  DEPENDS protobuf absl cares openssl zlib
  URL ${GRPC_TAR_URL}
  URL_HASH MD5=${GRPC_URL_HASH}
  UPDATE_COMMAND ""
  BUILD_COMMAND make -j${PROC_NUM} grpc grpc_unsecure grpc++_unsecure
  INSTALL_COMMAND ""
  BUILD_IN_SOURCE 1
  CMAKE_CACHE_ARGS
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_CXX_FLAGS_DEBUG:STRING=${CMAKE_CXX_FLAGS_DEBUG}
    -DCMAKE_CXX_FLAGS_RELEASE:STRING=${CMAKE_CXX_FLAGS_RELEASE}
    -DCMAKE_C_FLAGS_DEBUG:STRING=${CMAKE_C_FLAGS_DEBUG}
    -DCMAKE_C_FLAGS_RELEASE:STRING=${CMAKE_C_FLAGS_RELEASE}
    -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF
    -DCMAKE_INSTALL_PREFIX:PATH=${GRPC_SRC}
    -DgRPC_BUILD_TESTS:BOOL=OFF
    -DgRPC_ABSL_PROVIDER:STRING=package
    -Dabsl_DIR:PATH=${ABSL_CONFIG_DIR}
    -DgRPC_PROTOBUF_PROVIDER:STRING=package
    -DgRPC_PROTOBUF_PACKAGE_TYPE:STRING=CONFIG
    -DProtobuf_DIR:PATH=${PROTOBUF_CONFIG_DIR}
    -DgRPC_CARES_PROVIDER:STRING=package
    -Dc-ares_DIR:PATH=${CARES_CONFIG_DIR}
    -DgRPC_ZLIB_PROVIDER:STRING=package
    -DZLIB_ROOT:PATH=${ZLIB_SRC}
    -DgRPC_SSL_PROVIDER:STRING=package
    -DOPENSSL_ROOT_DIR:PATH=${OPENSSL_SRC}
)

add_custom_target(grpc_create_includes_symlink
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${GRPC_BUILD_DIR}/include ${GRPC_SRC}/include 
  DEPENDS grpc)

add_custom_target(grpc_create_library_dir
  COMMAND ${CMAKE_COMMAND} -E make_directory ${GRPC_LIBS_DIR}
  DEPENDS grpc)

add_custom_target(grpc_copy_libs_to_destination
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GRPC_LIBS} ${GRPC_LIBS_DIR}
  DEPENDS grpc_create_library_dir)

set (GRPC_INCLUDE_DIR ${GRPC_INCLUDE_DIR})