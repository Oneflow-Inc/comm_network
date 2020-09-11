include(ExternalProject)
set (PROTOBUF_TAR_URL "https://github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protobuf-cpp-3.13.0.tar.gz")
set (PROTOBUF_URL_HASH "6425d7466db2efe5a80de1e38899f317")
set (PROTOBUF_SRC ${THIRD_PARTY_DIR}/protobuf)

ExternalProject_Add(protobuf
  DEPENDS zlib
  PREFIX ${PROTOBUF_SRC}
  URL ${PROTOBUF_TAR_URL}
  URL_MD5 ${PROTOBUF_URL_HASH}
  CONFIGURE_COMMAND ${CMAKE_COMMAND} ${PROTOBUF_SRC}/src/protobuf/cmake/
    -Dprotobuf_BUILD_TESTS:BOOL=OFF
    -DCMAKE_INSTALL_PREFIX:STRING=${PROTOBUF_SRC}
)

set (PROTOBUF_INCLUDE_DIR ${PROTOBUF_SRC}/google/protobuf)
set (PROTOBUF_LIBS_DIR ${PROTOBUF_SRC}/${CMAKE_INSTALL_LIBDIR})
set (PROTOBUF_LIBS ${PROTOBUF_LIBS_DIR}/libprotobuf.a)