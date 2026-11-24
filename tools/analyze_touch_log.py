"""Read data/touch.log from the board and locate the strip where Y stops reporting.

The console only prints one line per gesture, so the raw stream behind it has to be
counted rather than eyeballed. This prints, for each Y channel the panel publishes:

  - the range that was actually seen, and how many samples landed there;
  - the largest run of values that never appeared between the two ends, which is the
    strip the finger cannot report a position in;
  - the values that were reported most often, because a controller that cannot read a
    strip snaps to that strip's edges and those edges then dominate the histogram.

Usage:  python tools/analyze_touch_log.py data/touch.log
"""
import sys
from collections import Counter

ABS_Y = 1
ABS_MT_POSITION_Y = 0x36
EV_SYN = 0
EV_ABS = 3
SYN_DROPPED = 3


def load(path):
    """Return (per-channel Y values, dropped count, event count)."""
    channels = {"ABS_Y": [], "ABS_MT_POSITION_Y": []}
    dropped = 0
    total = 0
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) != 4:
                continue
            total += 1
            _, type_, code, value = (int(item) for item in parts)
            if type_ == EV_SYN and code == SYN_DROPPED:
                dropped += 1
            elif type_ == EV_ABS and code == ABS_Y:
                channels["ABS_Y"].append(value)
            elif type_ == EV_ABS and code == ABS_MT_POSITION_Y:
                channels["ABS_MT_POSITION_Y"].append(value)
    return channels, dropped, total


def report(name, values):
    """Print one channel's range, its largest hole, and its most common values."""
    if not values:
        print(f"{name:18s} 没有采样（这条通道面板不发）")
        return None
    low, high = min(values), max(values)
    counts = Counter(values)
    seen = set(values)
    best_start, best_len, run_start = 0, 0, None
    for value in range(low, high + 1):
        if value not in seen:
            if run_start is None:
                run_start = value
        elif run_start is not None:
            if value - run_start > best_len:
                best_len, best_start = value - run_start, run_start
            run_start = None
    print(f"{name:18s} {len(values)} 个采样，范围 {low}..{high}")
    if best_len:
        print(f"{'':18s} 最大的空洞 {best_start}..{best_start + best_len - 1}"
              f"（{best_len} 个单位，约 {best_len * 480 / 600:.0f} 像素）")
    else:
        print(f"{'':18s} 没有空洞，全程连续")
    common = counts.most_common(6)
    print(f"{'':18s} 出现最多的值 " + "  ".join(
        f"{value}({times}次)" for value, times in common))
    return best_start, best_len


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    channels, dropped, total = load(sys.argv[1])
    print(f"事件 {total} 个，SYN_DROPPED {dropped} 次\n")
    holes = {}
    for name, values in channels.items():
        result = report(name, values)
        if result:
            holes[name] = result
    print()
    primary = holes.get("ABS_Y")
    alternate = holes.get("ABS_MT_POSITION_Y")
    if alternate is None:
        print("第二条通道没有采样：面板只走声明通道，没有可替换的另一条。")
    elif primary and not alternate[1]:
        print("结论：MT 通道没有空洞 —— 换通道就能修好，不必改版式。")
    elif primary and alternate[1]:
        print(f"结论：两条通道都在 {alternate[0]}..{alternate[0] + alternate[1] - 1} 有空洞"
              " —— 面板那一带读不出来，软件只能避让。")


if __name__ == "__main__":
    main()
