# Cost-effective distributed queue

## Dependencies

All libraries are automatically downloaded at the project configuration stage. `Boost fibers`, `protobuf`, `gRPC` and `gtest` are used directly. The rest of the dependencies are needed for the libraries themselves.

## Runtime

Boost fibers over `gRPC` implementation of `RPC` runtime are used. 
For serialization/deserialization protobufs are used. 
`RPC` services are declared in protobuf format.

### Code generation of services

It is not so easy to adapt the fibers so that they work correctly with `gRPC`. To simplify adding new `RPC` services, code generation has been written for both clients and handlers.

To add new `RPC` service, you first need to declare it in protobuf format. Stubs then can be generated. 

For handler stubs you need to implement service methods by overriding them. When the client sends a request, the implemented methods will be launched in the fiber. 

Clients are provided with an interface with the necessary `RPC` methods. When calling the method, the current fiber will be suspended and after the response is received, it will be resumed.

### Benchmarking

TODO (`~100K rps` on `Apple M1 Pro, 10 cores`)

## Distributed queue protocol

TODO

