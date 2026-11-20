# M6: 810 探针摘除命中过滤（miss 也计次）
import sys
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k485/resources/moves_lib/j_hit_power_decay.cpp"
s = open(p, encoding="utf-8").read()
old = """    if (c->ws.skill_exec_result[owner] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // 真 miss 不衰减
    }
"""
new = "    /* M6 变异：摘除命中过滤（miss 也计次） */\n"
assert s.count(old) == 1, f"pattern count={s.count(old)}"
open(p, "w", encoding="utf-8").write(s.replace(old, new))
assert new in open(p, encoding="utf-8").read()
print("M6 applied+verified")
