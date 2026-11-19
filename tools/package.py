"""Build, test, verify, regenerate docs, then write three local delivery ZIPs.

Requires built ARM output (tools/build_arm.ps1 or equivalent), MinGW/CMake on
PATH for the host build, and the host-only dependencies in requirements-docs.txt.
Never uploads files or packages personal drawings/toolchains/build caches.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import zipfile

from verify_project import verify

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / "dist"


def run(command, log):
    result = subprocess.run(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    entry = "COMMAND: " + " ".join(map(str, command)) + "\n"
    entry += result.stdout.decode("utf-8", errors="replace") + f"\nEXIT CODE: {result.returncode}\n"
    (ROOT / "docs/validation" / log).write_text(entry, encoding="utf-8")
    print(entry[-1600:])
    result.check_returncode()


def zip_bundle(name, files, texts):
    path = DIST / name
    hashes = {}
    prefix = "multimedia-terminal/"
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        def write(rel, blob, executable=False):
            info = zipfile.ZipInfo(prefix + rel)
            info.create_system = 3
            mode = 0o755 if executable else 0o644
            info.external_attr = (stat.S_IFREG | mode) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, blob)
            hashes[rel] = hashlib.sha256(blob).hexdigest()

        for source, rel in sorted(files, key=lambda item: item[1]):
            write(rel, source.read_bytes(), rel.endswith(".sh") or rel == "bin/terminal")
        for rel, value in texts.items():
            write(rel, value if isinstance(value, bytes) else value.encode("utf-8"))
        write("FILES-SHA256.txt", "".join(f"{value}  {key}\n" for key, value in sorted(hashes.items())).encode())
        for folder in ("data/", "data/drawings/"):
            info = zipfile.ZipInfo(prefix + folder)
            info.create_system = 3
            info.external_attr = (stat.S_IFDIR | 0o755) << 16 | 0x10
            archive.writestr(info, b"")
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
    print(f"WROTE {name}: {path.stat().st_size} bytes, {len(hashes)} files")
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--record-snapshot", action="store_true",
                        help="Record delivery sources and generated docs in an existing local Git repository")
    args = parser.parse_args()
    (ROOT / "docs/validation").mkdir(exist_ok=True)
    run(["cmake", "--build", "build-host", "-j", "4"], "host-build.txt")
    run(["ctest", "--test-dir", "build-host", "--output-on-failure"], "ctest.txt")
    run([str(ROOT / "build-host/terminal_tests.exe")], "regression.txt")
    run([sys.executable, "tools/build_docs.py"], "documents.txt")
    verify(artifacts=True, deliverables=True)
    DIST.mkdir(exist_ok=True)
    source = []
    for folder in ("src", "inc", "lib", "res", "cmake", "tests", "tools", "docs"):
        for path in (ROOT / folder).rglob("*"):
            if not path.is_file() or "__pycache__" in path.parts:
                continue
            if path.parent.name == "screenshots" and path.suffix.lower() == ".bmp":
                continue  # Recreated by tests; labelled PNGs are in the report.
            source.append((path, path.relative_to(ROOT).as_posix()))
    for name in ("README.md", ".gitignore", "CMakeLists.txt", "Makefile", "run.sh", "run-preview.cmd", "requirements-docs.txt"):
        source.append((ROOT / name, name))
    texts = {"源码包说明.txt": "从此目录构建。运行依赖为 res；data/drawings 为保存目录。\n"
             "视频由学生自行录制，录制提纲见 docs/演示讲稿.md。板端程序尚待实机验收。\n"
             "Git历史如附 project-history.bundle，可执行 git clone project-history.bundle restored-source 恢复已提交版本。\n"}
    git = shutil.which("git")
    if args.record_snapshot:
        if not git or not (ROOT / ".git").is_dir():
            raise RuntimeError("Initialize a local Git repository before requesting a delivery snapshot")
        subprocess.run([git, "add", "--", *[rel for _, rel in source]], cwd=ROOT, check=True)
        changed = subprocess.run([git, "diff", "--cached", "--quiet"], cwd=ROOT)
        if changed.returncode == 1:
            subprocess.run([git, "-c", "user.name=Codex", "-c", "user.email=codex@local.invalid",
                            "commit", "--quiet", "-m", "Build touch entertainment terminal from provided course projects"],
                           cwd=ROOT, check=True)
        elif changed.returncode != 0:
            changed.check_returncode()
    if git and (ROOT / ".git").is_dir():
        history = subprocess.check_output([git, "log", "--date=iso-strict", "--format=%h %ad %an %s"], cwd=ROOT)
        texts["GIT-HISTORY.txt"] = history
        bundle = DIST / "project-history.bundle"
        subprocess.run([git, "bundle", "create", str(bundle), "--all"], cwd=ROOT, check=True)
        texts["project-history.bundle"] = bundle.read_bytes()
    paths = [zip_bundle("项目源码.zip", source, texts)]
    resources = [(p, p.relative_to(ROOT).as_posix()) for p in (ROOT / "res").rglob("*") if p.is_file()]
    common = resources + [(ROOT / "docs/部署与验收.md", "部署与验收.md"),
                          (ROOT / "docs/演示讲稿.md", "演示讲稿.md")]
    paths.append(zip_bundle("Windows-预览包.zip", common + [
        (ROOT / "build-host/terminal.exe", "bin/terminal.exe"),
        (ROOT / "run-preview.cmd", "run-preview.cmd")], {
            "运行说明.txt": "解压整个文件夹后双击 run-preview.cmd；鼠标模拟800×480触摸。\n"
            "功能：时钟、电子相册、2048、画板。画作保存到 data/drawings。\n"
            "这是Windows桌面模拟，不能用于证明开发板已验收。\n"}))
    paths.append(zip_bundle("GEC6818-运行包.zip", common + [
        (ROOT / "build-arm/terminal", "bin/terminal"), (ROOT / "run.sh", "run.sh")], {
            "运行说明.txt": "目标GEC6818 / 800×480 / 32位ARM Linux。ARMv7-A EABI soft-float静态musl。\n"
            "cd 到本目录，chmod +x bin/terminal run.sh，运行 ./run.sh --touch /dev/input/eventN。\n"
            "eventN须核对实际触屏节点；方向参数与验收表见部署与验收.md。\n"
            "编译通过，尚未上板验证。视频由学生自行录制。\n"}))
    manifest = {p.name: {"bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths}
    (DIST / "SHA256.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    validation = subprocess.run([sys.executable, "tools/verify_packages.py", "--preview", "--rebuild"],
                                cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    receipt = validation.stdout.decode("utf-8", errors="replace")
    receipt += f"\nEXIT CODE: {validation.returncode}\n"
    (DIST / "delivery-validation.txt").write_text(receipt, encoding="utf-8")
    print(receipt)
    validation.check_returncode()
    print("Delivery ZIP CRC, SHA256 and extracted-package checks complete. Board not tested; no video generated.")


if __name__ == "__main__":
    main()
