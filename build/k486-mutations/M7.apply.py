# M7: 审计键位步长减半（owner×16→×8 同基址交叠串账）
import io
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k486/resources/moves_lib/k486_attr_counter_and_swap_trigger.cpp"
s = io.open(p, encoding="utf-8").read()
old = "inline int audit_key_of(int base, int owner, int slot) { return base + owner * 16 + slot; }"
new = "inline int audit_key_of(int base, int owner, int slot) { return base + owner * 8 + slot; }   // M7 变异：步长减半"
assert s.count(old) == 1, f"pattern count={s.count(old)}"
io.open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("M7 applied+verified")
