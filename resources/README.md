# resources

What the app is built with besides code, and where each came from.

- `KeyboardEDO.icon`: the app icon, an Icon Composer document (KeyEDO's design
  in the "Ember" palette). The Mac build compiles it with Xcode's `actool`
  (CMakeLists.txt). Its source is the author's artwork folder.
- `linux/icons`: the Linux icons (16 to 512 px, the freedesktop hicolor
  layout), made from `KeyboardEDO.icon/Assets/Image.png` by
  `tools/make-linux-icons.sh` (ImageMagick): run it again when the art
  changes. The 256 px one is also embedded as the window icon.
- `linux/com.unlikelyware.keyboardedo.desktop`: the Linux desktop entry.
- `brand/Unlikelyware-logo-144.png`: the Unlikelyware logo drawn at the left of
  the toolbar. It is `Resources/Unlikelyware-Final-Color.png` from IFSynth and
  PlainOldSynth (1000×1200), scaled to 144 px tall with ImageMagick's Lanczos
  filter so the toolbar never shrinks a large texture:
  `magick Unlikelyware-Final-Color.png -filter Lanczos -resize x144 -strip Unlikelyware-logo-144.png`.
- `fonts/Karla-Regular.ttf`: the wordmark's font, a sans with some
  character, copied from Dear ImGui's `misc/fonts`. (The plugins use
  Almendra, the unlikelyware.com face, but as the app's only serif it looked
  out of place.) By Jonathan Pinhorn, under the SIL Open Font License 1.1,
  whose text is `fonts/OFL.txt`; it must ship with the font. "Karla" is a
  Reserved Font Name, so a modified version of the font can't be called
  that.

- `help/help.md`: the help page's source, in Markdown Web Writer's dialect
  (cards, disclosures, `(#anchor)` lines). `help/help.html` is its export,
  with the CSS embedded, made in Markdown Web Writer by the author: export it
  again after editing `help.md`. The build installs it with the logo beside it
  as `Unlikelyware.png`, the name the page uses.

- `fonts/DejaVuSans-Latin.ttf`: the bar's and Settings' font, the plugins'
  DejaVu Sans (2.37, Ubuntu's fonts-dejavu-core), cut to Latin, general
  punctuation, arrows and the flat, natural and sharp signs with fontTools:
  `python3 -m fontTools.subset DejaVuSans.ttf --unicodes="U+0020-007E,U+00A0-017F,U+2010-2027,U+2030-203A,U+2190-2193,U+2212,U+00D7,U+266D-266F,U+FFFD" --layout-features='kern,liga' --output-file=DejaVuSans-Latin.ttf`.
  Its licence, which must ship with it, is `fonts/DejaVu-LICENSE.txt`.

The logo and fonts are embedded in the executable at build time
(`cmake/EmbedFile.cmake`), as the monitor's font is.
