{
  description = "A development environment for the RAI robotics library";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};

      ann = pkgs.stdenv.mkDerivation {
        pname = "libann";
        version = "1.1.2";

        src = pkgs.fetchFromGitHub {
          owner = "daveb-dev";
          repo = "libANN";
          rev = "6035fe3959a17c9d197e412b0cdd5361af9bcaeb";
          sha256 = "sha256-7gnh9FwCt5/stAaR0w13g4WVf3FvcHK4LO0oMfLo4aI=";
        };

        buildPhase = ''
          make linux-g++-sl
        '';

        installPhase = ''
          mkdir -p $out/lib $out/include
          cp lib/libANN.so $out/lib/
          cp -r include/ANN/ $out/include/
        '';
      };

      fcl = pkgs.stdenv.mkDerivation {
        pname = "fcl";
        version = "0.5.0";

        src = pkgs.fetchFromGitHub {
          owner = "flexible-collision-library";
          repo = "fcl";
          rev = "0.5.0";
          sha256 = "sha256-IzFhXz7HJjdVvBf9bt2S8xxyeaioBrmoh220kQlrpL4=";
        };

        nativeBuildInputs = with pkgs; [
          cmake
          libccd
        ];
      };

      qhull = pkgs.stdenv.mkDerivation {
        pname = "qhull";
        version = "7.3.2";

        src = pkgs.fetchFromGitHub {
          owner = "qhull";
          repo = "qhull";
          rev = "v7.3.2";
          sha256 = "sha256-ThBAerRRuTKcfCg1ERvw5syfNsIZf5Gvxi4UV/fi2y4=";
        };

        nativeBuildInputs = [ pkgs.cmake ];
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

      raiBuildInputs = with pkgs; [
        cmake
        pkg-config
        gcc
        eigen
        qhull
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
        fcl
        ann
      ];
    in {
        devShells.${system}.default = pkgs.mkShell {
          buildInputs = raiBuildInputs;
        };

        packages.${system}.default = pkgs.stdenv.mkDerivation {
          pname = "rai";
          version = "0.1.0";
          src = self;
          nativeBuildInputs = raiBuildInputs;
          NIX_CFLAGS_COMPILE = "-I${pkgs.eigen}/include/eigen3";
          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DUSE_PHYSX=OFF"
          ];
        };
    };
}