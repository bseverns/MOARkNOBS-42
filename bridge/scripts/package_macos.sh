#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARCH="${ARCH:-$(uname -m)}"
case "${ARCH}" in
  arm64|aarch64) ARCH_LABEL="arm64" ;;
  x86_64|amd64) ARCH_LABEL="x64" ;;
  *) echo "Unsupported macOS architecture: ${ARCH}" >&2; exit 1 ;;
esac

RELEASE_VERSION="${RELEASE_VERSION:-$(node -p "require('${ROOT_DIR}/package.json').version")}" 
OUTPUT_DIR="${OUTPUT_DIR:-${ROOT_DIR}/dist}"
RAW_PREFIX="${RAW_PREFIX:-${ROOT_DIR}/dist/mn42-bridge-${RELEASE_VERSION}}"
CONSOLE_BINARY="${CONSOLE_BINARY:-${RAW_PREFIX}-console-node24-macos-${ARCH_LABEL}}"
CLI_BINARY="${CLI_BINARY:-${RAW_PREFIX}-cli-node24-macos-${ARCH_LABEL}}"
REQUIRE_BRIDGE_SIGNING="${REQUIRE_BRIDGE_SIGNING:-0}"
APPLE_CODESIGN_IDENTITY="${APPLE_CODESIGN_IDENTITY:-}"
APPLE_NOTARY_KEY="${APPLE_NOTARY_KEY:-}"
APPLE_NOTARY_KEY_ID="${APPLE_NOTARY_KEY_ID:-}"
APPLE_NOTARY_ISSUER_ID="${APPLE_NOTARY_ISSUER_ID:-}"

for binary in "${CONSOLE_BINARY}" "${CLI_BINARY}"; do
  [[ -f "${binary}" ]] || { echo "Bridge binary not found: ${binary}" >&2; exit 1; }
done

if [[ "${REQUIRE_BRIDGE_SIGNING}" == "1" ]]; then
  for name in APPLE_CODESIGN_IDENTITY APPLE_NOTARY_KEY APPLE_NOTARY_KEY_ID APPLE_NOTARY_ISSUER_ID; do
    [[ -n "${!name}" ]] || { echo "${name} is required for signed macOS packaging" >&2; exit 1; }
  done
fi

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mn42-macos-package.XXXXXX")"
trap 'rm -rf "${WORK_DIR}"' EXIT
APP_PATH="${WORK_DIR}/MN42 Bridge.app"
CONTENTS="${APP_PATH}/Contents"
RESOURCES="${CONTENTS}/Resources"
mkdir -p "${CONTENTS}/MacOS" "${RESOURCES}" "${OUTPUT_DIR}"

cp "${CONSOLE_BINARY}" "${CONTENTS}/MacOS/MN42 Bridge"
cp "${CLI_BINARY}" "${RESOURCES}/mn42-bridge-cli"
chmod 755 "${CONTENTS}/MacOS/MN42 Bridge" "${RESOURCES}/mn42-bridge-cli"

PLIST="${CONTENTS}/Info.plist"
plutil -create xml1 "${PLIST}"
plutil -insert CFBundleDisplayName -string 'MN42 Bridge' "${PLIST}"
plutil -insert CFBundleExecutable -string 'MN42 Bridge' "${PLIST}"
plutil -insert CFBundleIdentifier -string 'com.mn42.bridge' "${PLIST}"
plutil -insert CFBundleName -string 'MN42 Bridge' "${PLIST}"
plutil -insert CFBundlePackageType -string 'APPL' "${PLIST}"
plutil -insert CFBundleShortVersionString -string "${RELEASE_VERSION#v}" "${PLIST}"
plutil -insert CFBundleVersion -string "${RELEASE_VERSION#v}" "${PLIST}"
plutil -insert LSMinimumSystemVersion -string '12.0' "${PLIST}"

SIGNED=false
NOTARIZED=false
if [[ "${REQUIRE_BRIDGE_SIGNING}" == "1" ]]; then
  ENTITLEMENTS="${WORK_DIR}/entitlements.plist"
  plutil -create xml1 "${ENTITLEMENTS}"
  plutil -insert com.apple.security.cs.allow-jit -bool true "${ENTITLEMENTS}"
  for binary in "${CONTENTS}/MacOS/MN42 Bridge" "${RESOURCES}/mn42-bridge-cli"; do
    codesign --force --timestamp --options runtime --entitlements "${ENTITLEMENTS}" --sign "${APPLE_CODESIGN_IDENTITY}" "${binary}"
  done
  codesign --force --timestamp --options runtime --entitlements "${ENTITLEMENTS}" --sign "${APPLE_CODESIGN_IDENTITY}" "${APP_PATH}"
  codesign --verify --deep --strict --verbose=2 "${APP_PATH}"
  SIGNED=true
fi

FINAL_APP="${OUTPUT_DIR}/MN42 Bridge.app"
rm -rf "${FINAL_APP}"
cp -R "${APP_PATH}" "${FINAL_APP}"

DMG_ROOT="${WORK_DIR}/dmg"
mkdir -p "${DMG_ROOT}"
cp -R "${APP_PATH}" "${DMG_ROOT}/MN42 Bridge.app"
ln -s /Applications "${DMG_ROOT}/Applications"
DMG_PATH="${OUTPUT_DIR}/MN42-Bridge-${RELEASE_VERSION}-${ARCH_LABEL}.dmg"
rm -f "${DMG_PATH}"
hdiutil create -quiet -volname 'MN42 Bridge' -srcfolder "${DMG_ROOT}" -ov -format UDZO "${DMG_PATH}"

if [[ "${REQUIRE_BRIDGE_SIGNING}" == "1" ]]; then
  codesign --force --timestamp --sign "${APPLE_CODESIGN_IDENTITY}" "${DMG_PATH}"
  xcrun notarytool submit "${DMG_PATH}" --key "${APPLE_NOTARY_KEY}" --key-id "${APPLE_NOTARY_KEY_ID}" --issuer "${APPLE_NOTARY_ISSUER_ID}" --wait
  xcrun stapler staple "${DMG_PATH}"
  xcrun stapler validate "${DMG_PATH}"
  spctl --assess --type open --context context:primary-signature --verbose=4 "${DMG_PATH}"
  NOTARIZED=true
fi

hdiutil verify "${DMG_PATH}" >/dev/null
RECEIPT="${OUTPUT_DIR}/MN42-Bridge-${RELEASE_VERSION}-${ARCH_LABEL}-signing-verification.json"
printf '{\n  "artifact": "%s",\n  "architecture": "%s",\n  "codeSignature": "%s",\n  "notarization": "%s",\n  "staple": "%s"\n}\n' \
  "$(basename "${DMG_PATH}")" "${ARCH_LABEL}" \
  "$([[ "${SIGNED}" == true ]] && echo valid || echo not-requested)" \
  "$([[ "${NOTARIZED}" == true ]] && echo accepted || echo not-requested)" \
  "$([[ "${NOTARIZED}" == true ]] && echo valid || echo not-requested)" > "${RECEIPT}"

printf '%s\n' "${FINAL_APP}" "${DMG_PATH}" "${RECEIPT}"
