{
  pkgs = import
    (fetchTarball {
      url = "https://github.com/NixOS/nixpkgs/archive/328bcf4d57f14f35f2e55d6ec3283c0bc8fd6892.tar.gz";
      sha256 = "sha256:0vgad7w236dsm3v3ja0l64jyvmmyyda4krj021niphz2rips7v70";
    })
    { };

  # Use old version of tectonic for biber version to be chosen correctly
  latexPkgs = import
    (builtins.fetchTarball {
      url = "https://github.com/NixOS/nixpkgs/archive/6adf48f53d819a7b6e15672817fa1e78e5f4e84f.tar.gz";
      sha256 = "sha256:0p7m72ipxyya5nn2p8q6h8njk0qk0jhmf6sbfdiv4sh05mbndj4q";
    })
    { };
}
