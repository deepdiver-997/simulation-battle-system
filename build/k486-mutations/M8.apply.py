# M8: 换宠窗终点多拍（until = start+rounds，窗外一回合误观测）
import io
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k486/resources/moves_lib/k486_attr_counter_and_swap_trigger.cpp"
s = io.open(p, encoding="utf-8").read()
old = "    const int until = start + rounds - 1;"
new = "    const int until = start + rounds;   // M8 变异：窗终点多一拍"
assert s.count(old) == 1, f"pattern count={s.count(old)}"
io.open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("M8 applied+verified")
