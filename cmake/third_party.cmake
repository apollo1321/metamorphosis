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
  URL_HASH MD5=e82199374acdfda3f425331028eb4e2a
)
FetchContent_MakeAvailable(googletest)

# ------------------------------------------------------------------------------
# protobuf
# ------------------------------------------------------------------------------

FetchContent_Declare(
  protobuf
  URL https://github.com/protocolbuffers/protobuf/archive/refs/tags/v21.9.tar.gz
  URL_HASH MD5=2156fbdf1de8f16a479a116ce22de01c
)
set(protobuf_BUILD_TESTS OFF)
FetchContent_Populate(protobuf) # Grpc calls add_subdirectory for protobuf

# ------------------------------------------------------------------------------
# [abseil]
# ------------------------------------------------------------------------------

FetchContent_Declare(
  abseil
  URL https://github.com/abseil/abseil-cpp/archive/refs/tags/20220623.1.tar.gz
  URL_HASH MD5=2aea7c1171c4c280f755de170295afd6
)
FetchContent_Populate(abseil)

# ------------------------------------------------------------------------------
# [cares]
# ------------------------------------------------------------------------------

FetchContent_Declare(
  cares
  URL https://github.com/c-ares/c-ares/archive/refs/tags/cares-1_18_1.tar.gz
  URL_HASH MD5=a0ec0dd35fd6c06544333d250e21ab22
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
  URL_HASH MD5=cb629f38da6b7234a9e9eba271ded5d6
)
FetchContent_Populate(re2)

# ------------------------------------------------------------------------------
# [zlib]
# ------------------------------------------------------------------------------

FetchContent_Declare(
  zlib
  URL https://github.com/madler/zlib/archive/refs/tags/v1.2.13.tar.gz
  URL_HASH MD5=9c7d356c5acaa563555490676ca14d23
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
  URL_HASH MD5=95d8b3c07d4d3d1bd80472b22e3f05af
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

set(BOOST_VERSION boost-1.80.0)

FetchContent_Declare(
        boost-assert
        URL https://github.com/boostorg/assert/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=e8ac059ed3d68381efddd6cc69d2a514
)
FetchContent_MakeAvailable(boost-assert)

FetchContent_Declare(
        boost-config
        URL https://github.com/boostorg/config/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=46e9dc457e803e6429bb13d82d04e81c
)
FetchContent_MakeAvailable(boost-config)

FetchContent_Declare(
        boost-preprocessor
        URL https://github.com/boostorg/preprocessor/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=714ede2e6946c95283f89b452917890d
)
FetchContent_MakeAvailable(boost-preprocessor)

FetchContent_Declare(
        boost-mp11
        URL https://github.com/boostorg/mp11/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=c177e549fb3bcecab3fa6845f2de88b9
)
FetchContent_MakeAvailable(boost-mp11)

FetchContent_Declare(
        boost-winapi
        URL https://github.com/boostorg/winapi/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=37698c41db97f121777d90b51194f389
)
FetchContent_MakeAvailable(boost-winapi)

FetchContent_Declare(
        boost-pool
        URL https://github.com/boostorg/pool/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=5b223929e79c43e2e63a5e9f09d35e20
)
FetchContent_MakeAvailable(boost-pool)

FetchContent_Declare(
        boost-context
        URL https://github.com/boostorg/context/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=3cebd8023ee9916d5b0ea30be91f914c
)
FetchContent_MakeAvailable(boost-context)

FetchContent_Declare(
        boost-static_assert
        URL https://github.com/boostorg/static_assert/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=d7073fb29aab9eb8c055ac021cdc5655
)
FetchContent_MakeAvailable(boost-static_assert)

FetchContent_Declare(
        boost-throw_exception
        URL https://github.com/boostorg/throw_exception/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=b250228627b71852250ad6e1ecf5da08
)
FetchContent_MakeAvailable(boost-throw_exception)

FetchContent_Declare(
        boost-type_traits
        URL https://github.com/boostorg/type_traits/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=80ffa61832f8a4bce057314acd420832
)
FetchContent_MakeAvailable(boost-type_traits)

FetchContent_Declare(
        boost-container_hash
        URL https://github.com/boostorg/container_hash/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=a495307e41fc505c15a92a0ecc27e518
)
FetchContent_MakeAvailable(boost-container_hash)

FetchContent_Declare(
        boost-move
        URL https://github.com/boostorg/move/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=f0ae0b0c275d63fddf1d1746bf6ea027
)
FetchContent_MakeAvailable(boost-move)

FetchContent_Declare(
        boost-core
        URL https://github.com/boostorg/core/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=8f055450f7f782c0a45530df235fc9ea
)
FetchContent_MakeAvailable(boost-core)

FetchContent_Declare(
        boost-integer
        URL https://github.com/boostorg/integer/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=ff72eedefd2c41bc2f1b5036830c16d7
)
FetchContent_MakeAvailable(boost-integer)

FetchContent_Declare(
        boost-detail
        URL https://github.com/boostorg/detail/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=ed833ee99da4461c9f1e8205a91fa964
)
FetchContent_MakeAvailable(boost-detail)

FetchContent_Declare(
        boost-intrusive
        URL https://github.com/boostorg/intrusive/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=977192691025150626469c05ed69e9f6
)
FetchContent_MakeAvailable(boost-intrusive)

FetchContent_Declare(
        boost-predef
        URL https://github.com/boostorg/predef/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=08614c3186e5b41c8eacb03c519a9915
)
FetchContent_MakeAvailable(boost-predef)

FetchContent_Declare(
        boost-smart_ptr
        URL https://github.com/boostorg/smart_ptr/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=76f0b890b507f9c0b56853bcd847b838
)
FetchContent_MakeAvailable(boost-smart_ptr)

FetchContent_Declare(
        boost-array
        URL https://github.com/boostorg/array/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=3742bcb1ffd6fceaf98a412252b0e9e2
)
FetchContent_MakeAvailable(boost-array)

FetchContent_Declare(
        boost-bind
        URL https://github.com/boostorg/bind/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=f927e022d91ccc8bb860f2466205620f
)
FetchContent_MakeAvailable(boost-bind)

FetchContent_Declare(
        boost-concept_check
        URL https://github.com/boostorg/concept_check/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=df9d39e24ad47db3e0ad62efcd2fb0d3
)
FetchContent_MakeAvailable(boost-concept_check)

FetchContent_Declare(
        boost-exception
        URL https://github.com/boostorg/exception/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=dfa6e24edeb6ae42b84785389045838a
)
FetchContent_MakeAvailable(boost-exception)

FetchContent_Declare(
        boost-function
        URL https://github.com/boostorg/function/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=6155a6e5a0f439a4496b8905a4b6df5c
)
FetchContent_MakeAvailable(boost-function)

FetchContent_Declare(
        boost-iterator
        URL https://github.com/boostorg/iterator/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=b0309f5eb1ed59df7ce6911ed346b37b
)
FetchContent_MakeAvailable(boost-iterator)

FetchContent_Declare(
        boost-mpl
        URL https://github.com/boostorg/mpl/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=7b6bb7656766b81f01388ed178c9a800
)
FetchContent_MakeAvailable(boost-mpl)

FetchContent_Declare(
        boost-range
        URL https://github.com/boostorg/range/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=9504abab2551aa7243286b9c48c29318
)
FetchContent_MakeAvailable(boost-range)

FetchContent_Declare(
        boost-regex
        URL https://github.com/boostorg/regex/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=ef3d55a8aaeb48e7fee611e53736e0cb
)
FetchContent_MakeAvailable(boost-regex)

FetchContent_Declare(
        boost-tuple
        URL https://github.com/boostorg/tuple/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=1ca6018c6f49fa4d9dad97ffb6aa9e2d
)
FetchContent_MakeAvailable(boost-tuple)

FetchContent_Declare(
        boost-unordered
        URL https://github.com/boostorg/unordered/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=34570d214944b6995f95503e53898423
)
FetchContent_MakeAvailable(boost-unordered)

FetchContent_Declare(
        boost-algorithm
        URL https://github.com/boostorg/algorithm/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=08ccfecf9f06665e0ddddc2bf0cad9bc
)
FetchContent_MakeAvailable(boost-algorithm)

FetchContent_Declare(
        boost-conversion
        URL https://github.com/boostorg/conversion/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=d382286b92e1b8a6d34e643f905aa9bd
)
FetchContent_MakeAvailable(boost-conversion)

FetchContent_Declare(
        boost-io
        URL https://github.com/boostorg/io/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=4b83c2fbf77c8068eac138efac0d702a
)
FetchContent_MakeAvailable(boost-io)

FetchContent_Declare(
        boost-function_types
        URL https://github.com/boostorg/function_types/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=b1f0d30c05048ab476e2354769b3f7fc
)
FetchContent_MakeAvailable(boost-function_types)

FetchContent_Declare(
        boost-fusion
        URL https://github.com/boostorg/fusion/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=551f0eca9a5d6d5a9409e180b0db9f29
)
FetchContent_MakeAvailable(boost-fusion)

FetchContent_Declare(
        boost-utility
        URL https://github.com/boostorg/utility/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=412b6800ee7eb0fab31e87985f22fc27
)
FetchContent_MakeAvailable(boost-utility)

FetchContent_Declare(
        boost-optional
        URL https://github.com/boostorg/optional/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=2507ef592765402ab7cc3a7b0370f5c6
)
FetchContent_MakeAvailable(boost-optional)

FetchContent_Declare(
        boost-system
        URL https://github.com/boostorg/system/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=530996c672f8b0239560b198b5c5a740
)
FetchContent_MakeAvailable(boost-system)

FetchContent_Declare(
        boost-align
        URL https://github.com/boostorg/align/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=3421d2fced36854941a9d9d9d7d5c73b
)
FetchContent_MakeAvailable(boost-align)

FetchContent_Declare(
        boost-atomic
        URL https://github.com/boostorg/atomic/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=a6b122319cbc74376f2f361463cc8136
)
FetchContent_MakeAvailable(boost-atomic)

FetchContent_Declare(
        boost-type_index
        URL https://github.com/boostorg/type_index/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=07f17f293fb59561b636ccafbf24707f
)
FetchContent_MakeAvailable(boost-type_index)

FetchContent_Declare(
        boost-typeof
        URL https://github.com/boostorg/typeof/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=c38246a9673ad2ff4864e451b289af7f
)
FetchContent_MakeAvailable(boost-typeof)

FetchContent_Declare(
        boost-variant2
        URL https://github.com/boostorg/variant2/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=e2dd6d3db2ec7780e702dc61a0da49ff
)
FetchContent_MakeAvailable(boost-variant2)

FetchContent_Declare(
        boost-filesystem
        URL https://github.com/boostorg/filesystem/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=c545f0a07a25ed3fd9f0e6355f0b1b2f
)
FetchContent_MakeAvailable(boost-filesystem)

FetchContent_Declare(
        boost-format
        URL https://github.com/boostorg/format/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=03beadeeedd00a6f752b9c7d1322a12a
)
FetchContent_MakeAvailable(boost-format)

FetchContent_Declare(
        boost-stacktrace
        URL https://github.com/boostorg/stacktrace/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=1653427ae54837dd2e61fdd74f5bfcbe
)
FetchContent_MakeAvailable(boost-stacktrace)

FetchContent_Declare(
        boost-fiber
        URL https://github.com/boostorg/fiber/archive/refs/tags/${BOOST_VERSION}.tar.gz
        URL_HASH MD5=79ebb7271561e737313c842497d3b4a8
)
FetchContent_MakeAvailable(boost-fiber)

# ------------------------------------------------------------------------------
# cli11
# ------------------------------------------------------------------------------

FetchContent_Declare(
  cli11
  URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.3.1.tar.gz
  URL_HASH MD5=80fa4eb988f0595ecb78c13871972ad4
)
FetchContent_MakeAvailable(cli11)

# ------------------------------------------------------------------------------
# rocksdb
# ------------------------------------------------------------------------------

FetchContent_Declare(
  rocksdb
  URL https://github.com/facebook/rocksdb/archive/refs/tags/v7.7.3.tar.gz
  URL_HASH MD5=3c5d371a8bd8340b1975ad016151709d
)
set(WITH_GFLAGS          OFF CACHE INTERNAL "")
set(WITH_TESTS           OFF CACHE INTERNAL "")
set(WITH_BENCHMARK_TOOLS OFF CACHE INTERNAL "")
set(WITH_TOOLS           OFF CACHE INTERNAL "")
set(FAIL_ON_WARNINGS     OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(rocksdb)

add_library(store INTERFACE)
target_link_libraries(store INTERFACE rocksdb)
target_include_directories(store INTERFACE src ${rocksdb_SOURCE_DIR}/include)

# ------------------------------------------------------------------------------
# spdlog
# ------------------------------------------------------------------------------

FetchContent_Declare(
  spdlog
  URL https://github.com/gabime/spdlog/archive/refs/tags/v1.11.0.tar.gz
  URL_HASH MD5=287c6492c25044fd2da9947ab120b2bd
)
FetchContent_MakeAvailable(spdlog)
