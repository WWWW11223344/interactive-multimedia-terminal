"""Check portable project boundaries and optional local build/source evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]


def verify(upstream=False, artifacts=False, deliverables=False):
    count = 0

    def check(condition, message):
        nonlocal count
        if not condition:
            raise RuntimeError(message)
        count += 1

    for name in ("src", "inc", "lib", "res"):
        check((ROOT / name).is_dir(), f"Missing layer: {name}")
    for name in ("CMakeLists.txt", "Makefile", "run.sh", "run-preview.cmd"):
        check((ROOT / name).is_file(), f"Missing build/launch file: {name}")
    for folder in ("src", "inc", "res", "lib"):
        for path in (ROOT / folder).rglob("*"):
            if path.suffix.lower() == ".c":
                check(folder == "src", f"C implementation outside src: {path}")
            if path.suffix.lower() == ".h":
                check(folder == "inc", f"Header outside inc: {path}")
    device_include = re.compile(r'#\s*include\s*[<"](?:windows\.h|linux/|sys/(?:mman|ioctl)\.h)')
    for path in (ROOT / "src").rglob("*.c"):
        if path.name not in ("platform.c", "main.c", "truetype.c"):
            check(not device_include.search(path.read_text(encoding="utf-8")),
                  f"Device dependency in application core: {path}")
    for path in (ROOT / "src/game.c", ROOT / "inc/game.h"):
        includes = re.findall(r'#\s*include\s*"([^"]+)"', path.read_text(encoding="utf-8"))
        check(all(item == "game.h" for item in includes), f"Rule depends on UI: {path}")
    for name in ("MusicSystem.c", "VideoSystem.c", "LoginAndRegisterSystem.c"):
        check(not (ROOT / "src" / name).exists(), f"Removed feature remains: {name}")
    check((ROOT / "res/simfang.ttf").stat().st_size > 10000, "Missing font")
    for name in ([f"Album/{n:02}.bmp" for n in range(1, 7)] +
                 [f"game/{n}.bmp" for n in range(12)]):
        path = ROOT / "res" / name
        check(path.is_file() and path.read_bytes()[:2] == b"BM", f"Missing BMP: {name}")
    if upstream:
        for name in ("upstream-files.json", "upstream-xiao.json"):
            data = json.loads((ROOT / "docs" / name).read_text(encoding="utf-8-sig"))
            base = Path(data.get("source", data.get("original_project", "")))
            for rel, expected in data["files"].items():
                path = base / rel
                check(path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == expected,
                      f"Upstream changed or unavailable: {path}")
            print(f"UPSTREAM PASS: {name}: {len(data['files'])} files unchanged")
    if artifacts:
        check((ROOT / "build-host/terminal.exe").is_file(), "Build desktop executable first")
        blob = (ROOT / "build-arm/terminal").read_bytes()
        check(blob[:7] == b"\x7fELF\x01\x01\x01", "Expected ELF32 little-endian")
        check(struct.unpack_from("<H", blob, 18)[0] == 40, "Expected EM_ARM")
        flags = struct.unpack_from("<I", blob, 36)[0]
        check(flags >> 24 == 5 and flags & 0x200 and not flags & 0x400,
              "Expected EABI5 soft-float")
        phoff = struct.unpack_from("<I", blob, 28)[0]
        phsize, phcount = struct.unpack_from("<HH", blob, 42)
        types = [struct.unpack_from("<I", blob, phoff + n * phsize)[0] for n in range(phcount)]
        check(2 not in types and 3 not in types, "Expected static ARM executable")
        print(f"ARM ELF PASS: {len(blob)} bytes, flags=0x{flags:08x}, static EABI5 soft-float")
    if deliverables:
        for name in ("项目书.md", "项目书.docx", "程序框架.md", "architecture.png",
                     "复用与改动说明.md", "部署与验收.md", "演示讲稿.md",
                     "validation/验证记录.md"):
            path = ROOT / "docs" / name
            check(path.is_file() and path.stat().st_size > 0, f"Missing deliverable: {name}")
    print(f"PROJECT PASS: {count} checks (mechanical boundaries only; board not tested)")
    return count


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", action="store_true", help="Check original files in the original workspace")
    parser.add_argument("--artifacts", action="store_true", help="Require desktop and static ARM build artifacts")
    parser.add_argument("--deliverables", action="store_true", help="Require generated project documents")
    args = parser.parse_args()
    verify(args.upstream, args.artifacts, args.deliverables)
