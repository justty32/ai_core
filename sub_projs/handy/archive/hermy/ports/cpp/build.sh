#!/usr/bin/env sh
# build.sh — 編出 hermy（根）＋ lib/*（同層 lib/ 下），維持與原版相同的相對路徑/介面。
set -e
cd "$(dirname "$0")"
CXX="${CXX:-g++}"          # g++16 / clang++22 皆可
FLAGS="${CXXFLAGS:--std=c++20 -O2 -Wall}"
mkdir -p lib
$CXX $FLAGS src/hermy.cpp       -o hermy
$CXX $FLAGS src/skills-json.cpp -o lib/skills-json
$CXX $FLAGS src/ds-chat.cpp     -o lib/ds-chat
$CXX $FLAGS src/run-skill.cpp   -o lib/run-skill
$CXX $FLAGS src/mem-append.cpp  -o lib/mem-append
echo "build ok → ./hermy, ./lib/{skills-json,ds-chat,run-skill,mem-append}"
