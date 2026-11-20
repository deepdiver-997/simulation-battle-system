# M5: 810 账本容器回迁 soulmark_storage（旧"跨切换保留"口径——翻案差分杀点）
import sys
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k485/resources/moves_lib/j_hit_power_decay.cpp"
s = open(p, encoding="utf-8").read()
o1 = "auto& store = c->getPet(owner).on_stage_storage;   // 2026-11-19 翻案：下场重置作用域"
n1 = "auto& store = c->getPet(owner).soulmark_storage;   // M5 变异：回迁跨切换保留"
o2 = "auto& store = ctx->getPet(owner).on_stage_storage;   // 2026-11-19 翻案：下场重置作用域"
n2 = "auto& store = ctx->getPet(owner).soulmark_storage;   // M5 变异：回迁跨切换保留"
assert s.count(o1) == 1 and s.count(o2) == 1, f"counts={s.count(o1)},{s.count(o2)}"
s = s.replace(o1, n1).replace(o2, n2)
open(p, "w", encoding="utf-8").write(s)
t = open(p, encoding="utf-8").read()
assert t.count(n1) == 1 and t.count(n2) == 1
print("M5 applied+verified")
