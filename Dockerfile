FROM nixos/nix:2.16.1 AS builder

RUN echo "experimental-features = nix-command flakes" >> /etc/nix/nix.conf

WORKDIR /app

COPY flake.nix flake.nix
COPY flake.lock flake.lock
RUN nix develop .#buildStatic

COPY cmake cmake
COPY third_party third_party
COPY CMakeLists.txt .
RUN nix develop .#buildStatic -i --command cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja -DMORF_PRECOMPILE=ON -DMORF_BUILD_STATIC=ON
RUN nix develop .#buildStatic -i --command cmake --build build --target morf_precompile

COPY src src
RUN nix develop .#buildStatic -i --command cmake -B build -DMORF_PRECOMPILE=OFF -DMORF_LOG_LEVEL=DEBUG
RUN nix develop .#buildStatic -i --command cmake --build build --target mtf_raft_test_client mtf_raft_test_node mtf_history_checker_exe


FROM alpine:3.18
WORKDIR /app
COPY --from=builder /app/build/src/raft/test/exe/mtf_raft_test_client .
COPY --from=builder /app/build/src/raft/test/exe/mtf_raft_test_node .
COPY --from=builder /app/build/src/raft/test/system_test/mtf_history_checker_exe .
