let
  pkgs = import ./source.nix;
in
  with pkgs;
  mkShell {
    name = "LaTeX environment";
    packages = [ 
      tectonic
    ];
  }
