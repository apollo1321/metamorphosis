set(ABSL_PROPAGATE_CXX_STD ON)
FetchContent_MakeAvailable(abseil-cpp)

FetchContent_MakeAvailable(googletest)

set(protobuf_BUILD_TESTS OFF)
set(protobuf_INSTALL OFF)
set(protobuf_WITH_ZLIB OFF)
FetchContent_MakeAvailable(protobuf)

set(RE2_BUILD_TESTING OFF)
FetchContent_MakeAvailable(re2)

if (MORF_FUZZ_TEST)
  FetchContent_MakeAvailable(antlr_cpp)
  FetchContent_MakeAvailable(fuzztest)
endif()

# ------------------------------------------------------------------------------

FetchContent_MakeAvailable(spdlog)
FetchContent_MakeAvailable(cli11)

# ------------------------------------------------------------------------------

FetchContent_Populate(cares)
FetchContent_Populate(boringssl)
FetchContent_Populate(zlib)

# ------------------------------------------------------------------------------

set(CARES_ROOT_DIR     ${cares_SOURCE_DIR})
set(ABSL_ROOT_DIR      ${abseil-cpp_SOURCE_DIR})
set(BORINGSSL_ROOT_DIR ${boringssl_SOURCE_DIR})
set(PROTOBUF_ROOT_DIR  ${protobuf_SOURCE_DIR})
set(RE2_ROOT_DIR       ${re2_SOURCE_DIR})
set(ZLIB_ROOT_DIR      ${zlib_SOURCE_DIR})

set(gRPC_BUILD_CSHARP_EXT              OFF CACHE INTERNAL "")
set(gRPC_BUILD_GRPC_NODE_PLUGIN        OFF CACHE INTERNAL "")
set(gRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN OFF CACHE INTERNAL "")
set(gRPC_BUILD_GRPC_PHP_PLUGIN         OFF CACHE INTERNAL "")
set(gRPC_BUILD_GRPC_PYTHON_PLUGIN      OFF CACHE INTERNAL "")
set(gRPC_BUILD_GRPC_RUBY_PLUGIN        OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(grpc)

# ------------------------------------------------------------------------------

set(WITH_GFLAGS          OFF CACHE INTERNAL "")
set(WITH_TESTS           OFF CACHE INTERNAL "")
set(WITH_BENCHMARK_TOOLS OFF CACHE INTERNAL "")
set(WITH_TOOLS           OFF CACHE INTERNAL "")
set(FAIL_ON_WARNINGS     OFF CACHE INTERNAL "")
set(ROCKSDB_BUILD_SHARED OFF CACHE INTERNAL "")
set(USE_RTTI             ON  CACHE INTERNAL "")

FetchContent_MakeAvailable(rocksdb)
add_library(rocksdb_lib INTERFACE)
target_link_libraries(rocksdb_lib INTERFACE rocksdb)
target_include_directories(rocksdb_lib INTERFACE ${rocksdb_SOURCE_DIR}/include)

# ------------------------------------------------------------------------------

if (${CMAKE_BUILD_TYPE} STREQUAL "Asan")
  add_compile_definitions(BOOST_USE_ASAN)
elseif (${CMAKE_BUILD_TYPE} STREQUAL "Tsan")
  add_compile_definitions(BOOST_USE_TSAN)
endif()

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  # Not working correctly for darwin
  if (${CMAKE_BUILD_TYPE} STREQUAL "Asan" OR 
      ${CMAKE_BUILD_TYPE} STREQUAL "Tsan")
    # Use appropriate context for tsan/asan
    # ucontext is less performant than fcontext (which is set by default)
    set(BOOST_CONTEXT_IMPLEMENTATION "ucontext" CACHE INTERNAL "")
  endif()
endif()

foreach(INDEX RANGE 0 ${BOOST_LIBRARIES_MAX_INDEX} 2)
  list(GET BOOST_LIBRARIES ${INDEX} NAME)
  FetchContent_MakeAvailable(boost-${NAME})
endforeach()
