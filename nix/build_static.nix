let
  pkgs = import ./source.nix;
in
  with pkgs;
  mkShell {
    name = "Static Build environment";
    packages = [ 
      patch
      cmake
      ninja
      llvmPackages_16.tools.libcxxClang
      cacert
      glibc.static
    ];
    shellHook = ''
      export CXX=clang++
      export CC=clang
      export CXXFLAGS="-I${llvmPackages_16.libcxxabi.dev}/include/c++/v1/"
      export CFLAGS="-I${llvmPackages_16.libcxxabi.dev}/include/c++/v1/"
    '';
  }
