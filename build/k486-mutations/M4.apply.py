# M4: 2060 触发仍回退（landed 分支 return 删除——掷中也走 heal+反伤）
import io
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k486/resources/moves_lib/k486_attr_counter_and_swap_trigger.cpp"
s = io.open(p, encoding="utf-8").read()
old = """            if (landed) {
                // 触发：不回退（官方「未触发则」回退分支在触发时不走——slot1=0 负测锁）。
                set_storage_int_local(c, audit_key_of(k2060AuditBase, self, 1), 0);
                return;
            }"""
new = """            if (landed) {
                // M4 变异：触发仍回退（return 删除——掷中也走 heal+反伤）
            }"""
assert s.count(old) == 1, f"pattern count={s.count(old)}"
io.open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("M4 applied+verified")
