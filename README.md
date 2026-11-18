# cinder-nozzle

CinderBlock for publishing and receiving nozzle textures with Cinder `ci::gl::TextureRef`. The macOS runtime smoke publishes a bottom-up RGBA8 texture, receives it through the IOSurface GL blit, and compares the entire RGBA buffer at two sizes.

## Target

- Cinder release target: `v0.9.3`, annotated tag `221e15f04627ef5fb225a593cb0efa7be282d4f9`, tag target commit `70c2904643ac5978e439bd79ca64223169d366f6`.
- Toolchain target: C++17.
- nozzle submodule: the gitlink at `deps/nozzle` records the core revision used by CI.

## Implemented

- CinderBlock metadata: `cinderblock.xml`.
- Public headers under `include/cinder/nozzle/`.
- Sources under `src/cinder/nozzle/`.
- `CinderNozzleConfig.cmake` and root CMake host-independent test build.
- `sender::publish_texture(ci::gl::TextureRef, texture_format)` publishes through nozzle GL interop when Cinder GL is available.
- `receiver::try_update_texture(ci::gl::TextureRef)` first attempts nozzle GL copy-to-texture and falls back to explicit CPU copy-out plus Cinder texture update when GL copy fails.
- GL/context diagnostics that return deterministic `missing_host_smoke`/`unsupported` states instead of pretending CI proved runtime texture interop.
- Deterministic CPU RGBA pattern/oracle for `320x240` and `641x479`.
- Samples for sender, receiver, and diagnostics.
- Package-shape check, package consumer compile check, and zip output with exactly one top-level `cinder-nozzle/` folder.
- The macOS smoke runs a Cinder 0.9.3 `RendererGl` app. Both texture sizes must report `macos_iosurface_blit=PASS copy_cost=gpu-copy` and match the full-buffer oracle. The app separately checks raw nozzle CPU frames.

## Not claimed

- No Windows fast GPU interop claim.
- No Linux GL support claim.
- Top-down sender textures are rejected until nozzle exposes an origin-aware GL publish C API.
- The GPU path copies texture data. The smoke checks pixel correctness and does not measure transfer latency or bandwidth.
- No release artifact publication.
- No Processing scope.

## Build

```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python3 scripts/package.py
python3 tests/check_package.py
```

macOS-only runtime smoke:

```bash
python3 scripts/run_cinder_runtime_smoke.py
```

The runtime smoke downloads the official `cinder_0.9.3_mac.zip` release asset
and verifies its SHA-256 before building the temporary Cinder app.

## Evidence table

| Path | Status | Evidence boundary |
| --- | --- | --- |
| CinderBlock package and consumer build | PASS in CI | `tests/check_package.py` validates metadata, headers, sources, samples, CMake config, zip root, bundled nozzle headers, and an out-of-tree package consumer build |
| Host-independent C++ API build | PASS in CI | CMake builds static library and self-test on macOS/Linux/Windows |
| CPU pattern oracle 320x240 | PASS in CI | deterministic positive plus y-flip, R/B swap, alpha mutation, and byte-size negative probes |
| CPU pattern oracle 641x479 | PASS in CI | odd-size deterministic positive plus y-flip, R/B swap, alpha mutation, and byte-size negative probes |
| Cinder `RendererGl` app startup/context | PASS on macOS CI | real Cinder app starts, creates a current GL context, runs one draw, and exits deterministically |
| CPU pattern oracle inside Cinder app 320x240 | PASS on macOS CI | deterministic positive plus y-flip, R/B swap, alpha mutation, and byte-size negative probes inside the runtime app |
| CPU pattern oracle inside Cinder app 641x479 | PASS on macOS CI | odd-size deterministic positive plus y-flip, R/B swap, alpha mutation, and byte-size negative probes inside the runtime app |
| Runtime-generated RGBA CPU pixels -> nozzle writable frame -> independent receiver 320x240 | PASS on macOS CI | `CINDER_NOZZLE_FRAME_INTEROP size=320x240 frame_sender=PASS frame_receiver=PASS ... short_buffer=PASS copy_cost=cpu-copy`; RGBA/BGRA storage is normalized back to RGBA before the probe-based oracle checks y-flip/R/B/alpha |
| Runtime-generated RGBA CPU pixels -> nozzle writable frame -> independent receiver 641x479 | PASS on macOS CI | `CINDER_NOZZLE_FRAME_INTEROP size=641x479 frame_sender=PASS frame_receiver=PASS ... short_buffer=PASS copy_cost=cpu-copy`; odd-size path uses the same probe-based deterministic oracle |
| Cinder `ci::gl::TextureRef` runtime publish/copy 320x240 | PASS on macOS CI | `CINDER_NOZZLE_TEXTURE_INTEROP size=320x240 texture_sender=PASS texture_receiver=PASS texture_transfer=PASS macos_iosurface_blit=PASS copy_cost=gpu-copy`; every received RGBA byte matches the published pattern |
| Cinder `ci::gl::TextureRef` runtime publish/copy 641x479 | PASS on macOS CI | `CINDER_NOZZLE_TEXTURE_INTEROP size=641x479 texture_sender=PASS texture_receiver=PASS texture_transfer=PASS macos_iosurface_blit=PASS copy_cost=gpu-copy`; the same full-buffer comparison covers the odd dimensions |
| CPU-copy receiver fallback | Implemented; unexercised by this smoke | If the GL copy fails, the receiver copies frame bytes to the Cinder texture. The smoke requires GL success. The separate CPU frame checks exercise the raw nozzle API. |
| Windows fast GPU interop | UNSUPPORTED | blocked until core #6-style path is implemented/verified |
| Linux GL interop | UNSUPPORTED | blocked until Linux GBM/EGL evidence exists |

## License

MIT
