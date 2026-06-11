#!/usr/bin/env python3
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PKG = ROOT / 'build/package/cinder-nozzle'
required = [
    PKG / 'cinderblock.xml',
    PKG / 'include/cinder/nozzle/CinderNozzle.h',
    PKG / 'include/cinder/nozzle/NozzleSender.h',
    PKG / 'include/cinder/nozzle/NozzleReceiver.h',
    PKG / 'include/cinder/nozzle/NozzleDiagnostics.h',
    PKG / 'deps/nozzle/include/nozzle/nozzle_c.h',
    PKG / 'src/cinder/nozzle/NozzleSender.cpp',
    PKG / 'src/cinder/nozzle/NozzleReceiver.cpp',
    PKG / 'proj/cmake/CinderNozzleConfig.cmake',
    PKG / 'samples/NozzleDiagnostics/src/NozzleDiagnosticsApp.cpp',
    PKG / 'samples/NozzleSenderBasic/src/NozzleSenderBasicApp.cpp',
    PKG / 'samples/NozzleReceiverBasic/src/NozzleReceiverBasicApp.cpp',
]
for path in required:
    if not path.exists():
        raise SystemExit(f'missing {path}')
root = ET.parse(PKG / 'cinderblock.xml').getroot()
for tag in ['name', 'id', 'version', 'sourcePattern', 'headerPattern', 'includePath']:
    if not root.findtext(tag):
        raise SystemExit(f'missing cinderblock.xml field {tag}')
if root.findtext('name') != 'cinder-nozzle':
    raise SystemExit('cinderblock name mismatch')
readme = (PKG / 'README.md').read_text(encoding='utf-8')
for phrase in ['No Windows fast GPU interop claim', 'No Linux GL support claim', 'CPU-copy receiver fallback', 'macos_iosurface_blit=FAIL', 'v0.9.3']:
    if phrase not in readme:
        raise SystemExit(f'README missing {phrase}')
zips = sorted((ROOT / 'build').glob('cinder-nozzle-latest-*.zip'))
if len(zips) != 1:
    raise SystemExit(f'expected one zip, found {zips}')
with zipfile.ZipFile(zips[0]) as zf:
    names = zf.namelist()
    roots = {name.split('/', 1)[0] for name in names if name}
    if roots != {'cinder-nozzle'}:
        raise SystemExit(f'wrong zip roots: {roots}')
    if any(name.startswith('cinder-nozzle-latest-') for name in names):
        raise SystemExit('zip contains extra wrapper directory')
    if 'cinder-nozzle/deps/nozzle/include/nozzle/nozzle_c.h' not in names:
        raise SystemExit('zip missing nozzle C API headers')

consumer = ROOT / 'build/package-consumer-check'
if consumer.exists():
    shutil.rmtree(consumer)
consumer.mkdir(parents=True)
(consumer / 'CMakeLists.txt').write_text(f'''
cmake_minimum_required(VERSION 3.20)
project(cinder_nozzle_package_consumer LANGUAGES CXX)
include("{(PKG / 'proj/cmake/CinderNozzleConfig.cmake').as_posix()}")
add_executable(package_consumer main.cpp)
target_link_libraries(package_consumer PRIVATE CinderNozzle)
''', encoding='utf-8')
(consumer / 'main.cpp').write_text('''
#include "cinder/nozzle/CinderNozzle.h"
int main() {
    auto diagnostics = cinder::nozzle::capture_diagnostics();
    return diagnostics.cinder_target == "v0.9.3" ? 0 : 1;
}
''', encoding='utf-8')
subprocess.run(['cmake', '-S', str(consumer), '-B', str(consumer / 'build'), '-DCMAKE_BUILD_TYPE=Release'], check=True)
subprocess.run(['cmake', '--build', str(consumer / 'build'), '--config', 'Release'], check=True)
print('cinder package shape and consumer build ok')
