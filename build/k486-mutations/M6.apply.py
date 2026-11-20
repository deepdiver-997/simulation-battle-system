# M6: "当回合开始后"时点塌缩——换宠 drain 即掷（ROUND_START 条目注册替换为直掷）
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
new = """            // M6 变异：时点塌缩——换宠 drain 即掷（1600 简化读法，非"当回合开始后"）
            (void)g_core->apply_anomaly_checked(c, opp, anomaly2, random_anomaly_duration(),
                                                self, chance2, ChanceSource::Skill);"""
assert s.count(old) == 1, f"pattern count={s.count(old)}"
io.open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("M6 applied+verified")
