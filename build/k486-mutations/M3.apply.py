# M3: 1562 未落地仍武装换宠窗（「触发后」限定语删除）
import io
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k486/resources/moves_lib/k486_attr_counter_and_swap_trigger.cpp"
s = io.open(p, encoding="utf-8").read()
old = """    if (!landed) {
        return EffectResult::kOk;   // 未触发：不武装换宠窗（「触发后」限定语）
    }"""
new = """    // M3 变异：未落地也武装换宠窗（「触发后」限定语删除）"""
assert s.count(old) == 1, f"pattern count={s.count(old)}"
io.open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("M3 applied+verified")
