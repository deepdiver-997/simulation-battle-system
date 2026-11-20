# M3: 2417 重评 tick 不注册（更新器桶缺席）
import sys
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k485/resources/moves_lib/k485_carry_2417_dragon_gate.cpp"
s = open(p, encoding="utf-8").read()
old = """    ctx->updater_effects.register_effect(State::BATTLE_ROUND_COMPLETION, owner,
                                         std::move(ce),
                                         ctx->round_effect_valid_id[owner]);"""
new = "    /* M3 变异：tick 不注册 */ (void)ce; (void)owner;"
assert s.count(old) == 1, f"pattern count={s.count(old)}"
open(p, "w", encoding="utf-8").write(s.replace(old, new))
assert new in open(p, encoding="utf-8").read()
print("M3 applied+verified")
