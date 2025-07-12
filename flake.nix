{
  description = "A development environment for the RAI robotics library";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};

      qhull-7-3-2 = pkgs.stdenv.mkDerivation {
        pname = "qhull";
        version = "7.3.2";

        src = pkgs.fetchFromGitHub {
          owner = "qhull";
          repo = "qhull";
          rev = "v7.3.2";
          sha256 = "sha256-ThBAerRRuTKcfCg1ERvw5syfNsIZf5Gvxi4UV/fi2y4=";
        };

        nativeBuildInputs = [ pkgs.cmake ];

        NIX_CFLAGS_COMPILE = "-fPIC";
        NIX_CXXFLAGS_COMPILE = "-fPIC";

        postInstall = ''
          mkdir -p $out/lib/pkgconfig
          cat > $out/lib/pkgconfig/qhull.pc << EOF
          prefix=$out
          exec_prefix=''${prefix}
          libdir=''${exec_prefix}/lib
          includedir=''${prefix}/include

          Name: qhull
          Description: Qhull library for convex hulls
          Version: $version
          Libs: -L''${libdir} -lqhullstatic
          Cflags: -I''${includedir}/libqhull
          EOF
        '';
      };
    in {
        devShells.${system}.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            cmake
            pkg-config
            gcc

            eigen
            qhull-7-3-2
            libf2c
            glfw
            glew
            freeglut
            jsoncpp
            assimp
            liblapack
            freetype
            hdf5
            xorg.libX11
            glm
            libccd

            yaml-cpp
          ];
        };
    };
}