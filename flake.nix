{
  description = "vis-helix";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
    in
    flake-utils.lib.eachSystem systems (system:
      let
        pkgs = import nixpkgs { inherit system; };
        lua = pkgs.lua5_4;
        lpeg = pkgs.lua54Packages.lpeg;
        runtimeInputs = with pkgs; [
          libtermkey
          ncurses
          lua
          lpeg
          tre
        ] ++ pkgs.lib.optionals pkgs.stdenv.isDarwin [
          libiconv
        ];
        nativeInputs = with pkgs; [
          pkg-config
          gnumake
          makeWrapper
        ];
        luaVersion = lua.luaversion;
        package = pkgs.stdenv.mkDerivation {
          pname = "vis-helix";
          version = "dev";
          src = self;

          nativeBuildInputs = nativeInputs;
          buildInputs = runtimeInputs;

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
              --prefix LUA_PATH ';' '${lpeg}/share/lua/${luaVersion}/?.lua;;' \
              --prefix LUA_CPATH ';' '${lpeg}/lib/lua/${luaVersion}/?.so;${lpeg}/lib/lua/${luaVersion}/?.dylib;;' \
              --set VIS_PATH "$out/share/vis"
            runHook postInstall
          '';
        };
      in {
        packages.default = package;

        apps.default = {
          type = "app";
          program = "${package}/bin/vis";
          meta.description = "vis-helix editor";
        };

        checks.default = package;

        devShells.default = pkgs.mkShell {
          packages = nativeInputs ++ runtimeInputs ++ (with pkgs; [
            clang
          ]);
        };
      });
}
