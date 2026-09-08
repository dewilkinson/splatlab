# Design: Splat coarse-level billboards

Requirements: docs/requirements/splat-coarse-level-billboards.md
Status: agreed
Date: 2026-09-07

## Status, 2026-09-07 late
Level 0 draws as discs too for now (`kSplatDiscMinLevel = 0`), since the Gaussian level 0 is
switched off (`kSplatGaussianLevel0 = false` in `SurfelsApp.cpp`). Everything about the discs
themselves stands and now applies to every level.

## Revision, 2026-09-07 evening (final)
The clause "a level 0 chunk mid cross-fade is drawn as discs" is withdrawn, and so are the
intermediate forms tried the same evening (a per-pixel dither, an opacity ramp with the parent
discs solid, a per-record disc-to-Gaussian blend). The code now does this: level 0 never draws
discs; the disc condition is `lod >= kSplatDiscMinLevel` only. The level 1 to 0 hand-over is an
opacity cross-fade with no dither: the level 1 parent's discs are appended with weight `1 - t` and
the level 0 children's Gaussians with weight `t`, both at full coverage, so the weights sum to one
at every pixel and the Gaussian halos grow in with `t`. A hand-over begins only once nothing on the
path to level 0 is still streaming, so the whole level 0 in view fades in together. See
docs/design/splat-transition-shimmer.md, revision note. Everything about levels 1 and up stands.

## Summary
In splat mode the per-slot splat builder learns the chunk's level and, for level 1 and above, emits
a camera-facing disc instead of a projected Gaussian: a quad spanned by the camera's right and up
vectors scaled by the level's point spacing times 1.5, with the existing solid-dot rule for
sub-pixel discs. A level-0 chunk whose cross-fade weight is below 1 is drawn as discs as well, so
the dithered LOD transition is disc-to-disc and the chunk becomes Gaussians only once resident.
The disc reuses the Gaussian quad structure (clip-space centre plus two clip-space half-axes), so
the mesh and vertex shaders that place corners are unchanged. Discs use the splat mode's own
bounded Gaussian falloff (zero at the disc edge) times the record's opacity, so the pixel shader
only needs the `disc` interpolant to make sub-pixel solid dots opaque. Level 0 goes through the
untouched Gaussian code, byte for byte. The app hands the renderer one radius per level (spacing
times 1.0) in a new constant-buffer array, and the status banner names the rule. No UI, no config
key, no format change, and the withdrawn bleed mask stays out of the tree.

## Components touched
| Area | Files | Change |
|------|-------|--------|
| Splat shaders | `src/SurfelsCore/Shaders/Surfels.hlsl` | `g_SplatDiscRadius[2]` in the constant buffer; `disc`/`solid` on `GaussianSplat`; `disc` interpolant on `VSOut`; `BuildGaussianSplat` takes the level and has a disc branch; `SplatFromSlot` passes the chunk level; `GaussianCornerVertex` copies `disc`/`solid`; `mainPS` disc branch inside the splat-mode block. |
| Renderer | `src/SurfelsCore/SurfelsRenderer.h`, `src/SurfelsCore/SurfelsRenderer.cpp` | `State::splatDiscRadius[8]`; `SurfelsCB::splatDiscRadius[8]` appended on a 16-byte row; filled in `OnRender`. |
| App | `src/SurfelsCore/SurfelsApp.cpp` | Per-frame copy of the level spacing (times 1.0) into `State::splatDiscRadius`; banner SPLAT line gains "levels 1+ as discs". |
| Docs | `docs/USER_GUIDE.md` | One sentence in the splat-mode description: every level above 0 draws as camera-facing discs sized from the level's point spacing. |

## Data flow
```mermaid
flowchart LR
  A[ComputeLodSpacing\nmedian spacing per level] --> B[State::splatDiscRadius = spacing x 1.5]
  B --> C[SurfelsCB g_SplatDiscRadius]
  D[Sorted slot -> chunk.lodLevel,\nchunk.blendWeight] --> E{level >= 1 or |weight| < 1,\nand radius > 0}
  E -->|no| F[Gaussian path\nunchanged]
  E -->|yes| G[Disc: centre clip,\naxes = VP x camRight/camUp x r,\nSolidDot, disc = 1]
  F --> H[GaussianCornerVertex]
  G --> H
  H --> I{mainPS splat block}
  I -->|disc, solid| J[alpha = 1]
  I -->|disc or gaussian| K[reference bounded falloff x opacity, unchanged]
```

1. **Radius per level.** The app already measures each level's median nearest-neighbour spacing on
   load (`ComputeLodSpacing`, trace line per level) for streamed packages, including splat packages,
   whose chunks carry decoded positions. Every frame the app writes `spacing[l] * 1.0f` into
   `State::splatDiscRadius[l]`, with the multiplier a named constant `kSplatDiscCoverage = 1.5f`
   (0 for levels without a measurement). The renderer copies it into the constant buffer. The Auto Splat Size checkbox and Coverage slider of the disc modes are not
   involved.
2. **Level and fade at the slot.** `SplatFromSlot` already reads the chunk for the blend weight; it
   passes `chunk.lodLevel` and that weight into `BuildGaussianSplat` (0 and 1 when the pipeline is
   not chunked).
3. **Disc branch in `BuildGaussianSplat`.** After decoding position, opacity, DC colour and the SH
   scale, and after the view-space and clip-space centre with the existing depth clamp: when
   `lod >= 1` or `abs(blendWeight) < 0.999`, and the level's radius is above 0, build `tX = g_CamRight * r`, `tY = g_CamUp * r`,
   apply `SolidDot` with the distance from the viewer eye (the same call the disc modes make), then
   `axis1 = mul(g_ViewProj, float4(tX, 0)).xy`, `axis2 = mul(g_ViewProj, float4(tY, 0)).xy`. Because
   both tangents are perpendicular to the view direction, the corners share the centre's clip w and
   z, so the existing corner placement `clipCenter + (cc.x * axis1 + cc.y * axis2, 0, 0)` is exact.
   Reject when `abs(clip.xy) - (abs(axis1) + abs(axis2)) > clip.w` on either axis. Set
   `uvScale = 1`, `alpha = opacity`, `disc = 1`, `solid` from `SolidDot`. Otherwise fall through to
   the covariance path exactly as it is today, with `disc = 0`, `solid = 0`. The colour block
   (DC plus harmonics, clamp, optional linear conversion) is shared by both branches and is not
   reordered internally.
4. **Corner and interpolants.** `GaussianCornerVertex` copies `gs.disc` to the new `VSOut::disc`
   and `gs.solid` to the existing `VSOut::solid`. `CulledSplatVertex` and `SplatCornerVertex` set
   `disc = 0`.
5. **Pixel shader.** Inside the existing `g_RenderMode == 3` block, before the Gaussian falloff:
   `if (i.disc > 0.5 && i.solid > 0.5) return float4(i.color, 1.0);`. Everything else falls into
   the existing normalised falloff `(exp(-4 d) - e^-4) / (1 - e^-4) * splatAlpha`, which for a disc
   (uvScale 1, so `d` is 1 at the disc edge) reaches zero exactly at the rim. The `d > 1` discard at
   the top of `mainPS` clips the quad to the disc. The Gaussian arithmetic is untouched.
6. **Everything else** (sort, dither cross-fade, occlusion depth test, blend space, temporal filter)
   sees a quad per slot as before.

## Render path impact
- **mesh:** `mainMS` calls `SplatFromSlot` and `GaussianCornerVertex` unchanged; the disc logic is
  inside the shared builder. `mainPS` gains the disc branch.
- **VS SM6:** `mainVS` uses the same shared builder and corner function; identical behaviour.
- **VS SM5 (FXC):** the same source compiles as vs_5_1/ps_5_1. The new code uses only `mul`, `exp`,
  `saturate` and array indexing into a `float4[2]` constant, which the legacy compiler already
  handles for `g_LodRadius`. No new resources or samplers.

## Public / private placement
All changes are public app code in `src/SurfelsCore` and `docs/`. The codec library, the package
format and the bake are untouched; the baked per-level opacity in coarse records is consumed as is.
No public file gains a dependency on a stripped path.

## Decisions
- **Disc as a Gaussian-shaped quad** (clip centre plus clip half-axes) rather than routing level 1+
  through the disc modes' `BuildSplat`/`SplatData` path: keeps one quad structure and one sort for
  splat mode, needs no per-splat normal, and leaves the mesh and vertex corner code untouched.
  Rejected: reusing `BuildSplat`, which expects packed surfel records and size classes that splat
  records do not have.
- **Dedicated `g_SplatDiscRadius` array** rather than reusing `g_LodRadius`: the disc modes'
  Coverage slider and Auto Splat Size checkbox must not change splat-mode discs, and the
  requirement fixes the multiplier at 1.0. Eight floats appended at the end of the constant buffer
  on a 16-byte boundary, guarded by a `static_assert` on the offset and size.
- **Level passed into the builder** rather than a separate disc builder called from the mesh and
  vertex shaders: one call site change in `SplatFromSlot`, zero changes in `mainMS`/`mainVS`.
- **Fallback to Gaussian when a level has no spacing** (radius 0): nothing can vanish if the
  measurement is missing; criterion 6 makes a missing measurement visible in the trace.
- **`SolidDot` reused verbatim** so far levels become opaque one-pixel dots as the disc modes do.
- **Bounded Gaussian falloff and 1.5 coverage for discs** (revised 2026-09-07): the disc-mode
  `exp(-2.5 d^2) x 0.9` rule left an eight-percent alpha ring at every disc edge and, at radius
  1.0, read as speckle next to level 0. Sharing the reference falloff removes the ring, matches the
  level-0 look, and needs no disc-specific alpha code beyond the solid-dot case.
- **Level-0 chunks mid cross-fade drawn as discs** (added 2026-09-07): on low bandwidth the
  chunk-by-chunk disc-to-Gaussian dither flickered. Disc-to-disc fades only change density; the
  switch to Gaussians happens once per chunk when its weight reaches 1. Rejected for now: a second
  timed disc-to-Gaussian fade inside the chunk, which needs both representations evaluated on one
  quad and a per-chunk timer; revisit if the per-chunk switch is visible.
- **Fixed threshold constant `kSplatDiscMinLevel = 1`** in the shader, no constant-buffer field: the
  requirement rules out a control, and a shader constant is the smallest change. (Was 2; lowered
  to 1 on 2026-09-07 because level 1 Gaussians flickered with glow against level 2 discs.)

## Test plan
- Existing test exes are unaffected (no codec, format or CPU-side geometry change); they must still
  build. TestGPUSort, TestOcclusionVolume (default package) and TestLoadPackage have pre-existing
  failures unrelated to this change; report them as such.
- `tests/stress_test.py` does not exist in this repo.
- App smoke (tester agent), bundled Cthulhu splat package, Viewer mode, temporal filter off:
  1. Preview level forced to 1, 2, then 3: screenshots show discs, no halo past the rim (criterion 3).
  2. Preview level 0: screenshot numerically identical to a build of the current tree
     without this change (criterion 4). The tester builds the baseline first from a stash or a
     temporary checkout, captures the two views, then builds with the change and compares.
  3. Level 1 view on `--render-path mesh`, `vs6`, `vs5`: numerically matching (criterion 5).
  4. Trace shows a non-zero ComputeLodSpacing value per level (criterion 6).
  5. Zoom out through the level 0 to 1 transition with dithering on: screenshot sequence shows the
     cross-fade, no popping (criterion 7).
  6. Main splat dispatch time at the level-1 view not above the baseline (criterion 8).
  7. Banner SPLAT line shows "levels 1+ as discs" (criterion 9).
  8. No bleed-mask residue: grep for `bleed_mask`, `BleedMask` in `src/`, `docs/USER_GUIDE.md`,
     `bin/ShaderLibDX` and `bin/config.json` after a run (criterion 2). Note `bin/ShaderLibDX` may
     still hold a stale `BleedMaskCS.hlsl` copied by an earlier build; the coder deletes it.
- No new test exe.

## Risks
- **Coverage at level 1 and up.** The disc alpha is the bounded Gaussian falloff times the
  record's opacity, and coarse records carry a baked opacity boost tuned for Gaussians. If discs
  still read thin against the background, the remedies are a larger `kSplatDiscCoverage` or an
  opacity floor for discs, both one-line changes.
- **Per-chunk switch to Gaussians** at the end of a level-0 fade is a discrete change; with the
  shared falloff it is small, but on very low bandwidth it may read as a slow trickle of pops.
- **Look change across the level 0 to 1 cross-fade.** Gaussians and discs have different textures,
  so the dither fade will show a change in surface character. Accepted by the owner.
- **Spacing missing for a level** falls back to Gaussians for that level; the trace exposes it.
- **Constant-buffer layout.** Appending to `SurfelsCB` has bitten before (see the `lodRadiusAlign`
  comment). The `static_assert` and a first-frame visual check on all three paths guard it.

## Implementation brief (for the coder agent)
1. `src/SurfelsCore/SurfelsRenderer.h`: add `float splatDiscRadius[8] = {};` to `State` (comment: splat mode, camera-facing disc radius per level, world units, 0 = draw Gaussians). Append `float splatDiscRadius[8];` at the end of `SurfelsCB` with any padding needed so its offset is a multiple of 16, plus `static_assert(offsetof(SurfelsCB, splatDiscRadius) % 16 == 0)` and `static_assert(sizeof(SurfelsCB) % 16 == 0)`. Done when: the header compiles with both asserts passing.
2. `src/SurfelsCore/SurfelsRenderer.cpp` `OnRender`: after the existing `lodRadius` copy, copy `pState->splatDiscRadius` into `pCB->splatDiscRadius`. Done when: the constant buffer fill compiles and no other field moves (compare `offsetof` of the field that precedes the new one before and after).
3. `src/SurfelsCore/Shaders/Surfels.hlsl` constant buffer: append `float4 g_SplatDiscRadius[2];` mirroring the C++ order and padding. Add `static const uint kSplatDiscMinLevel = 1;` near `AutoSplatRadius`. Add `float disc; float solid;` to `GaussianSplat` and `float disc : TEXCOORD5;` to `VSOut`; set `disc = 0` in `CulledSplatVertex` and `SplatCornerVertex`. Done when: the file compiles on all three paths (start the app with `--render-path mesh`, `vs6`, `vs5`; no shader error trace lines).
4. `src/SurfelsCore/Shaders/Surfels.hlsl` `BuildGaussianSplat(uint splatIndex, uint lod, float blendWeight, out GaussianSplat gs)`: initialise `gs.disc = 0; gs.solid = 0;` with the other defaults. Keep the decode and the opacity check as they are. Compute `viewPos`, the behind-camera reject, and the clamped `clip` as today. Then: `bool wantDisc = (lod >= kSplatDiscMinLevel) || (abs(blendWeight) < 0.999); float discR = (wantDisc && lod < 8) ? g_SplatDiscRadius[lod >> 2][lod & 3] : 0.0;` If `discR > 0`: build `tX = g_CamRight * discR`, `tY = g_CamUp * discR`, call `SolidDot(length(pos - g_ViewerEyePos), tX, tY, solid)` (match the distance argument the disc modes pass), compute `axis1 = mul(g_ViewProj, float4(tX, 0.0)).xy`, `axis2 = mul(g_ViewProj, float4(tY, 0.0)).xy`, reject when `any(abs(clip.xy) - (abs(axis1) + abs(axis2)) > clip.w)`, evaluate the shared colour block, then set `gs.clipCenter = clip; gs.axis1 = axis1; gs.axis2 = axis2; gs.uvScale = 1.0; gs.color = color; gs.alpha = opacity; gs.disc = 1.0; gs.solid = solid; return true;`. Else: the existing quaternion, covariance, Jacobian, eigen, frustum, colour and shrink code unchanged, in its current order. The colour block must be a single shared piece of code (a small helper or a block above the branch) with its arithmetic order unchanged. Done when: the Gaussian path's shader output is unchanged (criterion 4 later) and the disc path compiles.
5. `src/SurfelsCore/Shaders/Surfels.hlsl` `SplatFromSlot`: read `chunk.lodLevel` into a local `uint lod` (0 when not chunked) and call `BuildGaussianSplat(info.x, lod, blendWeight, gs)`. `GaussianCornerVertex`: set `o.disc = gs.disc; o.solid = gs.solid;`. Done when: mesh and vertex splat paths compile without touching `mainMS`/`mainVS` bodies.
6. `src/SurfelsCore/Shaders/Surfels.hlsl` `mainPS`: inside `if (g_RenderMode == 3)`, before the Gaussian falloff, add the solid-dot early return from Data flow step 5; discs otherwise share the existing falloff. Done when: at a forced level 1 the model renders as soft discs with no halo and no rim rings (criteria 3 and 10) and level 0 is untouched.
7. `src/SurfelsCore/SurfelsApp.cpp`: in the per-frame state block next to the `lodRadius` fill, add `static const float kSplatDiscCoverage = 1.5f;` and `for (int l = 0; l < 8; l++) m_state.splatDiscRadius[l] = (l < (int)m_lodSpacing.size()) ? m_lodSpacing[l] * kSplatDiscCoverage : 0.0f;` with a comment. In the status banner, change the SPLAT `add(...)` line to append ", levels 1+ as discs". Done when: criterion 9 shows in the banner and the trace shows non-zero spacing per level (criterion 6).
8. `docs/USER_GUIDE.md`: one sentence in the splat-mode description stating that every level above 0 draws as camera-facing discs sized from each level's point spacing, so coarse levels have no Gaussian halo. Done when: present.
9. Housekeeping: delete a stale `bin/ShaderLibDX/BleedMaskCS.hlsl` if present; confirm `grep -r bleed_mask src docs/USER_GUIDE.md` finds nothing (criterion 2). Done when: both hold.
10. Build `Surfels_DX12`, `SplatLab` and the test targets in Release; run the app on all three render paths against the bundled Cthulhu package and check the trace for shader errors; run the test exes. Do NOT commit; report `git status --short` and `git diff --stat`.
