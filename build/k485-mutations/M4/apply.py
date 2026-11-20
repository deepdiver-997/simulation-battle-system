# M4: 初评只看在场槽（场下携带者漏评）
import sys
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k485/resources/moves_lib/k485_carry_2417_dragon_gate.cpp"
s = open(p, encoding="utf-8").read()
old = """    for (int slot = 0; slot < 6; ++slot) {
        ElfPet& pet = ctx->seerRobot[side].elfPets[slot];"""
new = """    for (int slot = 0; slot < 6; ++slot) {
        if (!is_tick && slot != ctx->on_stage[side]) {
            continue;   // M4 变异：初评只看在场
        }
        ElfPet& pet = ctx->seerRobot[side].elfPets[slot];"""
assert s.count(old) == 1, f"pattern count={s.count(old)}"
open(p, "w", encoding="utf-8").write(s.replace(old, new))
assert new in open(p, encoding="utf-8").read()
print("M4 applied+verified")
