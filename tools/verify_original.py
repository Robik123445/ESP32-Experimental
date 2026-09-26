#!/usr/bin/env python3
"""Read-only check of a SHA-256 manifest captured before copying the original."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('original', type=Path)
parser.add_argument('manifest', type=Path)
args = parser.parse_args()
expected = json.loads(args.manifest.read_text())
actual = {str(p.relative_to(args.original)): hashlib.sha256(p.read_bytes()).hexdigest()
          for p in args.original.rglob('*') if p.is_file()}
changed = sorted(k for k in expected.keys() | actual.keys() if expected.get(k) != actual.get(k))
if changed:
    print(json.dumps({'unchanged': False, 'changed_paths': changed}, indent=2))
    raise SystemExit(1)
print(json.dumps({'unchanged': True, 'files_including_git_metadata': len(actual)}, indent=2))
