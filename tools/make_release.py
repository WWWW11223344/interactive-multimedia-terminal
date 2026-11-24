# -*- coding: utf-8 -*-
"""Assemble a self-contained delivery bundle for a zero-experience user.

Produces one folder with four subfolders (board runtime / Windows preview /
full source / documents), a SHA256 manifest, and a single ZIP for transfer.
Requires the ARM binary, Windows exe and board tarball to be built already.
"""
import hashlib
import json
import shutil
import zipfile
from pathlib import Path

ROOT = Path("G:/io/multimedia-terminal")
DEPLOY = Path("G:/io/outputs/Board_deploy")
OUTROOT = Path("G:/io/outputs")
BUNDLE = OUTROOT / "交互式多媒体终端_交付包_1120q"
BUILD = "1120q"
BOARD_TAR = DEPLOY / f"GEC6818-board-1120f-{BUILD}.tar.gz"
BOARD_BIN = DEPLOY / f"terminal-arm-1120f-{BUILD}"

# Directories copied verbatim into the source tree.
SOURCE_DIRS = ("src", "inc", "lib", "cmake", "tests", "tools", "res", "docs")
# Root-level files copied into the source tree.
SOURCE_FILES = ("CMakeLists.txt", "Makefile", "README.md", "run.sh",
                "run-preview.cmd", "requirements-docs.txt", ".gitignore", ".gitattributes")
# Files excluded everywhere (build junk, personal data, editor locks).
EXCLUDE_PARTS = {"__pycache__", ".git", ".vscode", "build", "build-host", "build-arm",
                 "build-demo", "build-arm-gcc", "dist", "data", "shots", "archive"}
EXCLUDE_SUFFIX = (".pyc",)


def clean_copy(src: Path, dst: Path):
    """Copy a file or directory tree while skipping build junk and locks."""
    if src.is_file():
        if src.name.startswith("~$") or src.name.startswith("~WRL") or src.suffix in EXCLUDE_SUFFIX:
            return
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        return
    dst.mkdir(parents=True, exist_ok=True)
    for child in sorted(src.iterdir()):
        if child.name in EXCLUDE_PARTS or child.name.startswith("~$") or child.name.startswith("~WRL"):
            continue
        clean_copy(child, dst / child.name)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def md5(path: Path) -> str:
    return hashlib.md5(path.read_bytes()).hexdigest()


def main():
    assert (ROOT / "build-arm" / "terminal").is_file(), "build ARM binary first"
    assert (ROOT / "build-host" / "terminal.exe").is_file(), "build Windows exe first"
    assert BOARD_TAR.is_file() and BOARD_BIN.is_file(), "board tarball not found"
    # The board tarball must carry the same binary as the freshly built one.
    assert md5(BOARD_BIN) == md5(ROOT / "build-arm" / "terminal"), "board binary out of date"

    if BUNDLE.exists():
        shutil.rmtree(BUNDLE)
    BUNDLE.mkdir(parents=True)

    # 1. Board runtime package.
    board = BUNDLE / "1.开发板运行包"
    board.mkdir()
    shutil.copy2(BOARD_TAR, board / f"GEC6818-board-{BUILD}.tar.gz")
    shutil.copy2(BOARD_BIN, board / f"terminal-arm-{BUILD}")
    (board / "先看这个.txt").write_text(
        "GEC6818 开发板运行包\n\n"
        "完整包 GEC6818-board-1120q.tar.gz：解压到板子 /mnt/terminal 后 chmod +x bin/terminal run.sh，\n"
        "再运行 ./run.sh --fb /dev/fb0 --touch /dev/input/eventN。\n"
        "单文件 terminal-arm-1120q：只替换旧版程序时用，wget 到 bin/terminal 后 chmod +x。\n"
        "eventN 需在板上用 cat /proc/bus/input/devices 查实际节点。\n"
        "详细步骤见 4.项目文档/从零复刻部署指南.md。\n",
        encoding="utf-8", newline="\n")

    # 2. Windows preview package.
    win = BUNDLE / "2.Windows预览包"
    (win / "bin").mkdir(parents=True)
    shutil.copy2(ROOT / "build-host" / "terminal.exe", win / "bin" / "terminal.exe")
    clean_copy(ROOT / "res", win / "res")
    shutil.copy2(ROOT / "run-preview.cmd", win / "run-preview.cmd")
    (win / "先看这个.txt").write_text(
        "Windows 桌面预览包\n\n"
        "双击 run-preview.cmd 即可运行。鼠标按下/拖动/松开模拟触摸。\n"
        "若闪退请把文件夹移到不含中文和空格的路径，例如 D:\\terminal-demo。\n"
        "演示账号 678678 / 678678。这是桌面模拟，不能证明开发板已验收。\n",
        encoding="utf-8", newline="\n")

    # 3. Full source tree.
    src = BUNDLE / "3.项目源码"
    src.mkdir()
    for name in SOURCE_DIRS:
        clean_copy(ROOT / name, src / name)
    for name in SOURCE_FILES:
        if (ROOT / name).is_file():
            shutil.copy2(ROOT / name, src / name)
    (src / "从源码编译说明.txt").write_text(
        "完整工程源码\n\n"
        "Windows 预览：cmake -S . -B build-host -G MinGW Makefiles，再 cmake --build build-host -j4。\n"
        "ARM 程序：工程根目录下运行 powershell -ExecutionPolicy Bypass -File tools/build_arm.ps1。\n"
        "自动测试：ctest --test-dir build-host --output-on-failure（应 5 项全过）。\n"
        "需要 MinGW GCC、CMake；ARM 交叉编译另需 Zig 0.13.0（工具链不含在包内）。\n"
        "详细步骤见 4.项目文档/从零复刻部署指南.md。\n",
        encoding="utf-8", newline="\n")

    # 4. Delivery documents.
    docs = BUNDLE / "4.项目文档"
    docs.mkdir()
    for name in ("项目书.docx", "项目书.md", "项目书.lake",
                 "交互式多媒体终端项目设计方案.docx", "交互式多媒体终端项目设计方案.pdf",
                 "演示讲稿.md", "程序框架.md", "部署与验收.md",
                 "从零复刻部署指南.md", "复用与改动说明.md"):
        path = ROOT / "docs" / name
        if path.is_file():
            shutil.copy2(path, docs / name)

    # Integrity manifest at bundle root.
    lines = []
    for path in sorted(BUNDLE.rglob("*")):
        if path.is_file():
            lines.append(f"{sha256(path)}  {path.relative_to(BUNDLE).as_posix()}")
    (BUNDLE / "校验清单_SHA256.txt").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")

    # Single ZIP for transfer.
    zip_path = OUTROOT / f"交互式多媒体终端_交付包_{BUILD}.zip"
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(BUNDLE.rglob("*")):
            if path.is_file():
                archive.write(path, (BUNDLE.name / path.relative_to(BUNDLE)).as_posix())

    print(f"BUNDLE {BUNDLE}")
    print(f"ZIP    {zip_path}  {zip_path.stat().st_size} bytes")
    print(f"FILES  {len(lines)}  BOARD_TAR_MD5 {md5(BOARD_TAR)}  BOARD_BIN_MD5 {md5(BOARD_BIN)}")


if __name__ == "__main__":
    main()
