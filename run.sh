#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
if [ -x ./bin/terminal ]; then
    exec ./bin/terminal "$@"
fi
exec ./build/terminal "$@"
