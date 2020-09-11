include (ExternalProject)

SET(OPENSSL_TAR_URL https://github.com/openssl/openssl/archive/OpenSSL_1_1_1g.tar.gz)
set(OPENSSL_URL_HASH dd32f35dd5d543c571bc9ebb90ebe54e)
SET(OPENSSL_SRC ${THIRD_PARTY_DIR}/openssl)

ExternalProject_Add(openssl
  PREFIX ${OPENSSL_SRC}
  URL ${OPENSSL_TAR_URL}
  URL_HASH MD5=${OPENSSL_URL_HASH}
  UPDATE_COMMAND ""
  CONFIGURE_COMMAND ${OPENSSL_SRC}/src/openssl/config --prefix=${OPENSSL_SRC}
  BUILD_COMMAND make -j${PROC_NUM}
  INSTALL_COMMAND make install
)
