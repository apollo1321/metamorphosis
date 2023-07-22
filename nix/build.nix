let
  pkgs = (import ./source.nix).pkgs;
in
with pkgs;
mkShell {
  name = "Build environment";
  packages = [
    patch
    cmake
    ninja
    llvmPackages_16.tools.libcxxClang
    cacert
  ];
  shellHook = ''
    export CXX=clang++
    export CC=clang
    export CXXFLAGS="-I${llvmPackages_16.libcxxabi.dev}/include/c++/v1/"
    export CFLAGS="-I${llvmPackages_16.libcxxabi.dev}/include/c++/v1/"
  '';
}
