# cinder-nozzle

Initial CinderBlock/addon implementation spike for nozzle diagnostics, Cinder `ci::gl::TextureRef`-shaped sender/receiver helpers, deterministic CPU pixel smoke scaffolding, CMake/package validation, and explicit unsupported runtime states.

This is not a Cinder runtime texture interop proof and it is not a zero-copy GPU support claim.

## Target

- Cinder release target: `v0.9.3`, annotated tag `221e15f04627ef5fb225a593cb0efa7be282d4f9`, tag target commit `70c2904643ac5978e439bd79ca64223169d366f6`.
- Toolchain target: C++17.
- nozzle submodule: `deps/nozzle` pinned to `a8efca3c847c39b76057a8e77f94b34146cc9125`.

## Implemented

- CinderBlock metadata: `cinderblock.xml`.
- Public headers under `include/cinder/nozzle/`.
- Sources under `src/cinder/nozzle/`.
- `CinderNozzleConfig.cmake` and root CMake host-independent test build.
- `sender::publish_texture(ci::gl::TextureRef, texture_format)` and receiver API shape.
- GL/context diagnostics that return deterministic `missing_host_smoke`/`unsupported` states instead of pretending CI proved runtime texture interop.
- Deterministic CPU RGBA pattern/oracle for `320x240` and `641x479`.
- Samples for sender, receiver, and diagnostics.
- Package-shape check, package consumer compile check, and zip output with exactly one top-level `cinder-nozzle/` folder.

## Not claimed

- No Windows fast GPU interop claim.
- No Linux GL support claim.
- No macOS Cinder runtime GL correctness claim yet.
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

## Evidence table

| Path | Status | Evidence boundary |
| --- | --- | --- |
| CinderBlock package and consumer build | PASS in CI | `tests/check_package.py` validates metadata, headers, sources, samples, CMake config, zip root, bundled nozzle headers, and an out-of-tree package consumer build |
| Host-independent C++ API build | PASS in CI | CMake builds static library and self-test on macOS/Linux/Windows |
| CPU pattern oracle 320x240 | PASS in CI | deterministic positive plus y-flip, R/B swap, alpha mutation, and byte-size negative probes |
| CPU pattern oracle 641x479 | PASS in CI | odd-size deterministic positive plus y-flip, R/B swap, alpha mutation, and byte-size negative probes |
| Cinder `ci::gl::TextureRef` runtime publish/copy | MISSING_HOST_SMOKE | needs real Cinder `RendererGl` app and nozzle oracle |
| macOS GL IOSurface/blit correctness | MISSING_HOST_SMOKE | no runtime Cinder smoke yet |
| Windows fast GPU interop | UNSUPPORTED | blocked until core #6-style path is implemented/verified |
| Linux GL interop | UNSUPPORTED | blocked until Linux GBM/EGL evidence exists |

## License

MIT
