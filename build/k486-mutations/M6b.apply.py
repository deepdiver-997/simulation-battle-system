# M6b: "当回合开始后"时点塌缩（审计面同步写变体）——drain 即掷且 slot12/13/14 照写，
# 仅 h2 时点停泊三检查点可杀（验证时点腿非冗余杀位）。
import io
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k486/resources/moves_lib/k486_attr_counter_and_swap_trigger.cpp"
s = io.open(p, encoding="utf-8").read()
old = """            Effect e(/*id=*/1562, /*priority=*/0, /*owner=*/self, /*left_round=*/1,
                     EffectArgs(std::vector<int>{self, opp, chance2, anomaly2}),
                     &k486_1562_round_start_tick);
            auto ce = std::make_unique<ContinuousEffect>(std::move(e), State::BATTLE_ROUND_START,
                                                         self, /*duration=*/1,
                                                         /*registeredRound=*/c->roundCount);
            ce->source_id_ = 1562;
            c->register_skill_effect(State::BATTLE_ROUND_START, self, std::move(ce));"""
new = """            // M6b 变异：时点塌缩——换宠 drain 即掷（审计面同步写，仅时点停泊腿可杀）
            const ApplyAnomalyResult r2 = g_core->apply_anomaly_checked(
                c, opp, anomaly2, random_anomaly_duration(), self, chance2,
                ChanceSource::Skill);
            set_storage_int_local(c, audit_key_of(k1562AuditBase, self, 12),
                                  k486_anomaly_landed(r2) ? 1 : 0);
            set_storage_int_local(c, audit_key_of(k1562AuditBase, self, 13),
                                  static_cast<int>(r2));
            set_storage_int_local(c, audit_key_of(k1562AuditBase, self, 14),
                                  storage_int_local(c, audit_key_of(k1562AuditBase, self, 14))
                                      + 1);"""
assert s.count(old) == 1, f"pattern count={s.count(old)}"
io.open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("M6b applied+verified")
