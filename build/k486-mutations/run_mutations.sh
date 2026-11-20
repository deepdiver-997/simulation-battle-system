#!/bin/bash
# K486 变异测试驱动（删 .o 强编 + SHA 双守卫，K471/K474/K485 同法）
# 用法: sh run_mutations.sh M1 M2 ...   （缺省跑全部 M1..M10）
set -u
WS=/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k486
SRC=$WS/resources/moves_lib/k486_attr_counter_and_swap_trigger.cpp
OBJ=$WS/build/CMakeFiles/moves_plugin.dir/resources/moves_lib/k486_attr_counter_and_swap_trigger.cpp.o
DYLIB=$WS/build/plugins/moves_lib/liblib_1.dylib
BIN=$WS/build/sim_scenario_8730_k486
BASELINE_SRC=/tmp/k486-baseline-source.cpp
MDIR=$WS/build/k486-mutations
BASE_SHA=$(shasum -a 256 /tmp/k486-baseline-dylib.dylib | awk '{print $1}')

run_one() {
  local mid=$1
  local out=$MDIR/$mid
  mkdir -p "$out"
  echo "═══ $mid ═══"
  # 1) apply
  python3 "$MDIR/$mid.apply.py" > "$out/apply.log" 2>&1
  local arc=$?
  echo "apply rc=$arc" >> "$out/apply.log"
  [ $arc -ne 0 ] && { echo "$mid APPLY FAILED"; return 1; }
  grep -c "$mid" "$SRC" > "$out/grep-confirm.log" 2>&1 || true
  grep "$mid" "$SRC" > "$out/grep-confirm.log" 2>&1 || echo "(marker via semantic replace)" >> "$out/grep-confirm.log"
  # 2) 强编（删 .o 迫使重编——Make 3.81 mtime 整数秒截断坑）
  rm -f "$OBJ"
  (cd $WS && cmake --build build -j8) > "$out/mutated-build.log" 2>&1
  echo $? > "$out/mutated-build.rc"
  [ "$(cat "$out/mutated-build.rc")" != "0" ] && { echo "$mid BUILD FAILED"; return 1; }
  shasum -a 256 "$DYLIB" | awk '{print $1}' > "$out/dylib-sha-mutated"
  # 3) run
  (cd $WS && ./build/sim_scenario_8730_k486) > "$out/run.log" 2>&1
  echo $? > "$out/run.rc"
  grep -A2 "CHECK" "$out/run.log" | head -4 > "$out/first-kill" 2>/dev/null || true
  echo "$mid: run.rc=$(cat "$out/run.rc") sha_mut=$(cat "$out/dylib-sha-mutated")"
  grep -E "FAILED|at scenario_8730" "$out/run.log" | head -3
  # 4) restore + 强编 + SHA 守卫 + 复绿
  cp "$BASELINE_SRC" "$SRC"
  rm -f "$OBJ"
  (cd $WS && cmake --build build -j8) > "$out/restored-build.log" 2>&1
  echo $? > "$out/restored-build.rc"
  shasum -a 256 "$DYLIB" | awk '{print $1}' > "$out/dylib-sha-restored"
  local rsha=$(cat "$out/dylib-sha-restored")
  [ "$rsha" != "$BASE_SHA" ] && { echo "$mid SHA GUARD FAILED (restored $rsha != base $BASE_SHA)"; return 1; }
  (cd $WS && ./build/sim_scenario_8730_k486) > "$out/rerun.log" 2>&1
  echo $? > "$out/rerun.rc"
  echo "$mid: restored-build.rc=$(cat "$out/restored-build.rc") sha==base OK rerun.rc=$(cat "$out/rerun.rc")"
  # 5) semantic diff 存档
  diff "$BASELINE_SRC" "$SRC" > "$out/semantic.diff" 2>&1 || true
}

ALL="M1 M2 M3 M4 M5 M6 M6b M7 M8 M9 M10"
[ $# -gt 0 ] && ALL="$@"
for m in $ALL; do run_one "$m" || echo "!!! $m FAILED CHAIN"; done
echo "ALL DONE"
