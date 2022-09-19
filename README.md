# Cost-effective distributed queue

## Run

```bash
mkdir build && cd build
cmake -GNinja ..
ninja
./server_[a]sync 0.0.0.0:9999
./client_[a]sync 127.0.0.1:9999
```

## Dependencies

* gRPC
* boost

