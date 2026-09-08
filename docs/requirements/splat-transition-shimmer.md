# Splat transition shimmer

Status: agreed
Date: 2026-09-07
Owner: Dave Wilkinson

## Status, 2026-09-07 late: switched off
The Gaussian level 0 is set aside for now: every level of a splat package draws as discs with the
ordinary dithered transitions, and the hand-over below is kept behind `kSplatGaussianLevel0 =
false`. Refinement pacing (criteria 2, 3, 5, 9, 10) is unaffected; criteria 4, 6 and 7 do not
apply while it is off.

## Revision, 2026-09-07 evening
The level 0 hand-over is an opacity cross-fade of the whole level 0 at once, not a per-chunk
dither and not disc-to-Gaussian. Level 0 always draws Gaussians; the level 1 discs stay until
nothing on the path to level 0 in view is still streaming, then every level 1 chunk fades its
discs out with weight 1 - t while its level 0 children fade their Gaussians in with weight t, all
in lockstep over the Dither Duration and at full coverage, so the Gaussians' halos grow in
together rather than sweeping across the model, and no pixel loses coverage. The "Level 0 Gaussian fade-in" bullet and criterion 4
below are read accordingly: no chunk switches representation in a single frame because level 0
never changes representation. Criteria 6 and 7 (settled and start-up level 0 identical) still
hold because a weight of 1 skips
the ramp. Refinement pacing (criteria 2, 3, 5, 9, 10) is unchanged.

## Goal
Refilling an evicted model at high bandwidth (5G preset, camera close enough that level 0 is
selected) must show the same clean chunk-by-chunk dither at levels 1 and 0 that the coarse levels
show, and a level 0 chunk must ease from discs into Gaussians rather than snapping. Two causes are
addressed: the transition overload valve that snaps every cross-fade to completion once too many
chunks are mid-fade, and the single-frame switch from discs to Gaussians when a level 0 chunk
finishes its fade.

## User-visible behaviour
- **Refinement pacing (all render modes).** New cross-fades start at a rate that keeps the number
  of chunks mid-fade under a soft cap well below the overload valve, so the valve never fires
  during normal streaming. A refinement that has to wait keeps rendering the parent chunk, which is
  already on screen, and starts on a later frame. The streamed data keeps arriving meanwhile; only
  the visual hand-over is paced. Waiting refinements start in the order the stream delivered them,
  so the model grows back in the scheduler's order. The overload valve stays in place as a last
  resort.
- The Streaming tab's transition statistics gain one line: the number of refinement starts
  deferred this frame by the pacing.
- **Level 0 Gaussian fade-in (splat mode only).** When a level 0 chunk finishes its LOD cross-fade
  and becomes fully resident, its splats fade from discs to Gaussians through the same screen-space
  dither the LOD transitions use, over the Dither Duration already on the Streaming tab. Chunks
  resident at load (streaming simulation off, or the initial resident set) show Gaussians at once
  with no fade. A level 0 chunk that starts fading out again drops back to discs and will fade in
  again when it returns to full residency.
- Nothing else changes: level 0 fully resident renders as before, coarser levels render as discs,
  no new controls, no config keys, no format change.

## Non-goals
- Any change to the streaming order, priorities, bandwidth model or eviction rules beyond deferring
  when a cross-fade may begin.
- Removing the overload valve.
- Fading coarser levels between representations; they stay discs throughout.
- Changing the Gaussian appearance of a fully resident level 0 chunk.

## Constraints
- Render paths: mesh shaders, vertex-shader SM6 and vertex-shader SM5 must produce the same image.
  The mesh shader's per-vertex output grows by a few floats and must stay within its limits.
- Modes (Studio / Viewer): identical behaviour; the pacing applies to disc modes as well.
- Package compatibility: none affected.
- Performance: the GPU keeps both parent and children alive for every mid-fade chunk, so the soft
  cap must keep that population bounded; the 60 second repro below must run without a device
  removal on the development machine. The fade-in adds at most two interpolants and one extra
  falloff evaluation per pixel for level 0 chunks that are mid fade-in only.
- The reference Gaussian math is untouched: a mid-fade pixel that the dither assigns to the
  Gaussian layer has exactly the alpha it would have without the fade.

## Public / private placement
Entirely public app code in `src/SurfelsCore` (streaming traversal, chunk state, shader). Nothing in
`libs/bluesec-codec` changes. The streaming order and delivery model are not touched.

## Acceptance criteria
1. `Surfels_DX12`, `SplatLab` and the test executables build in Release for all three render paths.
2. Repro: Viewer, bundled Cthulhu splat package, streaming simulation on with the 5G preset,
   dithered transitions on, camera close enough that level 0 is selected, then evict residency.
   During the refill the trace's activeTransitions value never exceeds the soft cap, and the
   deferred-starts line on the Streaming tab is non-zero at some point during the lowest two levels.
3. In a frame sequence of that refill, levels 1 and 0 refine by the same dithered cross-fade as the
   coarse levels; no region snaps from one level to the next in a single frame.
4. In the same sequence, level 0 chunks ease from discs to Gaussians over the Dither Duration; no
   chunk switches representation in a single frame.
5. Time from eviction to full residency in the repro is at most 25% longer than in the current
   build at the same settings (read from the Streaming tab or the trace).
6. After the refill settles, the level 0 view is pixel-identical to the current build at the same
   camera with the streaming simulation off.
7. Streaming simulation off: at start-up level 0 shows Gaussians immediately, pixel-identical to
   the current build; no disc fade-in.
8. Criterion 3 and 4 hold on `--render-path mesh`, `vs6` and `vs5`, and a level 1 view and a settled
   level 0 view match numerically across the three paths.
9. The 60 second repro runs on the mesh path with the D3D12 debug layer clean and no device removal.
10. Disc modes (a surfel package, streaming simulation on, 5G, evict): the refill shows no snapped
    transitions either.

## Open questions
- None. The soft cap value and the fade duration source (the existing Dither Duration slider) are
  design decisions recorded in the design document.
