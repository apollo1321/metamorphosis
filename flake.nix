{
  description = "Metamorphosis development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-23.11";
    flake-utils.url = "github:numtide/flake-utils";

    # Use old version of tectonic for biber version to be chosen correctly
    nixpkgsTex.url = "github:NixOS/nixpkgs/nixos-22.11";
  };

  outputs = { self, nixpkgs, flake-utils, nixpkgsTex }:
    let
      system = flake-utils.lib.system.x86_64-linux;
      pkgs = nixpkgs.legacyPackages.${system};
      buildPackages = with pkgs; [
        patch
        cmake
        ninja
        llvmPackages_16.tools.libcxxClang
        cacert
      ];
      shellHook = ''
        export CXX=clang++
        export CC=clang
        export CXXFLAGS="-I${pkgs.llvmPackages_16.libcxxabi.dev}/include/c++/v1/"
        export CFLAGS="-I${pkgs.llvmPackages_16.libcxxabi.dev}/include/c++/v1/"
      '';

      pkgsTex = nixpkgsTex.legacyPackages.${system};
    in
    {
      devShells.${system} = rec {
        default = build;

        build = pkgs.mkShell {
            name = "Build environment";
            packages = buildPackages;
            inherit shellHook;
        };

        buildStatic = pkgs.mkShell {
          name = "Static build environment";
          packages = buildPackages ++ [pkgs.glibc.static];
          inherit shellHook;
        };

        systest = pkgs.mkShell {
          name = "Systest environment";
          packages = with pkgs; [
            docker
            python311
            python311Packages.docker
          ];
        };

        latex = with pkgsTex; mkShell {
          name = "LaTeX environment";
          packages = [
            python311Packages.pygments
            which
            tectonic
          ];
        };
      };
    };
}
