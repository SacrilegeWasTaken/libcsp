{
  description = "libcps – smart pointers for C (header-only)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }: let
    eachSystem = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
    mkPkgs = system: nixpkgs.legacyPackages.${system};
  in {
    packages = eachSystem (system: let
      pkgs = mkPkgs system;
      libcps = pkgs.stdenv.mkDerivation {
        pname = "libcps";
        version = "0.1.0";
        src = self;
        dontBuild = true;
        installPhase = ''
          mkdir -p $out/include
          cp -r include/* $out/include/
        '';
      };
    in { default = libcps; });

    devShells = eachSystem (system: let
      pkgs = mkPkgs system;
      libcps = self.packages.${system}.default;
    in {
      default = pkgs.mkShell {
        buildInputs = [ pkgs.stdenv.cc ];
        C_INCLUDE_PATH = "${libcps}/include";
        shellHook = ''
          echo "libcps: C_INCLUDE_PATH is set to ${libcps}/include"
        '';
      };
    });

    checks = eachSystem (system: let
      pkgs = mkPkgs system;
      mkTest = std: pkgs.stdenv.mkDerivation {
        name = "libcps-test-${std}";
        src = self;
        buildInputs = [ pkgs.stdenv.cc ];
        buildPhase = ''
          cc -std=${std} -Wall -Wextra -pthread -Iinclude \
             -DCSP_IMPLEMENTATION -DCSP_DEBUG -DCSP_PANIC \
             test/test.c -o test_runner
        '';
        doCheck = true;
        checkPhase = "./test_runner";
        installPhase = "mkdir -p $out";
      };
    in {
      test-c11 = mkTest "c11";
      test-c23 = mkTest "c23";
    });
  };
}
