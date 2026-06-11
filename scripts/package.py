#!/usr/bin/env python3
import shutil
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
PKG = BUILD / 'package' / 'cinder-nozzle'

if PKG.exists():
    shutil.rmtree(PKG)
for rel in ['include', 'src', 'proj', 'samples']:
    shutil.copytree(ROOT / rel, PKG / rel)
for rel in ['README.md', 'LICENSE', 'cinderblock.xml']:
    shutil.copy2(ROOT / rel, PKG / rel)
short = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'], cwd=ROOT, text=True).strip()
zip_path = BUILD / f'cinder-nozzle-latest-{short}.zip'
if zip_path.exists():
    zip_path.unlink()
with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zf:
    for path in sorted(PKG.rglob('*')):
        if path.is_file():
            zf.write(path, path.relative_to(BUILD / 'package'))
print(f'wrote {zip_path}')
