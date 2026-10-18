# resources

What the app is built with besides code, and where each came from.

- `KeyboardEDO.icon`: the app icon, an Icon Composer document (KeyEDO's design
  in the "Ember" palette). The Mac build compiles it with Xcode's `actool`
  (CMakeLists.txt). Its source is the author's artwork folder.
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

The logo and font are embedded in the executable at build time
(`cmake/EmbedFile.cmake`), as the monitor's font is.
