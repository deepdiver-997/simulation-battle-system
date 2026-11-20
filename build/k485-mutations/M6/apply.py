# M6: 810 衰减折算丢 uses 乘数（每次命中都只降一档，不按次折算）
import sys
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k485/resources/moves_lib/j_hit_power_decay.cpp"
s = open(p, encoding="utf-8").read()
old = "        ctx->ws.skill_power_view[owner] = std::max(0, cur - decay * uses);"
new = "        ctx->ws.skill_power_view[owner] = std::max(0, cur - decay);   // M6 变异：丢 ×uses 折算"
assert s.count(old) == 1, f"pattern count={s.count(old)}"
open(p, "w", encoding="utf-8").write(s.replace(old, new))
assert new in open(p, encoding="utf-8").read()
print("M6 applied+verified")
