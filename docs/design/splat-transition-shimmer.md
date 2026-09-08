# Design: Splat transition shimmer

Requirements: docs/requirements/splat-transition-shimmer.md
Status: agreed
Date: 2026-09-07

## Status, 2026-09-07 late: switched off
The owner has set the Gaussian level 0 aside: every level of a splat package, level 0 included,
draws as discs (`kSplatDiscMinLevel = 0` in `Surfels.hlsl`) with the ordinary dithered level
transitions, and the level 1 to 0 opacity hand-over described in the revision below stays in the
code behind `kSplatGaussianLevel0 = false` in `SurfelsApp.cpp`. A 3DGS source still bakes as
Gaussians (the records carry the colours); only Match Level 0 Softness is greyed out. Part A
(refinement pacing) is unaffected.

## Revision, 2026-09-07 evening (supersedes Part B below)
The owner redirected the level 0 hand-over to an opacity cross-fade. Part B as designed (dual
quad, per-pixel disc-or-Gaussian dither, `discCC` and `fade` interpolants, float2 `uvScale`, the
separate fade clock) is withdrawn and is not in the code. What the code does:

- **Level 0 never draws discs.** `BuildGaussianSplat(splatIndex, lod, fade, wave, gs)` has no
  blend-weight parameter; the disc branch is taken only for `lod >= 1`. The Gaussian quad, reject
  test and falloff are the committed reference form.
- **Opacity cross-fade, level 1 to 0 (CASE 2 of the traversal, splat mode).** Instead of the
  Bayer `-t` / `+t`, the parent is appended with an opacity weight `1 - t` and the children with
  `t`, both at full coverage. The traversal hands the weight over in `m_traverseSplatFade`;
  `AppendChunkToRenderer` writes it to `MeshletChunkGPU::dilationMorph` in splat mode (1 for every
  chunk outside a hand-over, including all resident-at-load chunks). No per-chunk fade clock.
- **Shader.** In the disc branch, `discAlpha = max(opacity, floor)`, then `*= fade` when below
  0.999 (a fading sub-pixel disc drops its opaque-dot rule). In the Gaussian path, after the disc
  branch, `opacity *= fade` when below 0.999, with the 1/255 early-out re-applied; the
  alpha-dependent quad shrink then trims the quad with it. At fade 1 both paths are arithmetically
  unchanged.
- **All at once.** A level 1 to 0 hand-over may begin only when the previous frame's traversal
  found no node on the path to level 0 (target level 0, needing refinement) with children still
  missing (`m_splatLevel0Waiting == 0`, latched into `m_splatLevel0AllArrived`); until then the
  parent's discs render solid. So every ready group starts in the same frame and, sharing
  `progressStep`, advances in lockstep: the whole level 0 in view fades in as one. Those starts
  bypass the pacing budget, and because both layers are appended at full coverage they are not
  counted as active transitions, so the overload valve does not snap them. A trace line names the
  frame the wave begins. Stragglers whose children arrive later start on their own when nothing
  else is waiting. Trade: level 0 is not shown until it is complete in view.
- **Why this form.** The two weights sum to one at every pixel: no stipple, no loss of coverage,
  no see-through, and the Gaussians' halos grow in with `t`. The order dependence between the two
  layers is bounded by `t (1 - t)` of their colour difference, zero at both ends, instead of a
  near-opaque disc against Gaussians for a whole ramp (the forms that flickered). Demotions and
  coarser refinements keep the dithered fade; one timer (Dither Duration) governs all of them.
- **Withdrawn the same evening** (all flickered or flashed): dithered disc-to-Gaussian at full
  strength (halo flash); Gaussians ramping under solid discs dropped in one step; Gaussians
  ramping under dissolving discs; per-record disc-to-Gaussian blend after a disc-to-disc dither.
Part A (refinement pacing) stands as written, with the per-chunk budget noted in step 2, plus one
consequence fixed the same evening: the arrival glow (Refinement Visualizer) ran from delivery, so
a chunk whose start the pacing deferred for longer than the glow duration appeared with its glow
already expired (levels 1 and 0 never glowed at 5G). `StreamChunk::streamWavePending` is set at
delivery and the glow timer now starts on the chunk's first draw in `AppendChunkToRenderer`; the
edge-driven start cap's "just streamed" test counts a pending glow as well.

A second consequence fixed the same evening: the pacing admitted deferred starts in traversal
order (first fit in the tree walk), so whenever more groups waited than the budget admitted, the
model grew back in spatial index order rather than the scheduler's. Every delivery now carries
`StreamChunk::deliverySeq`; a group's rank is its children's latest delivery; the groups deferred
last frame are sorted by rank and the budget walked along them to set `m_refineAdmitSeqLimit`, and
a group may start only if its rank is within the walk (plus the per-frame budget as before). A
newly ready group waits one frame to enter the ranking; resident-at-load children rank first.

## Summary
Two changes, one per cause. First, the streaming traversal paces refinement starts: each frame it
allows at most a budget of new cross-fades, sized so the mid-fade population converges on a soft
cap of 6000 chunks and never reaches the 8000 overload valve; a node whose start is deferred keeps
rendering its parent and retries next frame. Second, in splat mode a level 0 chunk carries a
Gaussian fade-in weight that rises from 0 to 1 over the Dither Duration once its LOD cross-fade has
completed; while it is between 0 and 1 each splat is drawn on a quad enlarged to cover both its
Gaussian and its disc, and the pixel shader picks the Gaussian or the disc alpha per pixel with the
same Bayer dither the LOD fades use. The weight travels in the chunk record's `dilationMorph` field,
unused in splat mode. Fully resident level 0 chunks (weight 1) take the untouched Gaussian path.

## Components touched
| Area | Files | Change |
|------|-------|--------|
| Streaming pacing | `src/SurfelsCore/SurfelsApp.h`, `src/SurfelsCore/SurfelsApp.cpp` | Per-frame refinement start budget in `UpdateStreamingSimulation`, applied at the CASE 2 start; deferred count member, trace line, Streaming tab stat line. |
| Chunk fade state | `src/SurfelsCore/SurfelsApp.h`, `src/SurfelsCore/SurfelsApp.cpp` | `StreamChunk::gaussianFade`; reset on stream delivery; advanced or reset in `AppendChunkToRenderer`; written to `MeshletChunkGPU::dilationMorph` in splat mode. |
| Splat shaders | `src/SurfelsCore/Shaders/Surfels.hlsl` | `GaussianSplat` gains `float2 uvScale` (was scalar), `float2 discScale`, `float fade`; `VSOut` gains `float2 discCC`, `float fade`; `BuildGaussianSplat` gains the fade parameter and a dual branch; `SplatFromSlot` passes the chunk's fade; `GaussianCornerVertex` fills the new interpolants; `mainPS` gets the dual branch and a guarded top discard. |
| Docs | `docs/USER_GUIDE.md` | One sentence on the level 0 fade-in and one on refinement pacing. |

## Data flow
```mermaid
flowchart LR
  A[m_lastActiveTransitions] --> B{budget = min(cap x dt / duration,\ncap - active)}
  B --> C[CASE 2 start: within budget?]
  C -->|yes| D[transitionProgress advances,\ndithered fade]
  C -->|no| E[render parent solid,\ndeferred++ , retry next frame]
  F[stream delivery:\ngaussianFade = 0] --> G[AppendChunkToRenderer\nlevel 0, weight 1: fade += dt / duration]
  G --> H[chunkGpu.dilationMorph = fade]
  H --> I{BuildGaussianSplat}
  I -->|fade >= 1| J[Gaussian, unchanged]
  I -->|0 <= fade < 1| K[dual quad: Gaussian axes\nenlarged to cover disc]
  K --> L[mainPS: bayer < fade ? gaussianA : discA]
```

### Part A: refinement pacing
1. **Budget.** Before the traversal, with `cap = kTransitionSoftCap = 6000`, `duration =
   max(0.05, m_ditherTransitionDurationSec)` and `active = m_lastActiveTransitions`:
   `budget = min(cap * dt / duration, max(0, cap - active))`, rounded down, and unlimited when
   dithered transitions are off or streaming is paused (no fade means no double population). At 60
   fps and the default 0.75 s duration that is about 133 starts per frame, which converges the
   mid-fade population on the cap from below without bursts.
2. **Gate.** In CASE 2, where `transitionProgress <= 0` and the node is about to begin its fade
   (both stream-paced and edge-driven starts), consume `1 + childCount` units of budget, because
   the active-transition count includes the fading-out parent and every fading-in child (charging
   one unit per start let the population overshoot the cap by up to three times the frame budget
   in the first test run); if the budget is exhausted, render the parent solid and return, exactly
   as the existing edge-driven cap does. The budget and the tab line are therefore in chunks. The edge-driven
   sub-cap of 32 per frame stays as an additional limit for those starts. Nodes already mid-fade
   are never gated.
3. **Visibility.** Count deferred starts per frame into `m_refineStartsDeferred`, show it on the
   Streaming tab beside the active-transition count, and add one trace line per second while it is
   non-zero: `Refinement pacing: deferred N starts this frame (active M, budget B)`.
4. **Valve.** The 8000 overload valve is unchanged and should no longer fire in normal use.

### Part B: level 0 Gaussian fade-in
5. **State.** `StreamChunk::gaussianFade` defaults to 1 (resident at load shows Gaussians). The
   streaming delivery site that marks a chunk resident sets it to 0.
6. **Advance.** In `AppendChunkToRenderer`, splat mode, for a level 0 chunk: if
   `abs(blendWeight) < 0.999` set `gaussianFade = 0` (it is discs while fading either way);
   otherwise `gaussianFade = min(1, gaussianFade + dt / duration)`, or 1 immediately when dithered
   transitions are off. Write it to `chunkGpu.dilationMorph` in splat mode (other modes keep the
   existing silhouette value).
7. **Shader modes.** `SplatFromSlot` reads `chunk.dilationMorph` as `fade` (1 when not chunked)
   and passes it with the level and blend weight. `BuildGaussianSplat` decides: disc when
   `lod >= 1 || abs(blendWeight) < 0.999` (as today); else dual when `fade < 0.999`; else the
   Gaussian path unchanged with `fade = 1`.
8. **Dual quad.** Run the Gaussian path as today up to `v1`, `v2` (pixel half-axes) and `clipS`.
   Disc radius in pixels `discPx = discR * focal / -viewPos.z` with `discR = g_SplatDiscRadius[0]`
   and `focal = g_Viewport.x * g_Proj00` (already computed). Solid-dot rule: if `discPx < 1.5`,
   `discPx = max(discPx, 1)` and `solid = 1`. Enlarge so the quad covers the disc:
   `k1 = max(1, discPx / (l1 * clipS))`, `k2 = max(1, discPx / (l2 * clipS))`;
   `axis1 = v1 * c * clipS * k1`, `axis2 = v2 * c * clipS * k2`; `uvScale = clipS * float2(k1, k2)`;
   `discScale = float2(l1 * clipS * k1, l2 * clipS * k2) / discPx`; the frustum reject uses the
   enlarged extents. Colour and opacity as today; `fade` copied; `disc = 0`.
9. **Corner.** `GaussianCornerVertex`: `o.uv = (cc * gs.uvScale) * 0.5 + 0.5` with the float2
   scale (plain Gaussians and discs pass `(s, s)`), `o.discCC = cc * gs.discScale`, `o.fade =
   gs.fade`. Because every corner shares the centre's clip w, both interpolants are linear in screen
   space, so `dot(discCC, discCC)` in the pixel shader is the exact normalised disc distance.
10. **Pixel shader.** Top of `mainPS`: `bool dual = (g_RenderMode == 3 && i.disc < 0.5 && i.fade < 0.999);`
    and the existing `d > 1` discard runs only when `!dual`. The LOD dither block is unchanged (a
    chunk mid fade-in has blend weight 1, so the two dithers never overlap). In the splat block,
    before the solid-dot return: if `dual`, `gaussianA = (d <= 1) ? falloff(d) * splatAlpha : 0`,
    `dd = dot(i.discCC, i.discCC)`, `discA = (dd <= 1) ? ((i.solid > 0.5) ? 1 : falloff(dd) * splatAlpha) : 0`,
    `a = (GetBayer8x8(pixel) < i.fade) ? gaussianA : discA`, discard below 1/255, return
    `float4(color * a, a)`. `falloff(x) = (exp(-4 x) - e^-4) / (1 - e^-4)` is the existing formula,
    factored into a helper so the Gaussian path calls the same code it does today.

## Render path impact
- **mesh:** `mainMS` is untouched; the per-vertex payload grows by 12 bytes to about 96 bytes.
  With `SURFELS_PER_GROUP * 4` vertices per group that stays well inside the mesh shader output
  limits; the coder must confirm the group's vertex count times the payload against the 32 KB
  output budget and the app must start on the mesh path with no PSO error.
- **VS SM6:** `mainVS` untouched; same shared builder and corner function.
- **VS SM5 (FXC):** the dual branch uses `mul`, `exp`, `dot`, `max` and the existing Bayer lookup;
  nothing new for the legacy compiler. The float2 `uvScale` is a plain struct field change.

## Public / private placement
All public app code in `src/SurfelsCore` and `docs/`. The streaming order, priorities and delivery
model in the codec library are untouched; the pacing sits in the app's traversal, after delivery.

## Decisions
- **Pace starts rather than raise the valve**: the valve protects against a real GPU overload; the
  pacing keeps the population under it without changing what is delivered or in what order.
  Rejected: raising the threshold (still snaps, only later), and pacing delivery itself (it would
  slow the stream, whereas the visual hand-over is what needs bounding).
- **Budget from the fade duration** so the mid-fade population converges on the cap from below;
  a plain `cap - active` budget would admit thousands in one frame, then none for a fade length.
- **Soft cap 6000** leaves headroom under 8000 for the frame in which the count is measured (the
  count is one frame stale) and for demotions, which are not gated.
- **Fade weight in `dilationMorph`**: the field is a float in an 80-byte record shared by every
  path, already unused in splat mode (silhouette processing is off), so no layout change and no
  new buffer. The comment on the field records the dual use.
- **Dual quad from the Gaussian's own ellipse**, enlarged per axis to cover the disc, rather than a
  second quad per splat: the mesh and vertex shaders keep one quad per slot and the sort is
  unchanged. Per-axis `uvScale` keeps the Gaussian alpha at every pixel exactly the reference value.
- **Same Bayer, same convention** as the LOD fades (Gaussian visible where `bayer < fade`) so a
  pixel is composed from one representation at a time, never a blend of both.
- **Fade duration = Dither Duration slider**: one control governs every dissolve; no new control.
- **Chunks resident at load start with fade 1** so a package opened with the streaming simulation
  off shows Gaussians immediately and stays pixel-identical.

## Test plan
- Existing test exes unaffected; they must still build. The three known-broken ones remain so.
- App smoke (tester), Viewer, bundled Cthulhu, mesh path unless stated:
  1. Repro from criterion 2 with a frame sequence at 10+ fps and the trace: activeTransitions
     peak under 6000; deferred-starts line non-zero during levels 1 and 0; no snapped regions.
  2. Level 0 fade-in visible in the sequence; no single-frame representation switch.
  3. Refill time versus the current build within 25%.
  4. Settled level 0 view pixel-identical to the current build (streaming simulation off).
  5. Start-up with the streaming simulation off: Gaussians immediately, identical.
  6. Level 1 view and settled level 0 view on mesh, vs6, vs5: numerically matching.
  7. 60 s repro with the debug layer: clean, no device removal.
  8. Disc-mode package repro: no snaps.
- No new test exe.

## Risks
- **Pacing delays the visual refill** at very high bandwidth; the data is resident, only the
  hand-over waits. The 25% bound in criterion 5 is the check; the cap can be raised if it binds.
- **Deferred nodes' children could be evicted by decay while waiting**; the existing edge-driven
  cap already accepts this, and at 5G the wait is a fraction of a second.
- **Enlarged dual quads** raise fill cost for level 0 chunks mid fade-in, for one fade duration
  each. Bounded by the pacing.
- **Mesh shader output payload** must be re-checked after adding 12 bytes per vertex.
- **`dilationMorph` reuse** is a convention, not a type; a future re-enable of silhouette morphing
  in splat mode would need its own field.

## Implementation brief (for the coder agent)
1. `src/SurfelsCore/SurfelsApp.h`: add `float gaussianFade = 1.0f;` to `StreamChunk` (comment: splat mode, level 0, 0 = discs, 1 = Gaussians; reset to 0 on stream delivery, advances after the LOD fade completes); add `uint32_t m_refineStartsThisFrame = 0, m_refineStartBudget = 0, m_refineStartsDeferred = 0;` and `float m_refinePacingTraceTimer = 0.0f;`. Done when: it compiles.
2. `src/SurfelsCore/SurfelsApp.cpp` `UpdateStreamingSimulation`: next to the overload valve, compute the budget per Data flow step 1 into `m_refineStartBudget` (UINT32_MAX when dithering is off or streaming paused) and zero `m_refineStartsThisFrame` and `m_refineStartsDeferred`. In CASE 2 at the `transitionProgress <= 0.0f` start, before the existing edge-driven check: if `m_refineStartsThisFrame >= m_refineStartBudget` then `m_refineStartsDeferred++`, `AppendChunkToRenderer(&currentChunk, parentFactor, isSilhouette); return;` else `m_refineStartsThisFrame++`. Keep the edge-driven sub-cap after it. After the traversal, the once-per-second trace line from step 3. Done when: criterion 2's trace shows the cap held.
3. `src/SurfelsCore/SurfelsApp.cpp` Streaming tab: beside the active transitions statistic add "Refinement starts deferred: %u (budget %u)". Done when: visible in the tab.
4. `src/SurfelsCore/SurfelsApp.cpp`: at the streaming delivery site that sets `chunk.isResident = true` in the bandwidth delivery section, set `chunk.gaussianFade = 0.0f`. In `AppendChunkToRenderer`, when `m_splatMode && pChunk->lodLevel == 0`: apply Data flow step 6 and write `chunkGpu.dilationMorph = pChunk->gaussianFade`; other modes keep the existing assignment. Done when: the trace or a debug print shows a level 0 chunk's fade rising after its arrival, and chunks resident at load report 1.
5. `src/SurfelsCore/Shaders/Surfels.hlsl`: update the `MeshletChunk.dilationMorph` comment (splat mode: level 0 Gaussian fade-in weight); change `GaussianSplat.uvScale` to `float2`, add `float2 discScale; float fade;`; add `float2 discCC : TEXCOORD6; float fade : TEXCOORD7;` to `VSOut` and set them to 0 and 1 in `CulledSplatVertex` and `SplatCornerVertex`; factor the existing splat falloff into `float SplatFalloff(float x)` used by the Gaussian path unchanged. Done when: all three paths compile.
6. `src/SurfelsCore/Shaders/Surfels.hlsl` `BuildGaussianSplat(uint splatIndex, uint lod, float blendWeight, float fade, out GaussianSplat gs)`: defaults `uvScale = 0`, `discScale = 0`, `fade = 1`; disc branch sets `uvScale = float2(1, 1)`; Gaussian branch sets `uvScale = float2(clipS, clipS)`, and when `fade < 0.999` applies Data flow step 8 (dual) instead of the plain finalisation. `SplatFromSlot` reads `chunk.dilationMorph` into `fade` and passes it. `GaussianCornerVertex` per step 9. Done when: a level 0 chunk mid fade-in renders both layers under the dither (criterion 4) and a settled chunk is pixel-identical (criterion 6).
7. `src/SurfelsCore/Shaders/Surfels.hlsl` `mainPS`: per Data flow step 10. Done when: criteria 4, 6 and 7 hold.
8. Verify the mesh shader output payload: `sizeof(VSOut)` times `SURFELS_PER_GROUP * 4` under 32 KB; note the numbers in the report. Done when: the mesh path starts with no PSO error.
9. `docs/USER_GUIDE.md`: one sentence on refinement pacing under the Streaming tab and one on the level 0 fade-in in the Gaussian Splat Mode bullet. Done when: present.
10. Build `Surfels_DX12`, `SplatLab` and the test targets in Release; smoke all three render paths; run the test exes. Do NOT commit; report `git status --short` and `git diff --stat`.
