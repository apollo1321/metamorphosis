# Cost-effective distributed queue

## Purpose

As a rule, for fault tolerance, distributed queues are configured so that
replicas are located in different [availability
zones](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/using-regions-availability-zones.html).

In this configuration, when writing to the queue, or when reading from the
queue, data is copied to the long-range scale several times.

The goal of this project is to reduce the number of copies of data over the
network in a distributed queue.

Reducing the amount of data transmission over the network in the task of
collecting a large amount of data will significantly reduce the
[cost](https://aws.amazon.com/ec2/pricing/on-demand/).

## Dependencies

All libraries are automatically downloaded at the project configuration stage.
`Boost fibers`, `protobuf`, `gRPC`, `gtest`, `CLI11` and `RocksDB` are used
directly. The rest of the dependencies are needed for the libraries themselves.

## Requirements

* `cmake >= 3.15`
* `ninja`
* `clang` or `gcc`

## Running

### Queue

At the moment, the implementation of the queue with 1 replica is written. The
data is stored on the hard disk and written synchronously. The queue supports
the following operations:

* `Append` - add message and get its `id`
* `Read` - read message at given `id`
* `Trim` - delete all messages in range `[0, id)`

```sh
./scripts/build-queue.sh
./scripts/run-queue-service.sh &
./scripts/run-queue-client.sh -h
```

### Benchmark

```sh
./scripts/build-benchmark.sh
./scripts/run-benchmark.sh
```

### More info

* [runtime](/docs/runtime.md)
* [protocol](/docs/queue_protocol.md)
