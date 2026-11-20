# M2: 龙系集合摘除复合成员 26（水龙）——复合组件面失效
import sys
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k485/resources/moves_lib/k485_carry_2417_dragon_gate.cpp"
s = open(p, encoding="utf-8").read()
old = "constexpr int kDragonTypedIds[] = {15, 26, 28, 32, 39, 40, 43, 55,"
new = "constexpr int kDragonTypedIds[] = {15, 28, 32, 39, 40, 43, 55,   // M2 变异：摘 26"
assert s.count(old) == 1, f"pattern count={s.count(old)}"
open(p, "w", encoding="utf-8").write(s.replace(old, new))
assert new in open(p, encoding="utf-8").read()
print("M2 applied+verified")
