# Surfels

A minimal DirectX 12 sample built on AMD's [Cauldron](https://github.com/GPUOpen-LibrariesAndSDKs/Cauldron)
framework, vendored as a git submodule under `libs/cauldron`. It's a bare Cauldron
bootstrap intended as the starting point for a surfel-based GI renderer, not a
finished one: it draws a procedural, GPU-instanced point-splat cloud (billboard
quads placed on a Fibonacci sphere) with an ImGui control panel, and nothing else
yet — no scene geometry, no lighting/GI pass, no depth buffer.

Cauldron itself ships no bundled sample apps (that changed at some point after
`glTFSample`/`FidelityFX-CAS` were built against it); this project's `src/DX12/`
was written directly against Cauldron's current `FrameworkWindows` API by reading
its header/source (device + swapchain are now owned by the framework base class,
`OnCreate()` takes no window handle, etc.) rather than copied from an existing
sample.

## What's here

- `src/DX12/SurfelsSample.{h,cpp}` — the app shell: window/input handling,
  ImGui panel, orbit camera, `WinMain`. Subclasses Cauldron's `FrameworkWindows`.
- `src/DX12/SurfelsRenderer.{h,cpp}` — the GPU side: descriptor heaps, upload
  heap, constant buffer ring, command list ring, root signature/PSO, and the
  per-frame render loop (clear backbuffer → draw instanced splats → ImGui → present).
- `src/DX12/Shaders/Surfels.hlsl` — vertex/pixel shaders for the splats. No
  vertex or index buffers: each quad corner comes from `SV_VertexID` and each
  surfel's position from `SV_InstanceID` via a Fibonacci-sphere formula.
- `libs/cauldron` — the Cauldron framework, as a git submodule.

## Building

Prerequisites (per [Cauldron's own README](https://github.com/GPUOpen-LibrariesAndSDKs/Cauldron)):

- CMake 3.24+
- Visual Studio 2019 or newer, with the Windows 10 SDK
- No Vulkan SDK needed — the root `CMakeLists.txt` forces `GFX_API=DX12`

```bat
git clone --recurse-submodules <this repo>
cd surfels
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
```

Open `build/Surfels_DX12.sln` in Visual Studio and build/run, or build from the
command line with `cmake --build . --config Debug`. The executable and its
shaders land in `bin/` (`bin/ShaderLibDX/Surfels.hlsl` gets copied there by the
CMake build so `CompileShaderFromFile` can find it at runtime).

If you already cloned without `--recurse-submodules`, run
`git submodule update --init --recursive` first.

## Known gaps / not yet verified

This was written by reading Cauldron's headers and source directly (there was
no local Windows/MSVC/CMake/DX12 toolchain available to actually build it), so
treat the first build as the real test:

- If you hit a build error, it's most likely a small API mismatch against the
  exact Cauldron commit you land on after `git submodule update` — Cauldron's
  `FrameworkWindows`/`Device`/`SwapChain` API has changed shape across its
  history, and this code targets whatever `HEAD` looked like when this was
  written.
- No depth buffer / depth test — overlapping splats just draw in instance
  order, not sorted. Fine for a placeholder, not for real surfel rendering.
- No Agility SDK opt-in (see the comment in `SurfelsSample.cpp`) — uses
  whatever D3D12 runtime Windows provides.

## Where this goes next

The obvious next steps toward an actual surfel GI renderer: replace the
procedural Fibonacci-sphere placement with surfels seeded from real scene
geometry (e.g. loaded via Cauldron's glTF loader), add a depth buffer, and
add an irradiance-accumulation/shading pass instead of the flat hash-color
shading the splats currently get.
