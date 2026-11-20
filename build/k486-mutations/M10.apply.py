# M10: 2060 重施放摘旧删除（双 watcher 双触发）
import io
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k486/resources/moves_lib/k486_attr_counter_and_swap_trigger.cpp"
s = io.open(p, encoding="utf-8").read()
old = """    const int k2060HandleKey = 990206001;
    const int old_handle = storage_int_local(ctx, k2060HandleKey + self, -1);
    if (old_handle >= 0) {
        ctx->event_center_.remove_watcher(old_handle);
    }"""
new = """    const int k2060HandleKey = 990206001;
    // M10 变异：重施放摘旧删除（双 watcher 双触发）"""
assert s.count(old) == 1, f"pattern count={s.count(old)}"
io.open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("M10 applied+verified")
