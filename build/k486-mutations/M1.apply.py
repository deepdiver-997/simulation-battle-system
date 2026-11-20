# M1: 观测面——2060 watcher 的 used_attribute 门删除（攻技也触发）
import io
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k486/resources/moves_lib/k486_attr_counter_and_swap_trigger.cpp"
s = io.open(p, encoding="utf-8").read()
old = """            if (!c || ev.actor != opp || ev.target != self || !ev.used_attribute) {
                return;   // 条件：对手使用的是属性技（使用即触发，E13 字面）
            }"""
new = """            if (!c || ev.actor != opp || ev.target != self) {
                return;   // M1 变异：used_attribute 门删除（攻技也触发）
            }"""
assert s.count(old) == 1, f"pattern count={s.count(old)}"
io.open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("M1 applied+verified")
