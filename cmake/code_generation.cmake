function(generate_proto TARGET PROTO SIMULATION)
  get_filename_component(PROTO_PATH ${PROTO} ABSOLUTE)
  get_filename_component(PROTO_NAME ${PROTO} NAME_WLE)
  get_filename_component(IMPORTS_DIR ${PROTO_PATH} DIRECTORY)

  set(PROTO_DIR "${CMAKE_CURRENT_BINARY_DIR}")

  set(PROTO_SRC "${PROTO_DIR}/${PROTO_NAME}.pb.cc")
  set(PROTO_HDR "${PROTO_DIR}/${PROTO_NAME}.pb.h")

  if (SIMULATION)
    set(PREFIX "sim.")
    set(RPC_GENERATOR_EXE "sim_rpc_generator")
  else()
    set(PREFIX "")
    set(RPC_GENERATOR_EXE "rpc_generator")
  endif()

  set(CLIENT_SRC "${PROTO_DIR}/${PROTO_NAME}.${PREFIX}client.cc")
  set(CLIENT_HDR "${PROTO_DIR}/${PROTO_NAME}.${PREFIX}client.h")

  set(SERVICE_SRC "${PROTO_DIR}/${PROTO_NAME}.${PREFIX}service.cc")
  set(SERVICE_HDR "${PROTO_DIR}/${PROTO_NAME}.${PREFIX}service.h")

  add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HDR} ${CLIENT_SRC} ${CLIENT_HDR} ${SERVICE_SRC} ${SERVICE_HDR}
    COMMAND $<TARGET_FILE:protoc>
    ARGS 
      --proto_path "${protobuf_SOURCE_DIR}/src"
      --cpp_out "${PROTO_DIR}"
      --rpc_out "${PROTO_DIR}"
      -I "${IMPORTS_DIR}"
      --plugin=protoc-gen-rpc=$<TARGET_FILE:${RPC_GENERATOR_EXE}>
      "${PROTO_PATH}"
    DEPENDS "${PROTO_PATH}" ${RPC_GENERATOR_EXE}
  )

  add_library(${TARGET}
    ${PROTO_SRC}
    ${PROTO_HDR}
    ${CLIENT_SRC}
    ${CLIENT_HDR}
    ${SERVICE_SRC}
    ${SERVICE_HDR}
  )

  if (SIMULATION)
    target_link_libraries(${TARGET} runtime_simulator)
  else()
    target_link_libraries(${TARGET} runtime)
  endif()
endfunction()
