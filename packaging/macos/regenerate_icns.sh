#!/bin/bash

set -e
cd "$(dirname "$0")/../.."

if ! command -v rsvg-convert &> /dev/null; then
    echo "Please make sure to have 'rsvg-convert' on your PATH (brew install librsvg)!"
    echo "It rasterizes the SVG with a transparent background; qlmanage bakes a"
    echo "white background instead, which leaves a white border around the icon."
    exit 1
fi

# TangoQ macOS app icon source (a filled superellipse squircle on the macOS
# icon grid). TangoQ ships only the app icon; the DMG uses CPack's default
# volume icon, so there is no VolumeIcon.icns to regenerate.
input_svg="res/osx/tangoq_icon.svg"
tmp_dir="$(mktemp -dt tangoq_icon)"
output_dir="$tmp_dir.iconset"
output_icns="res/osx/application.icns"

mv "$tmp_dir" "$output_dir"

# We want $output_dir to expand now, therefore we disable the check
# shellcheck disable=SC2064
trap "rm -rf '$output_dir'" EXIT

echo "==> Generating icons from $input_svg..."

# name  width  (iconutil requires exactly these iconset members)
render() {
    rsvg-convert -w "$2" -h "$2" "$input_svg" -o "$output_dir/$1.png"
}
render icon_16x16        16
render icon_16x16@2x     32
render icon_32x32        32
render icon_32x32@2x     64
render icon_128x128     128
render icon_128x128@2x  256
render icon_256x256     256
render icon_256x256@2x  512
render icon_512x512     512
render icon_512x512@2x 1024

echo "==> Updating $output_icns..."
iconutil -c icns -o "$output_icns" "$output_dir"
