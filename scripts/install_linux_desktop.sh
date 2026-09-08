#!/usr/bin/env bash
set -euo pipefail

SCRIPT_NAME="install_linux_desktop"
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

log() {
  printf '[%s] %s\n' "${SCRIPT_NAME}" "$*"
}

fail() {
  printf '[%s ERROR] %s\n' "${SCRIPT_NAME}" "$*" >&2
  exit 1
}

UNINSTALL=0
BINARY_PATH=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --uninstall)
      UNINSTALL=1
      shift
      ;;
    --binary)
      [[ $# -ge 2 ]] || fail "Missing path for --binary"
      BINARY_PATH="$2"
      shift 2
      ;;
    -h|--help)
      cat <<EOF
Usage: ./scripts/install_linux_desktop.sh [OPTIONS]

Installs devpiano desktop entry and icons to ~/.local/share for current user.

Options:
  --binary <path>   Path to the DevPiano executable (default: auto-detected)
  --uninstall       Remove desktop entry and icons from ~/.local/share
  -h, --help        Show this help
EOF
      exit 0
      ;;
    *)
      fail "Unknown argument: $1"
      ;;
  esac
done

APPS_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
ICON_256_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/256x256/apps"
ICON_SCALABLE_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/scalable/apps"

DESKTOP_FILE="${APPS_DIR}/devpiano.desktop"
PNG_ICON_FILE="${ICON_256_DIR}/devpiano.png"
SVG_ICON_FILE="${ICON_SCALABLE_DIR}/devpiano.svg"

if [[ "${UNINSTALL}" -eq 1 ]]; then
  log "Uninstalling DevPiano desktop entry and icons..."
  rm -f "${DESKTOP_FILE}" "${PNG_ICON_FILE}" "${SVG_ICON_FILE}"
  if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${APPS_DIR}" 2>/dev/null || true
  fi
  if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor" 2>/dev/null || true
  fi
  log "Uninstallation completed."
  exit 0
fi

# Locate binary if not specified
if [[ -z "${BINARY_PATH}" ]]; then
  CANDIDATE="${ROOT_DIR}/build-wsl-clang/devpiano_artefacts/Debug/DevPiano"
  if [[ -f "${CANDIDATE}" ]]; then
    BINARY_PATH="$(realpath "${CANDIDATE}")"
  else
    CANDIDATE_REL="${ROOT_DIR}/build-wsl-clang-release/devpiano_artefacts/Release/DevPiano"
    if [[ -f "${CANDIDATE_REL}" ]]; then
      BINARY_PATH="$(realpath "${CANDIDATE_REL}")"
    else
      BINARY_PATH="devpiano"
    fi
  fi
fi

log "Target binary: ${BINARY_PATH}"

mkdir -p "${APPS_DIR}" "${ICON_256_DIR}" "${ICON_SCALABLE_DIR}"

# Install icons
log "Installing icons..."
cp "${ROOT_DIR}/assets/branding/app-icon/icon-256.png" "${PNG_ICON_FILE}"
if [[ -f "${ROOT_DIR}/assets/branding/app-icon/app-icon.svg" ]]; then
  cp "${ROOT_DIR}/assets/branding/app-icon/app-icon.svg" "${SVG_ICON_FILE}"
fi

# Install desktop entry with concrete binary path if absolute
log "Generating desktop entry at ${DESKTOP_FILE}..."
cat > "${DESKTOP_FILE}" <<EOF
[Desktop Entry]
Type=Application
Name=DevPiano
GenericName=Piano Synthesizer
GenericName[zh_CN]=物理建模钢琴与合成器
Comment=Modern keyboard piano with physical modeling synthesis and VST3 host
Comment[zh_CN]=全物理建模钢琴与 VST3 插件宿主键盘应用
Exec="${BINARY_PATH}" %F
Icon=devpiano
Terminal=false
Categories=AudioVideo;Audio;Music;
StartupWMClass=DevPiano
MimeType=audio/midi;audio/x-midi;
Keywords=piano;synth;synthesizer;midi;vst3;audio;
EOF

chmod 644 "${DESKTOP_FILE}"

if command -v update-desktop-database >/dev/null 2>&1; then
  log "Updating desktop database..."
  update-desktop-database "${APPS_DIR}" 2>/dev/null || true
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  log "Updating GTK icon cache..."
  gtk-update-icon-cache -f -t "${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor" 2>/dev/null || true
fi

log "Desktop entry and icons installed successfully."
log "  Desktop: ${DESKTOP_FILE}"
log "  Icon 256: ${PNG_ICON_FILE}"
log "  Icon SVG: ${SVG_ICON_FILE}"
