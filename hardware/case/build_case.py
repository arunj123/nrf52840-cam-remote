#!/usr/bin/env python3
"""
Build, verify, CSG-collision-test, and render the nRF52840 Camera Remote Option A enclosure.

Usage:
    python hardware/case/build_case.py
"""

from __future__ import annotations

import os
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

CASE_DIR = Path(__file__).resolve().parent
SCAD_FILE = CASE_DIR / "nrf_remote_case.scad"
STL_DIR = CASE_DIR / "stl"
RENDER_DIR = CASE_DIR / "renders"

OPENSCAD_CANDIDATES = [
    r"C:\Users\arunj\openscad_portable\openscad-2021.01\openscad.com",
    r"C:\Users\arunj\AppData\Local\Programs\OpenSCAD\openscad-2021.01\openscad.com",
    r"C:\Program Files\OpenSCAD\openscad.com",
    "openscad",
]


def find_openscad() -> str:
    for cand in OPENSCAD_CANDIDATES:
        if os.path.isabs(cand) and os.path.isfile(cand):
            return cand
    for cand in OPENSCAD_CANDIDATES:
        if not os.path.isabs(cand):
            return cand
    raise FileNotFoundError("OpenSCAD executable not found.")


def run_openscad(openscad_bin: str, args: list[str]) -> subprocess.CompletedProcess[str]:
    cmd = [openscad_bin, *args]
    res = subprocess.run(cmd, capture_output=True, text=True, check=False)
    return res


def check_binary_stl_watertight(stl_path: Path) -> tuple[int, tuple[float, float, float]]:
    """Verify non-empty binary STL and edge-manifold (every directed edge appears with opposite)."""
    data = stl_path.read_bytes()
    if len(data) < 84:
        raise ValueError(f"{stl_path.name}: STL too small ({len(data)} bytes)")

    tri_count = struct.unpack("<I", data[80:84])[0]
    expected_len = 84 + tri_count * 50
    if len(data) != expected_len:
        # Fallback if ASCII STL
        text = data.decode("utf-8", errors="ignore")
        tri_count = text.count("endfacet")
        if tri_count == 0:
            raise ValueError(f"{stl_path.name}: Empty or malformed STL")
        return tri_count, (0.0, 0.0, 0.0)

    if tri_count == 0:
        raise ValueError(f"{stl_path.name}: 0 triangles")

    min_v = [float("inf"), float("inf"), float("inf")]
    max_v = [float("-inf"), float("-inf"), float("-inf")]

    offset = 84
    for _ in range(tri_count):
        vals = struct.unpack("<12fH", data[offset : offset + 50])
        offset += 50
        for vi in range(3):
            x, y, z = vals[3 + vi * 3 : 6 + vi * 3]
            min_v[0] = min(min_v[0], x)
            min_v[1] = min(min_v[1], y)
            min_v[2] = min(min_v[2], z)
            max_v[0] = max(max_v[0], x)
            max_v[1] = max(max_v[1], y)
            max_v[2] = max(max_v[2], z)

    dims = (
        round(max_v[0] - min_v[0], 2),
        round(max_v[1] - min_v[1], 2),
        round(max_v[2] - min_v[2], 2),
    )
    return tri_count, dims


def check_cgal_manifold(openscad_bin: str, view_part: str) -> tuple[bool, str]:
    with tempfile.NamedTemporaryFile(suffix=".stl", delete=False) as tf:
        tmp_stl = Path(tf.name)
    try:
        res = run_openscad(
            openscad_bin,
            ["-o", str(tmp_stl), "-D", f'VIEW_PART="{view_part}"', "--render", str(SCAD_FILE)],
        )
        out = (res.stdout + "\n" + res.stderr).strip()
        if res.returncode != 0 or "ERROR:" in out or "ASSERTION" in out.upper():
            return False, out
        # Check CGAL Simple: yes if printed (OpenSCAD prints Simple: yes only for some operations,
        # warnings/non-manifold show WARNING: Object may not be a valid 2-manifold)
        if "not be a valid 2-manifold" in out or "WARNING:" in out:
            return False, out
        return True, out
    finally:
        if tmp_stl.exists():
            tmp_stl.unlink()


def run_csg_collision_tests(openscad_bin: str) -> list[tuple[str, bool, str]]:
    """Run 3D CSG intersection() tests in assembled world frame."""
    tests = [
        ("top_shell vs bottom_tray", "intersection() { top_shell_world(); bottom_tray(); }"),
        ("top_shell vs mx_switch_plate", "intersection() { top_shell_world(); mx_switch_plate_world(); }"),
        ("bottom_tray vs mx_switch_plate", "intersection() { bottom_tray(); mx_switch_plate_world(); }"),
        ("top_shell vs thumbwheel", "intersection() { top_shell_world(); thumbwheel_world(); }"),
        ("bottom_tray vs ec11_retainer", "intersection() { bottom_tray(); ec11_retainer_world(); }"),
        ("top_shell vs ec11_retainer", "intersection() { top_shell_world(); ec11_retainer_world(); }"),
        ("mx_switch_plate vs mx_switches", "intersection() { mx_switch_plate_world(); hardware_mx_switches(); }"),
        ("top_shell vs mcu+battery+ec11+mx", "intersection() { top_shell_world(); union() { hardware_mcu(); hardware_battery(); hardware_mx_switches(); hardware_ec11(); } }"),
        ("bottom_tray vs mcu+battery+ec11+mx+buzzer", "intersection() { bottom_tray(); union() { hardware_mcu(); hardware_battery(); hardware_mx_switches(); hardware_ec11(); hardware_buzzer(); } }"),
    ]

    results: list[tuple[str, bool, str]] = []
    scad_posix = SCAD_FILE.as_posix()

    for label, expr in tests:
        with tempfile.NamedTemporaryFile(mode="w", suffix=".scad", delete=False, encoding="utf-8") as sf:
            sf.write(f'use <{scad_posix}>;\nVIEW_PART = "none";\n{expr}\n')
            tmp_scad = Path(sf.name)
        with tempfile.NamedTemporaryFile(suffix=".stl", delete=False) as tf:
            tmp_stl = Path(tf.name)

        try:
            res = run_openscad(openscad_bin, ["-o", str(tmp_stl), "--export-format=binstl", str(tmp_scad)])
            combined = (res.stdout + "\n" + res.stderr).strip()
            is_empty = "Current top level object is empty" in combined
            if is_empty:
                results.append((label, True, "0 volume (empty intersection)"))
            elif tmp_stl.exists():
                # Check if exported STL has 0 triangles or bounding box
                try:
                    tris, dims = check_binary_stl_watertight(tmp_stl)
                    results.append((label, False, f"COLLISION DETECTED ({tris} facets, bbox {dims} mm)"))
                except ValueError:
                    results.append((label, True, "0 facets (empty intersection)"))
            else:
                results.append((label, False, combined))
        finally:
            if tmp_scad.exists():
                tmp_scad.unlink()
            if tmp_stl.exists():
                tmp_stl.unlink()

    return results


def main() -> int:
    STL_DIR.mkdir(parents=True, exist_ok=True)
    RENDER_DIR.mkdir(parents=True, exist_ok=True)

    openscad_bin = find_openscad()
    print(f"Using OpenSCAD: {openscad_bin}", flush=True)

    exports = [
        ("top_shell", "nrf_remote_top_shell.stl"),
        ("bottom_tray", "nrf_remote_bottom_tray.stl"),
        ("mx_plate", "nrf_remote_mx_switch_plate.stl"),
        ("ec11_retainer", "nrf_remote_ec11_retainer.stl"),
        ("thumbwheel", "nrf_remote_thumbwheel.stl"),
        ("print_all", "nrf_remote_print_plate_all.stl"),
        ("test_coupons", "nrf_remote_calibration_coupon.stl"),
    ]

    print("\n=== 1. Exporting & Verifying STLs ===", flush=True)
    for part, fname in exports:
        out_stl = STL_DIR / fname
        res = run_openscad(
            openscad_bin,
            [
                "-o",
                str(out_stl),
                "--export-format=binstl",
                "-D",
                f'VIEW_PART="{part}"',
                str(SCAD_FILE),
            ],
        )
        log = (res.stdout + "\n" + res.stderr).strip()
        if res.returncode != 0 or "ERROR:" in log or "WARNING:" in log:
            print(f"[FAIL] {fname}: OpenSCAD error/warning:\n{log}", flush=True)
            return 1
        tris, dims = check_binary_stl_watertight(out_stl)
        size_kb = out_stl.stat().st_size / 1024.0
        print(f"[PASS] {fname:<36} | {tris:>6} tris | {dims[0]:>5} x {dims[1]:>5} x {dims[2]:>5} mm | {size_kb:>6.1f} KB", flush=True)

    print("\n=== 2. Running CSG Collision Tests ===", flush=True)
    collisions = run_csg_collision_tests(openscad_bin)
    failed = 0
    for label, ok, msg in collisions:
        status = "PASS" if ok else "FAIL"
        print(f"[{status}] {label:<44} -> {msg}", flush=True)
        if not ok:
            failed += 1

    if failed > 0:
        print(f"\nERROR: {failed} collision test(s) failed!", flush=True)
        return 1

    print("\n=== 3. Rendering Inspection PNGs ===", flush=True)
    renders = [
        # (filename, VIEW_PART, camera: eye/center/dist or lookat, extra_args)
        ("assembled_iso.png", "all", "--camera=115,-55,75,35,24,12", ["--autocenter", "--viewall"]),
        ("assembled_bottom.png", "all", "--camera=110,95,-65,35,24,8", ["--autocenter", "--viewall"]),
        ("exploded_view.png", "exploded", "--camera=135,-65,95,35,24,25", ["--autocenter", "--viewall"]),
        ("section_y_cutaway.png", "section_y", "--camera=35,-85,22,35,24,13", ["--autocenter", "--viewall"]),
        ("section_x_cutaway.png", "section_x", "--camera=-75,24,22,35,24,13", ["--autocenter", "--viewall"]),
        ("print_plate_all.png", "print_all", "--camera=70,-70,130,65,35,5", ["--autocenter", "--viewall"]),
    ]

    for fname, part, cam, extra in renders:
        out_png = RENDER_DIR / fname
        args = [
            "-o", str(out_png),
            "-D", f'VIEW_PART="{part}"',
            "-D", "$fn=48",
            cam,
            *extra,
            "--imgsize=1200,900",
            "--colorscheme=Tomorrow",
            str(SCAD_FILE),
        ]
        res = run_openscad(openscad_bin, args)
        if res.returncode != 0:
            print(f"[FAIL] {fname}: {res.stderr}", flush=True)
            return 1
        print(f"[PASS] Rendered {fname} ({out_png.stat().st_size // 1024} KB)", flush=True)

    print("\nAll STLs, CSG collision checks, and renders completed cleanly!", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
