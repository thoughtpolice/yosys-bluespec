{ repo ? builtins.fetchGit ./.
, versionFile ? ./.version
, officialRelease ? false

, nixpkgs ? null
, config ? {}
, system ? builtins.currentSystem
}:

let
  bootstrap = import ./nix/bootstrap.nix {
    inherit nixpkgs config system;
    inherit repo officialRelease versionFile;
  };
in
  
with bootstrap.pkgs;

stdenv.mkDerivation {
  pname = "yosys-bluespec";
  inherit (bootstrap) version;
  src = lib.cleanSource ./.;

  buildInputs = [ yosys readline zlib bluespec ];
  nativeBuildInputs = [ pkgconfig ];

  doCheck = true;
  makeFlags = [
    "PREFIX=$(out)/share/yosys/plugins"
    "STATIC_BSC_PATH=${bluespec}/bin/bsc"
    "STATIC_BSC_LIBDIR=${bluespec}/lib"
  ];
}
