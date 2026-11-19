"""Verify delivery ZIPs, optionally rebuilding extracted sources and exercising the Windows package.

All extraction, builds and saved test drawings stay inside temporary directories.
The original projects and the user's drawings are never modified.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PREFIX = "multimedia-terminal/"
BUNDLES = ("项目源码.zip", "Windows-预览包.zip", "GEC6818-运行包.zip")


def run(command, cwd):
    result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"Command failed: {command}\n{result.stdout.decode('utf-8', errors='replace')}")
    return result.stdout


def inspect_archive(path, expected):
    blob = path.read_bytes()
    assert len(blob) == expected["bytes"], f"Archive size mismatch: {path.name}"
    assert hashlib.sha256(blob).hexdigest() == expected["sha256"], f"Archive SHA mismatch: {path.name}"
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None, f"CRC failure: {path.name}"
        names = archive.namelist()
        assert len(names) == len(set(names)), "Duplicate ZIP entries"
        for name in names:
            parts = PurePosixPath(name).parts
            assert name.startswith(PREFIX) and ".." not in parts and "\\" not in name, name
            assert not PurePosixPath(name).is_absolute() and ":" not in name, name
        hashes = {}
        for line in archive.read(PREFIX + "FILES-SHA256.txt").decode("utf-8").splitlines():
            digest, rel = line.split("  ", 1)
            assert rel not in hashes
            hashes[rel] = digest
        files = {n[len(PREFIX):] for n in names if not n.endswith("/")}
        assert files == set(hashes) | {"FILES-SHA256.txt"}, "Incomplete file hash manifest"
        for rel, digest in hashes.items():
            assert hashlib.sha256(archive.read(PREFIX + rel)).hexdigest() == digest, rel
            assert not rel.startswith("data/"), "Personal drawing found in archive"
            assert not any(p.startswith("build-") or p in ("__pycache__", "toolchains", ".git")
                           for p in PurePosixPath(rel).parts), rel
        for rel in ("run.sh", "bin/terminal"):
            if rel in files:
                mode = archive.getinfo(PREFIX + rel).external_attr >> 16
                assert stat.S_ISREG(mode) and mode & 0o111, f"Missing execute permission: {rel}"
        if "run.sh" in files:
            assert b"\r" not in archive.read(PREFIX + "run.sh"), "Linux script must use LF"
        print(f"PASS ZIP: {path.name}; {len(hashes)} file hashes; CRC; safe paths; no personal drawings")


def verify(dist, preview=False, rebuild=False):
    manifest = json.loads((dist / "SHA256.json").read_text(encoding="utf-8"))
    assert set(manifest) == set(BUNDLES)
    for name in BUNDLES:
        inspect_archive(dist / name, manifest[name])
    with tempfile.TemporaryDirectory(prefix="terminal-delivery-") as temp:
        work = Path(temp)
        if rebuild:
            with zipfile.ZipFile(dist / BUNDLES[0]) as archive:
                archive.extractall(work / "source")
            source = work / "source" / "multimedia-terminal"
            run(["cmake", "-S", ".", "-B", "build-host", "-G", "MinGW Makefiles",
                 "-DCMAKE_BUILD_TYPE=Release"], source)
            run(["cmake", "--build", "build-host", "-j", "4"], source)
            run(["ctest", "--test-dir", "build-host", "--output-on-failure"], source)
            print("PASS SOURCE: fresh CMake configure/build and CTest from the extracted source package")
            with zipfile.ZipFile(source / "docs/项目书.docx") as report:
                assert report.testzip() is None
                assert "word/document.xml" in report.namelist()
            print("PASS REPORT: Word archive structure and CRC")
            bundle = source / "project-history.bundle"
            if bundle.is_file():
                git = shutil.which("git")
                assert git, "Git is required to validate the attached history bundle"
                run([git, "bundle", "verify", str(bundle)], ROOT)
                restored = work / "restored"
                run([git, "clone", str(bundle), str(restored)], work)
                tracked = run([git, "ls-files", "-z"], restored).decode("utf-8").split("\0")
                for rel in filter(None, tracked):
                    assert (restored / rel).read_bytes() == (source / rel).read_bytes(), rel
                print("PASS GIT: bundle verifies, clones, and every tracked file matches the source ZIP")
        if preview:
            with zipfile.ZipFile(dist / BUNDLES[1]) as archive:
                archive.extractall(work / "preview")
            source = work / "preview" / "multimedia-terminal"
            from smoke_preview import main as preview_smoke
            preview_smoke(source / "bin/terminal.exe", source)
            print("PASS PREVIEW: extracted Windows package runs and saves a nonblank drawing")
    print("ALL DELIVERY CHECKS PASSED; temporary files cleaned; board not tested; no video generated")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", type=Path, default=ROOT / "dist")
    parser.add_argument("--preview", action="store_true", help="Exercise the actual extracted Win32 application")
    parser.add_argument("--rebuild", action="store_true", help="Rebuild extracted sources and restore Git bundle")
    args = parser.parse_args()
    verify(args.dist.resolve(), args.preview, args.rebuild)
