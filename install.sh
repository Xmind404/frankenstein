#!/usr/bin/env bash
set -euo pipefail

repo='Xmind404/frankenstein'
base="https://github.com/$repo/releases/latest/download"
install_dir="${FRANKENSTEIN_INSTALL_DIR:-$HOME/.local/bin}"
binary="$install_dir/frankenstein"
temporary_directory=''
shell_name="${SHELL##*/}"

logo() {
  local colors=(22 28 34 40 46 51)
  local lines=(
    '  _____                      __                           __         .__        '
    '_/ ____\___________    ____ |  | __ ____   ____   _______/  |_  ____ |__| ____  '
    '\   __\\_  __ \__  \  /    \|  |/ // __ \ /    \ /  ___/\   __\/ __ \|  |/    \ '
    ' |  |   |  | \// __ \|   |  \    <\  ___/|   |  \___ \  |  | \  ___/|  |   |  \'
    ' |__|   |__|  (____  /___|  /__|_ \___  >___|  /____  > |__|  \___  >__|___|  /'
    '                   \/     \/     \/    \/     \/     \/            \/        \/'
  )
  local i
  for i in "${!lines[@]}"; do printf '\033[38;5;%sm%s\033[0m\n' "${colors[$i]}" "${lines[$i]}"; done
  printf '\nFrankenstein installer\n\n'
}

say() { printf '\033[32m◆\033[0m %s\n' "$*"; }
ask_tf() {
  local answer
  while true; do
    printf '\033[32m◆\033[0m %s [T/F] ' "$1" > /dev/tty
    IFS= read -r answer < /dev/tty || return 1
    case "$answer" in T|t) return 0 ;; F|f) return 1 ;; *) printf '│ Please enter T or F.\n' > /dev/tty ;; esac
  done
}

menu_action() {
  local -a options=("$@")
  local selected=0 key i tty_open=0
  if [[ -t 0 ]]; then tty_open=1; elif [[ -r /dev/tty ]]; then tty_open=1; fi
  if (( ! tty_open )); then
    printf 'Interactive terminal required. Set FRANKENSTEIN_ACTION=install|reinstall|uninstall to choose an action.\n' >&2
    return 2
  fi
  while true; do
    printf '\033[2J\033[H'; logo
    say 'Choose an action:'; printf '│\n'
    for i in "${!options[@]}"; do
      if (( i == selected )); then printf '\033[32m●\033[0m %s\n' "${options[$i]}"; else printf '\033[90m○\033[0m %s\n' "${options[$i]}"; fi
    done
    printf '\033[90m└  ↑/↓ to navigate • Enter: confirm\033[0m\n'
    IFS= read -rsn1 key < /dev/tty || return 2
    if [[ "$key" == $'\x1b' ]]; then
      IFS= read -rsn2 key < /dev/tty || true
      case "$key" in
        '[A') if (( selected > 0 )); then selected=$((selected - 1)); fi ;;
        '[B') if (( selected < ${#options[@]} - 1 )); then selected=$((selected + 1)); fi ;;
      esac
    elif [[ -z "$key" || "$key" == $'\n' || "$key" == $'\r' ]]; then
      printf '\n'; REPLY=$selected; return 0
    fi
  done
}

profile_path() {
  if [[ "$shell_name" == fish ]]; then printf '%s' "$HOME/.config/fish/conf.d/frankenstein.fish"
  elif [[ "$(uname -s)" == Darwin && "$shell_name" == zsh ]]; then printf '%s' "$HOME/.zprofile"
  elif [[ "$(uname -s)" == Darwin && "$shell_name" == bash ]]; then printf '%s' "$HOME/.bash_profile"
  elif [[ "$shell_name" == zsh ]]; then printf '%s' "$HOME/.zprofile"
  elif [[ "$shell_name" == bash && -f "$HOME/.bashrc" ]]; then printf '%s' "$HOME/.bashrc"
  else printf '%s' "$HOME/.profile"; fi
}

remove_path_block() {
  local profile="$1"
  [[ -f "$profile" ]] || return 0
  local temp; temp=$(mktemp)
  awk '/# >>> Frankenstein PATH >>>/{skip=1;next} /# <<< Frankenstein PATH <<</{skip=0;next} !skip{print}' "$profile" > "$temp"
  cat "$temp" > "$profile"; rm -f "$temp"
  if [[ "$profile" == */fish/conf.d/frankenstein.fish && ! -s "$profile" ]]; then rm -f "$profile"; fi
}

add_path() {
  local profile="$1"
  mkdir -p "$(dirname "$profile")"
  remove_path_block "$profile"
  if [[ "$shell_name" == fish ]]; then
    printf '# >>> Frankenstein PATH >>>\nfish_add_path -- %q\n# <<< Frankenstein PATH <<<\n' "$install_dir" >> "$profile"
  else
    printf '\n# >>> Frankenstein PATH >>>\ncase ":$PATH:" in *":%s:"*) ;; *) export PATH="%s:$PATH" ;; esac\n# <<< Frankenstein PATH <<<\n' "$install_dir" "$install_dir" >> "$profile"
  fi
}

show_manual_path() {
  local profile="$1"
  say "Add this directory to PATH: $install_dir"
  if [[ "$shell_name" == fish ]]; then
    printf '│ Run: fish_add_path -- %q\n│ Or add it to %s and run: source %q\n' "$install_dir" "$profile" "$profile"
  else
    printf '│ Add this line to %s:\n│ export PATH="%s:$PATH"\n│ Then reload it with: source %q\n' "$profile" "$install_dir" "$profile"
  fi
}

main() {
  logo
  local installed=0 action profile
  [[ -x "$binary" ]] && installed=1
  if [[ -n "${FRANKENSTEIN_ACTION:-}" ]]; then
    action=$(printf '%s' "$FRANKENSTEIN_ACTION" | tr '[:upper:]' '[:lower:]')
  elif (( installed )); then
    menu_action 'Reinstall' 'Uninstall' 'Exit' || exit $?
    case "$REPLY" in 0) action=reinstall ;; 1) action=uninstall ;; *) exit 0 ;; esac
  else
    menu_action 'Install' 'Exit' || exit $?
    case "$REPLY" in 0) action=install ;; *) exit 0 ;; esac
  fi
  if [[ "$action" != install && "$action" != reinstall && "$action" != uninstall ]]; then printf 'Invalid FRANKENSTEIN_ACTION: %s\n' "$action" >&2; exit 2; fi
  if ! ask_tf "Continue with $action?"; then say 'Cancelled.'; exit 0; fi
  profile=$(profile_path)
  if [[ "$action" == uninstall ]]; then
    rm -f "$binary"
    remove_path_block "$profile"
    say "Uninstalled Frankenstein from $install_dir"
    say "Removed the managed PATH entry from $profile"
    exit 0
  fi

  local os arch asset
  os=$(uname -s); arch=$(uname -m)
  case "$os:$arch" in
    Linux:x86_64|Linux:amd64) asset=frankenstein-linux-x86_64.tar.gz ;;
    Linux:aarch64|Linux:arm64) asset=frankenstein-linux-aarch64.tar.gz ;;
    Darwin:x86_64|Darwin:amd64) asset=frankenstein-macos-x86_64.tar.gz ;;
    Darwin:arm64|Darwin:aarch64) asset=frankenstein-macos-aarch64.tar.gz ;;
    *) printf 'Unsupported platform: %s %s\n' "$os" "$arch" >&2; exit 1 ;;
  esac
  for tool in curl tar; do command -v "$tool" >/dev/null || { printf 'Required tool not found: %s\n' "$tool" >&2; exit 1; }; done
  temporary_directory=$(mktemp -d)
  trap 'if [[ -n "$temporary_directory" ]]; then rm -rf "$temporary_directory"; fi' EXIT
  if ! curl --proto '=https' --tlsv1.2 -fsSL "$base/$asset" -o "$temporary_directory/$asset"; then
    printf 'No downloadable Release asset found. Publish a GitHub Release first: https://github.com/%s/releases\n' "$repo" >&2; exit 1
  fi
  curl --proto '=https' --tlsv1.2 -fsSL "$base/SHA256SUMS" -o "$temporary_directory/SHA256SUMS" || { echo 'Release is missing SHA256SUMS; refusing to install an unverified binary.' >&2; exit 1; }
  grep -F "  $asset" "$temporary_directory/SHA256SUMS" > "$temporary_directory/asset.sha256" || { echo "No checksum entry found for $asset" >&2; exit 1; }
  if command -v sha256sum >/dev/null 2>&1; then (cd "$temporary_directory" && sha256sum --check asset.sha256)
  elif command -v shasum >/dev/null 2>&1; then (cd "$temporary_directory" && shasum -a 256 --check asset.sha256)
  else echo 'SHA-256 verification requires sha256sum or shasum.' >&2; exit 1; fi
  tar -xzf "$temporary_directory/$asset" -C "$temporary_directory"
  [[ -f "$temporary_directory/bin/frankenstein" ]] || { echo 'Release archive does not contain bin/frankenstein.' >&2; exit 1; }
  mkdir -p "$install_dir"
  install -m 755 "$temporary_directory/bin/frankenstein" "$binary"
  say "Installed Frankenstein in $binary"
  if ask_tf 'Add the install directory to PATH automatically?'; then
    add_path "$profile"
    say "Updated PATH startup file: $profile"
    if [[ "$shell_name" == fish ]]; then say "Apply now with: source $(printf %q "$profile")"; else say "Apply now with: source $(printf %q "$profile")"; fi
  else
    show_manual_path "$profile"
  fi
  say 'Open a new terminal (or reload the startup file), then run: frankenstein --help'
}

main "$@"
