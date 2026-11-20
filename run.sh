#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
# 两种目录布局：运行包是 bin/terminal，源码树是 build/terminal。
if [ -x ./bin/terminal ]; then
    exec ./bin/terminal "$@"
fi
if [ -x ./build/terminal ]; then
    exec ./build/terminal "$@"
fi
# 区分“没有这个文件”和“有这个文件但没有执行权限”。只写 exec ./build/terminal 的话，
# 权限丢了只会看到 “exec: line 7: ./build/terminal: not found”，看不出真正原因。
if [ -f ./bin/terminal ]; then
    echo "bin/terminal 存在但没有执行权限，请先执行：chmod +x bin/terminal run.sh" >&2
    exit 1
fi
if [ -f ./build/terminal ]; then
    echo "build/terminal 存在但没有执行权限，请先执行：chmod +x build/terminal" >&2
    exit 1
fi
echo "找不到可执行程序：请先构建，或使用运行包里的 bin/terminal" >&2
exit 1
