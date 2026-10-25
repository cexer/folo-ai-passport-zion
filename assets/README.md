<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

Zion compiles `fonts/zion_font_14.c` (14px, 1 bpp, 6,873 characters). It is
generated from Google Fonts Noto Sans SC under SIL OFL 1.1; see
`fonts/NotoSansSC-OFL.txt`, `fonts/zion-font-manifest.json`, and
`../tools/generate-zion-font.py`. It has no kerning or compression and covers
GB2312 Han, ASCII and selected punctuation. No audio assets are linked
into the launcher. The UTF-8 inventory is `fonts/coverage.txt`.

The install page embeds `images/folo_mini_code_208.c`: the official FoloToy
WeChat Mini Program code, encoded as 208x208 I1 with an 8-byte palette (5,416
bytes total). Its published source, original WebP, 1-bit preview, raw data and
hash manifest are retained under `images/`. Convert using
`tools/generate-zion-mini-code.py`; optional Python dependencies are in
`tests/realdevice/requirements-zion-pixels.txt`. This service-issued graphic
cannot be replaced by generating a URL QR. The source is FoloToy's public
website; no separate asset redistribution license is stated there. Its rights
are distinct from the repository code license. It contains no device secrets.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Images

Store reusable source images and generated display assets in `images/`.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
