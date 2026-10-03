# Phase 8 texture assets

All included PNG files are self-generated.

## Integrated material maps

- `materials/floor_tile_base.png` — colour data, uploaded with an sRGB internal format
- `materials/floor_tile_normal.png` — linear tangent-space normal data
- `materials/floor_tile_roughness.png` — linear roughness data

## Semantic fallback maps

- `fallback/white_base_colour.png`
- `fallback/black_metallic.png`
- `fallback/medium_roughness.png`
- `fallback/flat_normal.png`
- `fallback/white_ao.png`
- `fallback/black_emissive.png`

`TextureManager` caches by canonical path, texture semantic, and UV-flip flag.
One-channel textures use `GL_RED`, two-channel textures use `GL_RG`, colour RGB
and RGBA textures use `GL_SRGB`/`GL_SRGB_ALPHA`, and data maps remain linear.
A missing or unreadable texture produces one warning and a semantic fallback;
it does not make an object black or stop the application.
