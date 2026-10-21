#!/bin/sh
# Build and test on a Windows XP machine over ssh, where neither CMake nor
# Universal Ctags runs: copy the sources, build them with the one-command
# builds at the top of ctags.cpp, run the unit tests, and check that the
# number of threads does not change the output.
# Usage: sh test/xp.sh [HOST]  (default 192.168.1.106)
set -e -o pipefail
host=${1:-192.168.1.106}
dir=src/scratch/ctags

if ! ssh -o BatchMode=yes "$host" true 2>/dev/null; then
    echo "xp.sh: cannot reach $host" >&2
    exit 77
fi

cd "$(dirname "$0")/.."
build=$(sed -n 's,^// w64dk: \$ ,,p' ctags.cpp)
tests=$(sed -n 's,^// tests: \$ ,,p' ctags.cpp)

tar cf - *.cpp test/*.cpp |
    ssh "$host" "rm -rf $dir && mkdir -p $dir && cd $dir && tar xf -" 2>/dev/null

ssh "$host" "cd $dir && $build && $tests && ./ctags_test.exe" 2>&1 |
    grep -v -i 'post-quantum\|store now, decrypt later\|openssh.com/pq'

ssh "$host" "cd $dir && ./ctags.exe --version &&
    ./ctags.exe -R --jobs=1 -f one.tags . &&
    ./ctags.exe -R --jobs=4 -f four.tags . &&
    cmp one.tags four.tags && echo 'xp.sh: threads agree'" 2>&1 |
    grep -v -i 'post-quantum\|store now, decrypt later\|openssh.com/pq'
