"""Exercise the real Win32 window and save a drawing in an isolated temporary folder."""
import argparse
import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import subprocess
import tempfile
import time

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]


def ensure_demo_account(root):
    """The login screen only accepts accounts already in data/accounts.txt.

    A freshly extracted package has an empty data folder, so the smoke check would
    stall on the login page and never reach the drawing board. Append the demo
    account the login screen advertises, exactly as tests/test_terminal.c does.
    """
    path = Path(root) / "data" / "accounts.txt"
    path.parent.mkdir(parents=True, exist_ok=True)
    existing = path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""
    if "123456 123456" not in existing:
        with path.open("a", encoding="utf-8") as handle:
            handle.write("123456 123456\n")


def main(exe, resource_root):
    if os.name != "nt":
        raise RuntimeError("This smoke check is for the Windows preview")
    user = ctypes.WinDLL("user32", use_last_error=True)
    user.SetProcessDPIAware()
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = (callback_type, wintypes.LPARAM)
    user.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
    user.PostMessageW.argtypes = (wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)
    user.IsWindowVisible.argtypes = (wintypes.HWND,)
    user.GetWindowTextW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
    user.GetClientRect.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.RECT))
    with tempfile.TemporaryDirectory(prefix="terminal-preview-") as temp:
        output = Path(temp) / "drawings"
        ensure_demo_account(resource_root)
        process = subprocess.Popen([str(exe), "--root", str(resource_root), "--data", str(output)],
                                   cwd=resource_root, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            windows = []

            @callback_type
            def callback(hwnd, _):
                pid = wintypes.DWORD()
                user.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
                title = ctypes.create_unicode_buffer(256)
                user.GetWindowTextW(hwnd, title, 256)
                if pid.value == process.pid and user.IsWindowVisible(hwnd) and title.value.startswith("GEC6818 Terminal"):
                    windows.append(hwnd)
                return True

            deadline = time.monotonic() + 8
            while not windows and time.monotonic() < deadline:
                user.EnumWindows(callback, 0)
                if process.poll() is not None:
                    raise RuntimeError(f"Preview exited before opening: {process.communicate()}")
                time.sleep(0.05)
            assert windows, "Preview did not open a window"
            hwnd = windows[0]
            rect = wintypes.RECT()
            user.GetClientRect(hwnd, ctypes.byref(rect))
            print(f"Preview client: {rect.right}x{rect.bottom}")

            def event(message, x, y, down=0):
                real_x, real_y = x * rect.right // 800, y * rect.bottom // 480
                assert user.PostMessageW(hwnd, message, down, real_x | (real_y << 16))
                time.sleep(0.15)

            def click(x, y):
                event(0x201, x, y, 1)
                event(0x202, x, y)

            time.sleep(0.3)
            # Sign in: type 123456 twice on the nine-grid keypad, then 确认.
            for _ in range(2):
                for key in range(6):
                    click(432 + (key % 3) * 112 + 52, 190 + (key // 3) * 68 + 30)
            click(708, 430)          # 确认
            time.sleep(1.6)          # let the success animation finish and land on home
            click(640, 260)          # home -> 创意画板
            event(0x201, 100, 220, 1)
            for x, y in ((120, 230), (160, 250), (210, 280), (250, 300)):
                event(0x200, x, y, 1)
            event(0x202, 250, 300)
            click(720, 40)           # 保存
            files = list(output.glob("*.bmp"))
            assert len(files) == 1, "Native window save did not produce one drawing"
            with Image.open(files[0]) as drawing:
                assert drawing.size == (752, 286)
                assert ImageChops.difference(drawing.convert("RGB"), Image.new("RGB", drawing.size, "white")).getbbox(), "Saved drawing is blank"
            click(60, 40)
            click(140, 260)          # home -> 电子相册 -> saved drawing
            click(100, 140)
            click(60, 40)
            click(390, 260)          # home -> 小游戏列表
            click(212, 180)          # 2048
            click(690, 40)           # 开始 / 重新开始
            click(500, 300)          # 下 方向卡片
            click(60, 40)            # 返回小游戏列表
            click(60, 40)            # 返回主页
            click(720, 40)           # Normal application exit.
            stdout, stderr = process.communicate(timeout=5)
            assert process.returncode == 0, (process.returncode, stdout, stderr)
            print("PASS: actual Win32 window startup, keypad login, mouse draw, nonblank BMP save, "
                  "album/photo/小游戏/2048 navigation and normal exit")
            print("Temporary drawing folder cleaned automatically; no user drawings touched; board not tested.")
        finally:
            if process.poll() is None:
                process.terminate()
                process.communicate(timeout=5)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT / "build-host/terminal.exe")
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args()
    main(args.exe.resolve(), args.root.resolve())
