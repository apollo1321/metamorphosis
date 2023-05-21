from io import BytesIO
import docker

dockerfile = '''
FROM ubuntu:22.04 AS builder

RUN apt update && \
    apt install clang-15 lld-15 ninja-build cmake patch -y

ENV CXX=/usr/bin/clang++-15
ENV CC=/usr/bin/clang-15

ENV LDFLAGS="-fuse-ld=lld"

WORKDIR /app

COPY cmake cmake
COPY CMakeLists.txt .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja -DMORF_BUILD_STATIC=ON -DMORF_PRECOMPILE=ON
RUN cmake --build build --target all

COPY src src
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja -DMORF_BUILD_STATIC=ON -DMORF_PRECOMPILE=OFF 
RUN cmake --build build --target ceq_raft_test_client ceq_raft_test_node


FROM alpine:3.18
WORKDIR /app
COPY --from=builder /app/build/src/raft/test/exe/ceq_raft_test_client .
COPY --from=builder /app/build/src/raft/test/exe/ceq_raft_test_node .
'''

f = BytesIO(dockerfile.encode('utf-8'))
cli = docker.from_env()
response = cli.api.build(path="/Users/apollo1321/Projects/cost-effective-queue",
                         tag='tag1', decode=True)

for l in response:
    print(l)
