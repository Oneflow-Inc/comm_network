include (ExternalProject)

SET(ABSL_TAR_URL https://github.com/abseil/abseil-cpp/archive/20200225.2.tar.gz)
SET(ABSL_URL_HASH 73f2b6e72f1599a9139170c29482ddc4)
 
SET(ABSL_SRC ${THIRD_PARTY_DIR}/absl)

ExternalProject_Add(absl
  PREFIX ${ABSL_SRC}
  URL ${ABSL_TAR_URL}
  URL_HASH MD5=${ABSL_URL_HASH}
  UPDATE_COMMAND ""
  CMAKE_CACHE_ARGS
    -DCMAKE_INSTALL_PREFIX:PATH=${ABSL_SRC}
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
    -DCMAKE_BUILD_TYPE:STRING=Release
)

set (ABSL_INCLUDE_DIR ${ABSL_SRC}/include)
set (ABSL_LIBS_DIR ${ABSL_SRC}/${CMAKE_INSTALL_LIBDIR})
set(ABSL_LIBS
  ${ABSL_LIBS_DIR}/libabsl_base.a
  ${ABSL_LIBS_DIR}/libabsl_spinlock_wait.a
  ${ABSL_LIBS_DIR}/libabsl_dynamic_annotations.a
  ${ABSL_LIBS_DIR}/libabsl_malloc_internal.a
  ${ABSL_LIBS_DIR}/libabsl_throw_delegate.a
  ${ABSL_LIBS_DIR}/libabsl_int128.a
  ${ABSL_LIBS_DIR}/libabsl_strings.a
  ${ABSL_LIBS_DIR}/libabsl_str_format_internal.a
  ${ABSL_LIBS_DIR}/libabsl_time.a
  ${ABSL_LIBS_DIR}/libabsl_bad_optional_access.a)