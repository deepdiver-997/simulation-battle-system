# M7: 2417 审计键 owner 维度拉平（双边串账）
import sys
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k485/resources/moves_lib/k485_carry_2417_dragon_gate.cpp"
s = open(p, encoding="utf-8").read()
old = "inline int k2417_audit_key_of(int side, int slot) { return k2417AuditBase + side * 16 + slot; }"
new = "inline int k2417_audit_key_of(int side, int slot) { (void)side; return k2417AuditBase + slot; }  // M7 变异：owner 拉平"
assert s.count(old) == 1, f"pattern count={s.count(old)}"
open(p, "w", encoding="utf-8").write(s.replace(old, new))
assert new in open(p, encoding="utf-8").read()
print("M7 applied+verified")
