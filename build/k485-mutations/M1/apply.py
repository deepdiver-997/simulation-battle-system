# M1: 2417 龙系判向取反（非龙解锁/是龙置锁）
import sys
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k485/resources/moves_lib/k485_carry_2417_dragon_gate.cpp"
s = open(p, encoding="utf-8").read()
old = "                s.is_locked = !dragon;   // 非龙置锁 / 是龙解封（双向）"
new = "                s.is_locked = dragon;   // M1 变异：判向取反"
assert s.count(old) == 1, f"pattern count={s.count(old)}"
open(p, "w", encoding="utf-8").write(s.replace(old, new))
assert new in open(p, encoding="utf-8").read()
print("M1 applied+verified")
