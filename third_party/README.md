stb_image.h, stb_truetype.h (v1.26, SHA-256 ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab) and LICENSE from https://github.com/nothings/stb/tree/2c980bb59875b0d32144a71867fbdebb2f77cd20
Unmodified upstream image decoder and TrueType rasteriser. Used for the original JPEG-encoded RRI assets and for the TrueType fonts on the disc (TEXT/*.DLL).

SDL Android runtime: https://github.com/libsdl-org/SDL/releases/tag/release-3.4.8
The official `SDL3-devel-3.4.8-android.zip` archive has SHA-256
`c4c23cec585cb99ea626625dc06b421933dd78733b3841628d86be1c13ab16c6`.
Its unmodified `SDL3-3.4.8.aar` is in `android/app/libs/` and the license is `SDL-LICENSE.txt`.

Standalone macOS builds fetch the official SDL3-3.4.8.tar.gz source archive,
SHA-256 `e9fff7467fb60f037e6708da18b25560649e4c63edc2a69bb871b960d9cbfbba`.
`cmake/sdl3_static.cmake` verifies that digest and builds SDL statically.
The existing SDL and stb licences are copied into the application's Resources.
