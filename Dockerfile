FROM nixos/nix:2.16.1 AS builder

WORKDIR /app

COPY default.nix default.nix
COPY nix nix
RUN nix-shell

COPY cmake cmake
COPY third_party third_party
COPY CMakeLists.txt .
RUN nix-shell ./nix/build_static.nix --run "cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja -DMORF_PRECOMPILE=ON -DMORF_BUILD_STATIC=ON"
RUN nix-shell ./nix/build_static.nix --run "cmake --build build --target morf_precompile"

COPY src src
RUN nix-shell ./nix/build_static.nix --run "cmake -B build -DMORF_PRECOMPILE=OFF"
RUN nix-shell ./nix/build_static.nix --run "cmake --build build --target ceq_raft_test_client ceq_raft_test_node ceq_history_checker_exe"


FROM alpine:3.18
WORKDIR /app
COPY --from=builder /app/build/src/raft/test/exe/ceq_raft_test_client .
COPY --from=builder /app/build/src/raft/test/exe/ceq_raft_test_node .
COPY --from=builder /app/build/src/raft/test/system_test/ceq_history_checker_exe .
