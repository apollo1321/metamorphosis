let
  pkgs = (import ./source.nix).latexPkgs;
in
pkgs.mkShell
{
  name = "LaTeX environment";
  packages = [
    pkgs.python311Packages.pygments
    pkgs.which
    pkgs.tectonic
  ];
}
