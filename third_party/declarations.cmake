# Using URL instead of GIT in FetchContent to speed up configuration step and 
# decrease download size

# ------------------------------------------------------------------------------
# Direct dependencies
# ------------------------------------------------------------------------------

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.13.0.tar.gz
  URL_HASH MD5=95b29f0038ec84a611df951d74d99897
)

if (MORF_FUZZ_TEST)
  FetchContent_Declare(
    fuzztest
    URL https://github.com/google/fuzztest/archive/4825da056b52636ebe895fa365b84036882efa8a.tar.gz
    URL_HASH MD5=38c902614e62abe1d1011834df7a8baa
    PATCH_COMMAND patch -p0 -s < ${CMAKE_CURRENT_LIST_DIR}/patches/fuzztest.patch
  )
endif()

FetchContent_Declare(
  protobuf
  URL https://github.com/protocolbuffers/protobuf/archive/refs/tags/v22.2.tar.gz
  URL_HASH MD5=6d8cd8470fd1ee1eacb6c12cade820e2
  PATCH_COMMAND patch -p0 -s < ${CMAKE_CURRENT_LIST_DIR}/patches/protobuf.patch
)

FetchContent_Declare(
  grpc
  URL https://github.com/grpc/grpc/archive/refs/tags/v1.55.1.tar.gz
  URL_HASH MD5=539c94b306dbc39e25bb643b09dd4391
  PATCH_COMMAND patch -p0 -s < ${CMAKE_CURRENT_LIST_DIR}/patches/grpc.patch
)

FetchContent_Declare(
  cli11
  URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.3.1.tar.gz
  URL_HASH MD5=80fa4eb988f0595ecb78c13871972ad4
)

FetchContent_Declare(
  rocksdb
  URL https://github.com/facebook/rocksdb/archive/refs/tags/v7.7.3.tar.gz
  URL_HASH MD5=3c5d371a8bd8340b1975ad016151709d
)

FetchContent_Declare(
  spdlog
  URL https://github.com/gabime/spdlog/archive/refs/tags/v1.11.0.tar.gz
  URL_HASH MD5=287c6492c25044fd2da9947ab120b2bd
)

set(BOOST_VERSION boost-1.80.0)
set(BOOST_LIBRARIES
  assert          e8ac059ed3d68381efddd6cc69d2a514
  config          46e9dc457e803e6429bb13d82d04e81c
  preprocessor    714ede2e6946c95283f89b452917890d
  mp11            c177e549fb3bcecab3fa6845f2de88b9
  winapi          37698c41db97f121777d90b51194f389
  pool            5b223929e79c43e2e63a5e9f09d35e20
  context         3cebd8023ee9916d5b0ea30be91f914c
  static_assert   d7073fb29aab9eb8c055ac021cdc5655
  throw_exception b250228627b71852250ad6e1ecf5da08
  type_traits     80ffa61832f8a4bce057314acd420832
  container_hash  a495307e41fc505c15a92a0ecc27e518
  move            f0ae0b0c275d63fddf1d1746bf6ea027
  core            8f055450f7f782c0a45530df235fc9ea
  integer         ff72eedefd2c41bc2f1b5036830c16d7
  detail          ed833ee99da4461c9f1e8205a91fa964
  intrusive       977192691025150626469c05ed69e9f6
  predef          08614c3186e5b41c8eacb03c519a9915
  smart_ptr       76f0b890b507f9c0b56853bcd847b838
  array           3742bcb1ffd6fceaf98a412252b0e9e2
  bind            f927e022d91ccc8bb860f2466205620f
  concept_check   df9d39e24ad47db3e0ad62efcd2fb0d3
  exception       dfa6e24edeb6ae42b84785389045838a
  function        6155a6e5a0f439a4496b8905a4b6df5c
  iterator        b0309f5eb1ed59df7ce6911ed346b37b
  mpl             7b6bb7656766b81f01388ed178c9a800
  range           9504abab2551aa7243286b9c48c29318
  regex           ef3d55a8aaeb48e7fee611e53736e0cb
  tuple           1ca6018c6f49fa4d9dad97ffb6aa9e2d
  unordered       34570d214944b6995f95503e53898423
  algorithm       08ccfecf9f06665e0ddddc2bf0cad9bc
  conversion      d382286b92e1b8a6d34e643f905aa9bd
  io              4b83c2fbf77c8068eac138efac0d702a
  function_types  b1f0d30c05048ab476e2354769b3f7fc
  fusion          551f0eca9a5d6d5a9409e180b0db9f29
  utility         412b6800ee7eb0fab31e87985f22fc27
  optional        2507ef592765402ab7cc3a7b0370f5c6
  system          530996c672f8b0239560b198b5c5a740
  align           3421d2fced36854941a9d9d9d7d5c73b
  atomic          a6b122319cbc74376f2f361463cc8136
  type_index      07f17f293fb59561b636ccafbf24707f
  typeof          c38246a9673ad2ff4864e451b289af7f
  variant2        e2dd6d3db2ec7780e702dc61a0da49ff
  filesystem      c545f0a07a25ed3fd9f0e6355f0b1b2f
  format          03beadeeedd00a6f752b9c7d1322a12a
  stacktrace      1653427ae54837dd2e61fdd74f5bfcbe # used by project
  fiber           79ebb7271561e737313c842497d3b4a8 # used by project
)

list(LENGTH BOOST_LIBRARIES BOOST_LIBRARIES_COUNT)
math(EXPR BOOST_LIBRARIES_MAX_INDEX "${BOOST_LIBRARIES_COUNT} - 1")

foreach(INDEX RANGE 0 ${BOOST_LIBRARIES_MAX_INDEX} 2)
  math(EXPR NEXT_INDEX "${INDEX} + 1")
  list(GET BOOST_LIBRARIES ${INDEX}      NAME)
  list(GET BOOST_LIBRARIES ${NEXT_INDEX} MD5)

  if (${NAME} STREQUAL "filesystem")
    FetchContent_Declare(
      boost-${NAME}
      URL https://github.com/boostorg/${NAME}/archive/refs/tags/${BOOST_VERSION}.tar.gz
      URL_HASH MD5=${MD5}
      PATCH_COMMAND patch -p0 -s < ${CMAKE_CURRENT_LIST_DIR}/patches/boost_filesystem.patch
    )
  else()
    FetchContent_Declare(
      boost-${NAME}
      URL https://github.com/boostorg/${NAME}/archive/refs/tags/${BOOST_VERSION}.tar.gz
      URL_HASH MD5=${MD5}
    )
  endif()
endforeach()

# ------------------------------------------------------------------------------
# Transitive dependencies
# ------------------------------------------------------------------------------

FetchContent_Declare(
  zlib
  URL https://github.com/madler/zlib/archive/refs/tags/v1.2.13.tar.gz
  URL_HASH MD5=9c7d356c5acaa563555490676ca14d23
)

FetchContent_Declare(
  abseil-cpp
  URL https://github.com/abseil/abseil-cpp/archive/refs/tags/20230125.3.tar.gz
  URL_HASH MD5=9b6dae642c4bd92f007ab2c148bc0498
)

FetchContent_Declare(
  re2
  URL https://github.com/google/re2/archive/refs/tags/2022-06-01.tar.gz
  URL_HASH MD5=cb629f38da6b7234a9e9eba271ded5d6
)

FetchContent_Declare(
  cares
  URL https://github.com/c-ares/c-ares/archive/refs/tags/cares-1_18_1.tar.gz
  URL_HASH MD5=a0ec0dd35fd6c06544333d250e21ab22
)

FetchContent_Declare(
  boringssl
  URL https://github.com/google/boringssl/archive/6c763bae33add3892c63048c12cf8282a27a2fae.tar.gz
  URL_HASH MD5=d0f16de89870467d096bf0518505ee0e
)

if (MORF_FUZZ_TEST)
  FetchContent_Declare(
    antlr_cpp
    URL https://www.antlr.org/download/antlr4-cpp-runtime-4.12.0-source.zip
    URL_HASH MD5=acf7371bd7562188712751266d8a7b90
  )

  FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.tar.gz
    URL_HASH MD5=e8d56bc54621037842ee9f0aeae27746
  )
endif()
