include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Using URL instead of GIT in FetchContent to speed up configuration step and 
# decrease download size

# ------------------------------------------------------------------------------
# google test
# ------------------------------------------------------------------------------

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz
)
FetchContent_MakeAvailable(googletest)

# ------------------------------------------------------------------------------
# protobuf
# ------------------------------------------------------------------------------

FetchContent_Declare(
  protobuf
  URL https://github.com/protocolbuffers/protobuf/archive/refs/tags/v21.9.tar.gz
)
set(protobuf_BUILD_TESTS OFF)
FetchContent_Populate(protobuf) # Grpc calls add_subdirectory for protobuf

# ------------------------------------------------------------------------------
# [abseil]
# ------------------------------------------------------------------------------

FetchContent_Declare(
  abseil
  URL https://github.com/abseil/abseil-cpp/archive/refs/tags/20220623.1.tar.gz
)
FetchContent_Populate(abseil)

# ------------------------------------------------------------------------------
# [cares]
# ------------------------------------------------------------------------------

FetchContent_Declare(
  cares
  URL https://github.com/c-ares/c-ares/archive/refs/tags/cares-1_18_1.tar.gz
)
FetchContent_Populate(cares)

# ------------------------------------------------------------------------------
# [boringssl]
# ------------------------------------------------------------------------------

FetchContent_Declare(
  boringssl
  URL https://boringssl.googlesource.com/boringssl/+archive/4fb158925f7753d80fb858cb0239dff893ef9f15.tar.gz
)
FetchContent_Populate(boringssl)

# ------------------------------------------------------------------------------
# [re2]
# ------------------------------------------------------------------------------

FetchContent_Declare(
  re2
  URL https://github.com/google/re2/archive/refs/tags/2022-06-01.tar.gz
)
FetchContent_Populate(re2)

# ------------------------------------------------------------------------------
# [zlib]
# ------------------------------------------------------------------------------

FetchContent_Declare(
  zlib
  URL https://github.com/madler/zlib/archive/refs/tags/v1.2.13.tar.gz
)
FetchContent_Populate(zlib)

# ------------------------------------------------------------------------------
# grpc
# ------------------------------------------------------------------------------

# Set up projects root for grpc to add_subdirectory on them
set(CARES_ROOT_DIR     ${cares_SOURCE_DIR})
set(ABSL_ROOT_DIR      ${abseil_SOURCE_DIR})
set(BORINGSSL_ROOT_DIR ${boringssl_SOURCE_DIR})
set(PROTOBUF_ROOT_DIR  ${protobuf_SOURCE_DIR})
set(RE2_ROOT_DIR       ${re2_SOURCE_DIR})
set(ZLIB_ROOT_DIR      ${zlib_SOURCE_DIR})

FetchContent_Declare(
  grpc
  URL https://github.com/grpc/grpc/archive/refs/tags/v1.51.0.tar.gz
)
FetchContent_MakeAvailable(protobuf grpc)

# Fix gcc compilation bug
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(BROKEN_HEADER ${grpc_SOURCE_DIR}/src/core/ext/xds/xds_listener.h)
  set(BROKEN_SOURCE ${grpc_SOURCE_DIR}/src/core/ext/xds/xds_listener.cc)
  execute_process(
    COMMAND bash ${PROJECT_SOURCE_DIR}/scripts/impl/fix-gcc-compilation.sh ${BROKEN_HEADER} ${BROKEN_SOURCE}
  )
endif()

# ------------------------------------------------------------------------------
# boost
# ------------------------------------------------------------------------------

set(BOOST_LIBRARIES 
  assert config preprocessor mp11 winapi pool context static_assert throw_exception
  type_traits container_hash move core integer detail intrusive predef smart_ptr
  array bind concept_check exception function iterator mpl range regex tuple
  unordered algorithm conversion io function_types fusion utility optional
  system align atomic type_index typeof variant2 filesystem format stacktrace
  fiber)
set(BOOST_VERSION boost-1.80.0)

foreach(lib ${BOOST_LIBRARIES})
  FetchContent_Declare(
          boost-${lib}
          URL https://github.com/boostorg/${lib}/archive/refs/tags/${BOOST_VERSION}.tar.gz
  )
  FetchContent_MakeAvailable(boost-${lib})
endforeach()

# ------------------------------------------------------------------------------
# cli11
# ------------------------------------------------------------------------------

FetchContent_Declare(
  cli11
  URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.3.1.tar.gz
)
FetchContent_MakeAvailable(cli11)

# ------------------------------------------------------------------------------
# rocksdb
# ------------------------------------------------------------------------------

FetchContent_Declare(
  rocksdb
  URL https://github.com/facebook/rocksdb/archive/refs/tags/v7.7.3.tar.gz
)
set(WITH_GFLAGS          OFF CACHE INTERNAL "")
set(WITH_TESTS           OFF CACHE INTERNAL "")
set(WITH_BENCHMARK_TOOLS OFF CACHE INTERNAL "")
set(WITH_TOOLS           OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(rocksdb)

add_library(store INTERFACE)
target_link_libraries(store INTERFACE rocksdb)
target_include_directories(store INTERFACE src ${rocksdb_SOURCE_DIR}/include)
