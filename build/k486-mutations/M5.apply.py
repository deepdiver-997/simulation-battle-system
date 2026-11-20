# M5: 等量口径破坏——反伤量用申报量 max/分母 而非实得 healed
import io
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k486/resources/moves_lib/k486_attr_counter_and_swap_trigger.cpp"
s = io.open(p, encoding="utf-8").read()
old = """            (void)g_core->deal_pink_damage(c, /*target=*/opp, healed,
                                           DamageKind::PERCENT_VALUE, self);"""
new = """            (void)g_core->deal_pink_damage(c, /*target=*/opp, max_hp_of(c, self) / denom,
                                           DamageKind::PERCENT_VALUE, self);   // M5 变异：申报量误读"""
assert s.count(old) == 1, f"pattern count={s.count(old)}"
io.open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("M5 applied+verified")
