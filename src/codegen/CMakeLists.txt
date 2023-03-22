add_library(codegen util.cpp)
target_link_libraries(codegen ceq_lib libprotobuf libprotoc)
