#!/usr/bin/env python3
import hashlib
import os
import platform
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
TOOLS = BUILD / 'tools'
CINDER_VERSION = 'v0.9.3'
CINDER_TAG_SHA = '221e15f04627ef5fb225a593cb0efa7be282d4f9'
CINDER_TARGET_SHA = '70c2904643ac5978e439bd79ca64223169d366f6'
CINDER_ZIP_NAME = 'cinder_0.9.3_mac.zip'
CINDER_URL = f'https://github.com/cinder/Cinder/releases/download/{CINDER_VERSION}/{CINDER_ZIP_NAME}'
CINDER_SHA256 = 'bccb59585f22f28b300c07962e06f9c73abf7ea78839cb202821b10bc34a184c'
CINDER_DIR = TOOLS / 'cinder_0.9.3_mac'

NOZZLE_SOURCES = [
    'src/cinder/nozzle/NozzleDiagnostics.cpp',
    'src/cinder/nozzle/NozzleDiscovery.cpp',
    'src/cinder/nozzle/NozzleReceiver.cpp',
    'src/cinder/nozzle/NozzleSender.cpp',
    'src/cinder/nozzle/PixelPattern.cpp',
    'src/cinder/nozzle/Status.cpp',
]


def run(cmd, cwd=None, env=None):
    print('+ ' + ' '.join(str(part) for part in cmd), flush=True)
    subprocess.run(cmd, cwd=cwd, env=env, check=True)


def run_capture(cmd, cwd=None, env=None):
    print('+ ' + ' '.join(str(part) for part in cmd), flush=True)
    completed = subprocess.run(cmd, cwd=cwd, env=env, text=True, capture_output=True, check=False)
    if completed.stdout:
        print(completed.stdout, end='')
    if completed.stderr:
        print(completed.stderr, end='', file=sys.stderr)
    if completed.returncode != 0:
        raise subprocess.CalledProcessError(completed.returncode, cmd, completed.stdout, completed.stderr)
    return completed.stdout + completed.stderr


def require_marker(output, marker):
    if marker not in output:
        raise SystemExit(f'missing runtime smoke marker: {marker}')


def sha256(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def ensure_cinder():
    TOOLS.mkdir(parents=True, exist_ok=True)
    zip_path = TOOLS / CINDER_ZIP_NAME
    if not zip_path.exists() or sha256(zip_path) != CINDER_SHA256:
        print(f'downloading {CINDER_URL}', flush=True)
        urllib.request.urlretrieve(CINDER_URL, zip_path)
    digest = sha256(zip_path)
    if digest != CINDER_SHA256:
        raise SystemExit(f'Cinder zip sha256 mismatch: {digest}')
    if not CINDER_DIR.exists():
        extract_root = TOOLS / 'cinder-extract'
        if extract_root.exists():
            shutil.rmtree(extract_root)
        extract_root.mkdir(parents=True)
        with zipfile.ZipFile(zip_path) as zf:
            zf.extractall(extract_root)
        extracted = extract_root / 'cinder_0.9.3_mac'
        if not extracted.exists():
            raise SystemExit('Cinder zip did not contain cinder_0.9.3_mac')
        if CINDER_DIR.exists():
            shutil.rmtree(CINDER_DIR)
        shutil.move(str(extracted), str(CINDER_DIR))
        shutil.rmtree(extract_root)
    config_path = CINDER_DIR / 'lib/macosx/Release/cinderConfig.cmake'
    if not config_path.exists():
        configure_build = CINDER_DIR / 'build'
        run(['cmake', '-S', str(CINDER_DIR), '-B', str(configure_build), '-DCMAKE_BUILD_TYPE=Release'])
    print(f'CINDER_NOZZLE_CINDER_BASELINE version={CINDER_VERSION} tag={CINDER_TAG_SHA} target={CINDER_TARGET_SHA} path={CINDER_DIR}', flush=True)


def main():
    if platform.system() != 'Darwin':
        raise SystemExit('run_cinder_runtime_smoke.py requires macOS because Cinder v0.9.3 publishes a mac runtime zip but no Linux runtime zip')
    ensure_cinder()
    smoke_dir = BUILD / 'cinder-runtime-smoke'
    build_dir = BUILD / 'cinder-runtime-smoke-build'
    if smoke_dir.exists():
        shutil.rmtree(smoke_dir)
    if build_dir.exists():
        shutil.rmtree(build_dir)
    smoke_dir.mkdir(parents=True)
    source_lines = [f'    "{(ROOT / "tests/runtime_smoke_app.cpp").as_posix()}"']
    source_lines.extend(f'    "{(ROOT / rel).as_posix()}"' for rel in NOZZLE_SOURCES)
    (smoke_dir / 'CMakeLists.txt').write_text(f'''
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(CinderNozzleRuntimeSmoke LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include("{(CINDER_DIR / 'proj/cmake/modules/cinderMakeApp.cmake').as_posix()}")
set(NOZZLE_INSTALL OFF CACHE BOOL "" FORCE)
set(NOZZLE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NOZZLE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("{(ROOT / 'deps/nozzle').as_posix()}" "${{CMAKE_BINARY_DIR}}/nozzle-build")

ci_make_app(
    APP_NAME CinderNozzleRuntimeSmoke
    SOURCES
{os.linesep.join(source_lines)}
    INCLUDES
        "{(ROOT / 'include').as_posix()}"
        "{(ROOT / 'deps/nozzle/include').as_posix()}"
    CINDER_PATH "{CINDER_DIR.as_posix()}"
)
target_link_libraries(CinderNozzleRuntimeSmoke PRIVATE nozzle)
''', encoding='utf-8')
    env = os.environ.copy()
    env.setdefault('CMAKE_BUILD_PARALLEL_LEVEL', str(os.cpu_count() or 2))
    run(['cmake', '-S', str(smoke_dir), '-B', str(build_dir), '-DCMAKE_BUILD_TYPE=Release', '-DCINDER_APP_LINK_RELEASE=ON'], env=env)
    run(['cmake', '--build', str(build_dir), '--config', 'Release'], env=env)
    candidates = sorted(build_dir.rglob('CinderNozzleRuntimeSmoke.app/Contents/MacOS/CinderNozzleRuntimeSmoke'))
    if not candidates:
        raise SystemExit('built app executable not found')
    output = run_capture([str(candidates[0])], env=env)
    for marker in [
        'CINDER_NOZZLE_GL_CONTEXT current=PASS',
        'CINDER_NOZZLE_TEXTURE_INTEROP size=320x240 texture_sender=PASS texture_receiver=PASS texture_transfer=PASS macos_iosurface_blit=FAIL copy_cost=cpu-copy',
        'CINDER_NOZZLE_TEXTURE_INTEROP size=641x479 texture_sender=PASS texture_receiver=PASS texture_transfer=PASS macos_iosurface_blit=FAIL copy_cost=cpu-copy',
        'CINDER_NOZZLE_TEXTURE_TRANSFER texture_sender=PASS texture_receiver=PASS texture_transfer=PASS macos_iosurface_blit=FAIL copy_cost=cpu-copy',
        'CINDER_NOZZLE_FRAME_INTEROP size=320x240 frame_sender=PASS frame_receiver=PASS',
        'CINDER_NOZZLE_FRAME_INTEROP size=641x479 frame_sender=PASS frame_receiver=PASS',
        'CINDER_NOZZLE_RUNTIME_SMOKE PASS',
    ]:
        require_marker(output, marker)


if __name__ == '__main__':
    main()
