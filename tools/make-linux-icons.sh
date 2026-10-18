#!/bin/sh
# Makes the Linux icons in resources/linux/icons from the Mac icon's art,
# resources/KeyboardEDO.icon/Assets/Image.png (1024 px, its own plum
# background): a flat rounded square with a small margin, as GNOME and KDE
# icons are, rather than the Mac's shadow. Needs ImageMagick 7 (magick). Run
# from the repository's root after the art changes, and commit the PNGs.
set -e
art=resources/KeyboardEDO.icon/Assets/Image.png
out=resources/linux/icons
name=com.unlikelyware.keyboardedo

# 1024 px: the art in a 896 px square (64 px margin) with 160 px corners.
master=$(mktemp --suffix=.png)
trap 'rm -f "$master"' EXIT
magick "$art" -strip -resize 896x896 \
    \( -size 896x896 xc:none -fill white -draw "roundrectangle 0,0,895,895,160,160" \) \
    -compose DstIn -composite -compose Over \
    -background none -gravity center -extent 1024x1024 "PNG32:$master"

for size in 16 24 32 48 64 128 256 512; do
    mkdir -p "$out/${size}x${size}/apps"
    magick "$master" -filter Lanczos -resize ${size}x${size} -strip \
        "PNG32:$out/${size}x${size}/apps/$name.png"
done
