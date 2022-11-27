include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
cmake_policy(SET CMP0135 NEW)

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
  URL https://boringssl.googlesource.com/boringssl/+archive/refs/heads/main-with-bazel.tar.gz
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

# ------------------------------------------------------------------------------
# boost
# ------------------------------------------------------------------------------

set(BOOST_LIBRARIES config preprocessor assert mp11 winapi pool context
                static_assert throw_exception type_traits container_hash move
                core integer detail intrusive predef smart_ptr array bind
                concept_check exception function iterator mpl range regex tuple
                unordered algorithm conversion io function_types fusion utility
                optional system align atomic type_index typeof variant2
                filesystem format fiber)
set(BOOST_VERSION boost-1.80.0)

foreach(lib ${BOOST_LIBRARIES})
  FetchContent_Declare(
          boost-${lib}
          URL https://github.com/boostorg/${lib}/archive/refs/tags/${BOOST_VERSION}.tar.gz
  )
  FetchContent_MakeAvailable(boost-${lib})
endforeach()
