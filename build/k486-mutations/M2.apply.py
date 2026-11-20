# M2: 1562 首层 {0}% 行参硬编码 100（关闸档/80% 档失效）
import io
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k486/resources/moves_lib/k486_attr_counter_and_swap_trigger.cpp"
s = io.open(p, encoding="utf-8").read()
old = """    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), self, chance, ChanceSource::Skill);
    const bool landed = k486_anomaly_landed(r);"""
new = """    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), self, 100, ChanceSource::Skill);
    const bool landed = k486_anomaly_landed(r);   // M2 变异：{0}% 硬编码 100"""
assert s.count(old) == 1, f"pattern count={s.count(old)}"
io.open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("M2 applied+verified")
