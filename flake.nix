{
  description = "vis development shell + build package";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        lua = pkgs.lua5_4;
        lpeg = pkgs.lua54Packages.lpeg;
      in {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            clang
            gnumake
            pkg-config
            libtermkey
            ncurses
            lua
            lpeg
            tre
          ] ++ pkgs.lib.optionals pkgs.stdenv.isDarwin [
            libiconv
          ];
        };

        packages.default = pkgs.stdenv.mkDerivation {
          pname = "vis";
          version = "dev";
          src = self;

          nativeBuildInputs = with pkgs; [ pkg-config gnumake makeWrapper ];
          buildInputs = with pkgs; [
            libtermkey
            ncurses
            lua
            lpeg
            tre
          ] ++ pkgs.lib.optionals pkgs.stdenv.isDarwin [
            libiconv
          ];

          configurePhase = ''
            runHook preConfigure
            ./configure --prefix=$out
            runHook postConfigure
          '';

          buildPhase = ''
            runHook preBuild
            make
            runHook postBuild
          '';

          installPhase = ''
            runHook preInstall
            make install PREFIX=$out
            wrapProgram $out/bin/vis \
              --prefix LUA_PATH ';' '${lpeg}/share/lua/5.4/?.lua;;' \
              --prefix LUA_CPATH ';' '${lpeg}/lib/lua/5.4/?.so;;' \
              --set VIS_PATH "$out/share/vis"
            runHook postInstall
          '';
        };
      });
}
