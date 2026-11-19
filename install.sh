#!/bin/sh
set -eu

repo="Xmind404/frankenstein"
base="https://github.com/$repo/releases/latest/download"
os=$(uname -s)
arch=$(uname -m)

case "$os:$arch" in
  Linux:x86_64|Linux:amd64) asset="frankenstein-linux-x86_64.tar.gz" ;;
  Linux:aarch64|Linux:arm64) asset="frankenstein-linux-aarch64.tar.gz" ;;
  Darwin:x86_64|Darwin:amd64) asset="frankenstein-macos-x86_64.tar.gz" ;;
  Darwin:arm64|Darwin:aarch64) asset="frankenstein-macos-aarch64.tar.gz" ;;
  *) printf 'Unsupported platform: %s %s\n' "$os" "$arch" >&2; exit 1 ;;
esac

if ! command -v curl >/dev/null 2>&1 || ! command -v tar >/dev/null 2>&1; then
  echo "This installer requires curl and tar." >&2
  exit 1
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
curl --proto '=https' --tlsv1.2 -fsSL "$base/$asset" -o "$tmp/$asset"
curl --proto '=https' --tlsv1.2 -fsSL "$base/SHA256SUMS" -o "$tmp/SHA256SUMS"
grep -F "  $asset" "$tmp/SHA256SUMS" > "$tmp/asset.sha256"
if command -v sha256sum >/dev/null 2>&1; then
  (cd "$tmp" && sha256sum --check --status asset.sha256)
elif command -v shasum >/dev/null 2>&1; then
  (cd "$tmp" && shasum -a 256 --check asset.sha256 >/dev/null)
else
  echo "SHA-256 verification requires sha256sum or shasum." >&2
  exit 1
fi

tar -xzf "$tmp/$asset" -C "$tmp"
install_dir=${FRANKENSTEIN_INSTALL_DIR:-"$HOME/.local/bin"}
mkdir -p "$install_dir"
install -m 755 "$tmp/bin/frankenstein" "$install_dir/frankenstein"
printf 'Installed Frankenstein in %s/frankenstein\n' "$install_dir"
case ":${PATH}:" in
  *":$install_dir:"*) ;;
  *) printf 'Add this directory to your PATH: %s\n' "$install_dir" ;;
esac
