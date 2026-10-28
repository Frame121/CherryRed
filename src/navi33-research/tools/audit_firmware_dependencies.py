#!/usr/bin/env python3
"""Correlate AMDGPU source firmware declarations with local firmware files.

This is a static inventory tool, not proof a payload was loaded or suitable for
macOS. It never writes to the host firmware directory.
"""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

FIRMWARE_RE = re.compile(r'(?P<path>amdgpu/[A-Za-z0-9_.+-]+\.bin)')
DECLARATION_RE = re.compile(r'MODULE_FIRMWARE\s*\(\s*"(?P<path>amdgpu/[^" ]+\.bin)"\s*\)')


def scan(source_root: Path, firmware_root: Path, prefixes: tuple[str, ...] = ()) -> dict:
    if not source_root.is_dir():
        raise ValueError(f"source root does not exist: {source_root}")
    if not firmware_root.is_dir():
        raise ValueError(f"firmware root does not exist: {firmware_root}")
    rows: dict[str, dict] = {}
    for source in sorted(source_root.rglob('*')):
        if not source.is_file() or source.suffix not in {'.c', '.h'}:
            continue
        try:
            content = source.read_text(errors='replace')
        except OSError:
            continue
        for match in FIRMWARE_RE.finditer(content):
            rel = match.group('path')
            if prefixes and not any(Path(rel).name.startswith(prefix) for prefix in prefixes):
                continue
            entry = rows.setdefault(rel, {'path': rel, 'source_references': [], 'declared_module_firmware': False})
            source_rel = str(source.relative_to(source_root))
            if source_rel not in entry['source_references']:
                entry['source_references'].append(source_rel)
            if any(d.group('path') == rel for d in DECLARATION_RE.finditer(content)):
                entry['declared_module_firmware'] = True
    results = []
    for rel, entry in sorted(rows.items()):
        base = firmware_root / Path(rel).name
        candidates = [base, Path(str(base) + '.zst'), Path(str(base) + '.xz')]
        present = [str(p) for p in candidates if p.is_file()]
        results.append({**entry, 'local_payload_candidates': present,
                        'local_file_present': bool(present),
                        'evidence_limit': 'Static source reference and local file presence only; not proof requested, loaded, compatible with macOS, or legally redistributable.'})
    return {'schema_version': 1, 'source_root': str(source_root), 'firmware_root': str(firmware_root),
            'source_file_count': sum(1 for p in source_root.rglob('*') if p.is_file() and p.suffix in {'.c', '.h'}),
            'firmware_reference_count': len(results), 'entries': results}


def render_markdown(report: dict) -> str:
    lines = ['# Static AMDGPU firmware dependency correlation', '',
             f"- Source C/H files scanned: {report['source_file_count']}",
             f"- Unique firmware path references: {report['firmware_reference_count']}", '',
             '| Firmware path | `MODULE_FIRMWARE` declaration | Local payload candidate | Source references |',
             '|---|---|---|---|']
    for row in report['entries']:
        local = ', '.join(Path(p).name for p in row['local_payload_candidates']) or 'not found'
        refs = ', '.join(row['source_references'][:3])
        if len(row['source_references']) > 3:
            refs += f" (+{len(row['source_references']) - 3} more)"
        lines.append(f"| `{row['path']}` | {'yes' if row['declared_module_firmware'] else 'no / macro reference only'} | {local} | {refs} |")
    lines += ['', '## Interpretation limits', '',
              'This report correlates static source references with file names found locally. It does not prove the installed kernel binary was built from this source tree, a file was requested or loaded at runtime, the blob is compatible with macOS, or redistribution is permitted. Compressed `.zst` / `.xz` matches are filename matches only.', '']
    return '\n'.join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', required=True, type=Path)
    parser.add_argument('--firmware-root', required=True, type=Path)
    parser.add_argument('--prefix', action='append', default=[], help='Include only firmware basenames starting with this prefix; may be repeated.')
    parser.add_argument('--json-out', type=Path)
    parser.add_argument('--markdown-out', type=Path)
    args = parser.parse_args()
    try:
        report = scan(args.source_root, args.firmware_root, tuple(args.prefix))
    except ValueError as exc:
        parser.error(str(exc))
    payload = json.dumps(report, indent=2) + '\n'
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(payload)
    else:
        print(payload, end='')
    if args.markdown_out:
        args.markdown_out.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_out.write_text(render_markdown(report))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
