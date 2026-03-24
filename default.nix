{ pkgs ? import <nixpkgs> {} }:

pkgs.stdenv.mkDerivation rec {
  pname = "mosh-tcp";
  version = "1.4.0-tcp";

  src = pkgs.lib.cleanSource ./.;

  nativeBuildInputs = with pkgs; [
    autoconf
    automake
    pkg-config
    protobuf
    perl
  ];

  buildInputs = with pkgs; [
    openssl
    protobuf
    zlib
    ncurses
    libutempter
  ];

  preConfigure = ''
    ./autogen.sh
  '';

  configureFlags = [
    "--enable-compile-warnings=error"
  ];

  meta = with pkgs.lib; {
    description = "Mobile shell with TCP transport support";
    longDescription = ''
      Mosh (mobile shell) with native TCP transport. Use --protocol=tcp
      when UDP is blocked by firewalls or VPNs.
    '';
    homepage = "https://mosh.org";
    license = licenses.gpl3Plus;
    platforms = platforms.unix;
  };
}
