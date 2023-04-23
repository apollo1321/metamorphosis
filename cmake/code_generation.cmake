function(generate_proto TARGET PROTO)
  get_filename_component(PROTO_PATH ${PROTO} ABSOLUTE)
  get_filename_component(PROTO_NAME ${PROTO} NAME_WLE)
  get_filename_component(IMPORTS_DIR ${PROTO_PATH} DIRECTORY)

  set(PROTO_DIR "${CMAKE_CURRENT_BINARY_DIR}")

  set(PROTO_SRC "${PROTO_DIR}/${PROTO_NAME}.pb.cc")
  set(PROTO_HDR "${PROTO_DIR}/${PROTO_NAME}.pb.h")

  set(CLIENT_HDR "${PROTO_DIR}/${PROTO_NAME}.client.h")
  set(SERVICE_HDR "${PROTO_DIR}/${PROTO_NAME}.service.h")

  set(CLIENT_PROD_HDR "${PROTO_DIR}/${PROTO_NAME}.client.prod.h")
  set(CLIENT_PROD_SRC "${PROTO_DIR}/${PROTO_NAME}.client.prod.cc")
  set(SERVICE_PROD_HDR "${PROTO_DIR}/${PROTO_NAME}.service.prod.h")
  set(SERVICE_PROD_SRC "${PROTO_DIR}/${PROTO_NAME}.service.prod.cc")

  set(CLIENT_SIM_HDR "${PROTO_DIR}/${PROTO_NAME}.client.sim.h")
  set(CLIENT_SIM_SRC "${PROTO_DIR}/${PROTO_NAME}.client.sim.cc")
  set(SERVICE_SIM_HDR "${PROTO_DIR}/${PROTO_NAME}.service.sim.h")
  set(SERVICE_SIM_SRC "${PROTO_DIR}/${PROTO_NAME}.service.sim.cc")

  set(OUTPUT_FILES
    ${PROTO_SRC}
    ${PROTO_HDR}
    ${CLIENT_HDR}
    ${SERVICE_HDR}
    ${CLIENT_PROD_HDR}
    ${CLIENT_PROD_SRC}
    ${SERVICE_PROD_HDR}
    ${SERVICE_PROD_SRC}
    ${CLIENT_SIM_HDR}
    ${CLIENT_SIM_SRC}
    ${SERVICE_SIM_HDR}
    ${SERVICE_SIM_SRC}
  )

  add_custom_command(
    OUTPUT ${OUTPUT_FILES}
    COMMAND $<TARGET_FILE:protoc>
    ARGS
      --proto_path "${protobuf_SOURCE_DIR}/src"
      --cpp_out "${PROTO_DIR}"
      --rpc_out "${PROTO_DIR}"
      -I "${IMPORTS_DIR}"
      --plugin=protoc-gen-rpc=$<TARGET_FILE:ceq_rpc_generator>
      "${PROTO_PATH}"
    DEPENDS "${PROTO_PATH}" ceq_rpc_generator
  )

  add_library(${TARGET}_prod
    ${PROTO_SRC}
    ${CLIENT_PROD_SRC}
    ${SERVICE_PROD_SRC}
  )
  target_link_libraries(${TARGET}_prod ceq_runtime_production)

  add_library(${TARGET}_sim
    ${PROTO_SRC}
    ${CLIENT_SIM_SRC}
    ${SERVICE_SIM_SRC}
  )
  target_link_libraries(${TARGET}_sim ceq_runtime_simulator)
endfunction()
