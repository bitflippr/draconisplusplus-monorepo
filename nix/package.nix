{
  pkgs,
  lib,
  self,
  ...
}: let
  llvmPackages = pkgs.llvmPackages_20;
  isLinux = pkgs.stdenv.hostPlatform.isLinux;

  # Use libc++ on Linux too, so the standard library comes from the pinned LLVM
  # rather than whichever GCC nixpkgs defaults to. Darwin's stdenv already does.
  stdenv =
    if isLinux
    then pkgs.stdenvAdapters.useMoldLinker llvmPackages.libcxxStdenv
    else llvmPackages.stdenv;

  # Compiled C++ dependencies must share the binary's standard library.
  withLibcxx = pkg: args: pkg.override ({stdenv = llvmPackages.libcxxStdenv;} // args);

  sqlitecpp =
    if isLinux
    then
      (withLibcxx pkgs.sqlitecpp {inherit (pkgs.pkgsStatic) sqlite;}).overrideAttrs (old: {
        doCheck = false;
        cmakeFlags = old.cmakeFlags ++ ["-DSQLITECPP_BUILD_TESTS=OFF" "-DBUILD_SHARED_LIBS=OFF"];
      })
    else pkgs.pkgsStatic.sqlitecpp;

  boostUt = pkgs.callPackage ./boost-ut.nix {};

  deps = with pkgs;
    [
      (glaze.overrideAttrs rec {
        version = "9.0.0";

        src = pkgs.fetchFromGitHub {
          owner = "stephenberry";
          repo = "glaze";
          tag = "v${version}";
          hash = "sha256-dzvKhaSfaDuJ+yHep+OgC6kTRMW59kOtSDw2KD1drVI=";
        };
      })
      boostUt
      sqlitecpp
    ]
    ++ (with pkgs.pkgsStatic; [
      (magic-enum.overrideAttrs (old: {
        doCheck = false;
        cmakeFlags = (old.cmakeFlags or []) ++ ["-DMAGIC_ENUM_OPT_BUILD_TESTS=OFF"];
      }))
      boostUt
    ])
    ++ darwinPkgs
    ++ linuxPkgs;

  darwinPkgs = lib.optionals stdenv.isDarwin (with pkgs.pkgsStatic;
    [
      libiconv
      apple-sdk_15
    ]
    ++ [
      pkgs.darwin.sigtool
    ]);

  linuxPkgs = lib.optionals stdenv.isLinux (with pkgs;
    [
      valgrind
      (withLibcxx pugixml {})
    ]
    ++ (with pkgsStatic; [
      libxcb
      wayland
    ]));

  mkDraconisPackage = lib.makeOverridable ({native}:
    stdenv.mkDerivation {
      name =
        "draconis++"
        + (
          if native
          then "-native"
          else "-generic"
        );
      version = "0.1.0";
      src = self;

      nativeBuildInputs = with pkgs;
        [
          cmake
          gitMinimal
          meson
          ninja
          pkg-config
          python3
        ]
        ++ lib.optional stdenv.isLinux xxd;

      buildInputs = deps;

      mesonFlags = [
        "-Dbuild_examples=false"
        "-Db_lto=true"
        "-Dnative_tuning=${if native then "true" else "false"}"
        (lib.optionalString stdenv.isLinux "-Duse_linked_pci_ids=true")
        (lib.optionalString stdenv.isLinux "-Dpci_ids_path=${pkgs.pciutils}/share/pci.ids")
      ];

      configurePhase = ''
        meson setup build --prefix=/ --buildtype=release $mesonFlags
      '';

      buildPhase = ''
        meson compile -C build
      '';

      checkPhase = ''
        meson test -C build --print-errorlogs
      '';

      installPhase = ''
        meson install -C build --no-rebuild --destdir "$out"
      '';

      postFixup = lib.optionalString stdenv.isDarwin ''
        echo "Signing binary..."
        codesign --force -s - --identifier com.apple.draconisplusplus $out/bin/draconis++
      '';

      NIX_ENFORCE_NO_NATIVE =
        if native
        then 0
        else 1;
    });
in {
  "generic" = mkDraconisPackage {native = false;};
  "native" = mkDraconisPackage {native = true;};
}
