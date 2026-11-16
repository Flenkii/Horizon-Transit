#!/usr/bin/env python3
"""Baut die Mod und testet die Logik.

  python build.py        -> Logik-Tests (gcc oder zig) und out/TransportSim.dll
Braucht: pip install ziglang==0.15.2   (ein C-Compiler fuer Windows, ohne Visual Studio)
"""
import os, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.abspath(__file__))
CORE = ["src/core/jobs.c", "src/core/stops_data.c", "src/core/cars_data.c", "src/core/lines.c", "src/core/phone_draw.c", "src/core/people.c", "src/core/career.c", "src/core/wishes.c", "src/core/timetable.c", "src/core/bus_sql.c", "src/core/shop.c"]


def run(cmd):
    print(">", " ".join(cmd))
    subprocess.check_call(cmd, cwd=ROOT)


def main():
    zig = [sys.executable, "-m", "ziglang", "cc"]
    os.makedirs(os.path.join(ROOT, "out"), exist_ok=True)
    # 1. Logik-Tests auf diesem PC
    cc = ["gcc"] if shutil.which("gcc") else zig
    exe = os.path.join(tempfile.gettempdir(), "ts_test_core.exe" if os.name == "nt" else "ts_test_core")
    run(cc + ["-std=c11", "-O1", "-Wall", "-Wextra", "-Isrc/core", "tests/test_core.c"] + CORE + ["-lm", "-o", exe])
    run([exe])
    # 2. Die DLL fuer das Spiel (Windows x64)
    run(zig + ["-target", "x86_64-windows-gnu", "-shared", "-O2", "-std=c11", "-Wall", "-Wextra", "-Isrc/core", "-Isrc",
               "src/mod.c", "src/phone_window.c"] + CORE + ["-luser32", "-lgdi32", "-o", "out/TransportSim.dll"])
    for junk in ("out/mod.lib", "out/TransportSim.lib"):
        if os.path.exists(os.path.join(ROOT, junk)):
            os.remove(os.path.join(ROOT, junk))
    print("fertig: out/TransportSim.dll")


if __name__ == "__main__":
    main()
