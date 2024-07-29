with import <nixpkgs> {};

llvmPackages_17.stdenv.mkDerivation {
  name = "c-env";
  nativeBuildInputs = [ clang-tools_17 meson ninja lldb valgrind kcachegrind ];
  hardeningDisable = [ "fortify" ];
}
