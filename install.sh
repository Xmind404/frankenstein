#!/bin/sh
set -eu

repo="Xmind404/frankenstein"
base="https://github.com/$repo/releases/latest/download"
os=$(uname -s)
arch=$(uname -m)

print_logo_line() {
  color=$1
  line=$2
  if [ -t 1 ] && [ "${NO_COLOR-}" = "" ] && [ "${TERM-}" != "dumb" ]; then
    printf '\033[38;5;%sm%s\033[0m\n' "$color" "$line"
  else
    printf '%s\n' "$line"
  fi
}

print_logo() {
  print_logo_line 22 '  _____                      __                           __         .__        '
  print_logo_line 34 '_/ ____\___________    ____ |  | __ ____   ____   _______/  |_  ____ |__| ____  '
  print_logo_line 46 '\   __\\_  __ \__  \  /    \|  |/ // __ \ /    \ /  ___/\   __\/ __ \|  |/    \ '
  print_logo_line 44 ' |  |   |  | \// __ \|   |  \    <\  ___/|   |  \\___ \  |  | \  ___/|  |   |  \'
  print_logo_line 33 ' |__|   |__|  (____  /___|  /__|_ \___  >___|  /____  > |__|  \___  >__|___|  /'
  print_logo_line 27 '                   \/     \/     \/    \/     \/     \/            \/        \/'
  printf '\nFrankenstein installer\n\n'
}

print_logo

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
shell_name=${SHELL##*/}
if [ "$shell_name" = "fish" ]; then
  profile="$HOME/.config/fish/conf.d/frankenstein.fish"
  mkdir -p "$(dirname "$profile")"
  if ! grep -Fq 'fish_add_path' "$profile" 2>/dev/null; then
    printf "fish_add_path -- '%s'\n" "$install_dir" >> "$profile"
  fi
else
  case "$os:$shell_name" in
    Darwin:zsh|Linux:zsh) profile="$HOME/.zprofile" ;;
    Darwin:bash) profile="$HOME/.bash_profile" ;;
    *) profile="$HOME/.profile" ;;
  esac
  if ! grep -Fq '# >>> Frankenstein PATH >>>' "$profile" 2>/dev/null; then
    {
      printf '\n# >>> Frankenstein PATH >>>\n'
      printf 'case ":$PATH:" in *":%s:"*) ;; *) export PATH="%s:$PATH" ;; esac\n' "$install_dir" "$install_dir"
      printf '# <<< Frankenstein PATH <<<\n'
    } >> "$profile"
  fi
fi
printf 'Installed Frankenstein in %s/frankenstein\n' "$install_dir"
printf 'Updated PATH in %s\n' "$profile"
if [ "$shell_name" = "fish" ]; then
  printf 'Open a new fish terminal, or run: fish_add_path -- "%s"\n' "$install_dir"
else
  printf 'Open a new terminal, or run: . "%s"\n' "$profile"
fi
printf 'Then run: frankenstein --help\n'
