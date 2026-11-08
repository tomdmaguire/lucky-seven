#!/bin/bash
# Builds a macOS installer package for Lucky Seven.
# Usage: build-pkg.sh <version> <path to Lucky Seven.vst3> <path to Lucky Seven.component> <output .pkg>
set -euo pipefail

version="$1"
vst3="$2"
component="$3"
output="$4"
here="$(cd "$(dirname "$0")" && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

mkdir -p "$work/vst3/Library/Audio/Plug-Ins/VST3" "$work/au/Library/Audio/Plug-Ins/Components" "$work/packages"
ditto "$vst3" "$work/vst3/Library/Audio/Plug-Ins/VST3/Lucky Seven.vst3"
ditto "$component" "$work/au/Library/Audio/Plug-Ins/Components/Lucky Seven.component"

for kind in vst3 au; do
  pkgbuild --analyze --root "$work/$kind" "$work/$kind.plist" > /dev/null
  plutil -replace 0.BundleIsRelocatable -bool NO "$work/$kind.plist"
done

pkgbuild --quiet --root "$work/vst3" --component-plist "$work/vst3.plist" \
  --identifier com.luckyseven.reverb.vst3 --version "$version" --install-location / \
  "$work/packages/LuckySeven-VST3.pkg"

pkgbuild --quiet --root "$work/au" --component-plist "$work/au.plist" --scripts "$here/pkg-scripts/au" \
  --identifier com.luckyseven.reverb.au --version "$version" --install-location / \
  "$work/packages/LuckySeven-AU.pkg"

sed "s/__VERSION__/$version/g" "$here/distribution.xml" > "$work/distribution.xml"
productbuild --quiet --distribution "$work/distribution.xml" --package-path "$work/packages" \
  --resources "$here/pkg-resources" "$output"

echo "built $output"
