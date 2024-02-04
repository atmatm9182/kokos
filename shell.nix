with import <nixpkgs> {};

llvmPackages_13.stdenv.mkDerivation {
  name = "c-env";
  nativeBuildInputs = [ clang-tools meson ninja gdb ];
  hardeningDisable = [ "fortify" ];
}
