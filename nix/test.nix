let
  pkgs = import ./source.nix;
in
  with pkgs;
  mkShell {
    name = "Test environment";
    packages = [ 
      docker
      python311
      python311Packages.docker
    ];
  }
