# Glide R2 compact vertex field and first rendering work log

## Confirmed

Direct `pumpit1` loader runs with `aot-dynamic` reached `grDrawTriangle` and continuously submitted more than 3,258 compact triangles with no gate rejection, caught exception, or OpenGL error. The PIU producer stride is 60 bytes. dwords 0/1 are screen-space float x/y; dword 3 and 7 are 255.0 in observed samples, dword 8 is 1.0, and dwords 9/10 vary as texture-coordinate candidates.

The initial renderer intentionally uses only x/y and submits opaque white triangles. Raw triangle stderr dumps were useful for discovery but are now removed from the hot path; the bounded in-memory ring trace remains available for future diagnostics.

## Next

1. Convert confirmed compact vertex values into a dedicated draw command/data type instead of decoding in the gate boundary.
2. Identify packed color fields (2/6 and related data) and render interpolated vertex color.
3. Implement R3 texture memory, download/source, and sampling using confirmed s/t candidates.
4. Add frame/swap counters and a screenshot or frame-hash verification path.
