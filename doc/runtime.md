## Runtime

Boost fibers over `gRPC` implementation of `RPC` runtime are used. For
serialization/deserialization protobufs are used. `RPC` services are declared
in protobuf format.

### Code generation of RPC services

It is not so easy to adapt the fibers so that they work correctly with `gRPC`.
To simplify adding new `RPC` services, code generation has been written for
both clients and handlers.

To add new `RPC` service, you first need to declare it in protobuf format.
Stubs then can be generated. 

For handler stubs you need to implement service methods by overriding them.
When the client sends a request, the implemented methods will be launched in
the fiber. 

Clients are provided with an interface with the necessary `RPC` methods. When
calling the method, the current fiber will be suspended and after the response
is received, it will be resumed.

### Benchmarking

To test the runtime, a benchmark was written that checks the `RPS` for the
[echo service](../src/benchmark/echo_service.proto), which takes a string as
input, adds another one to it and returns it to the client. On `Apple M1 Pro,
10 cores` `RPS` reaches `~100K`.

### Runtime simulation

* RPC generator for runtime simulation
* 
