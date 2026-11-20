# M9: 第二层 tick 条目 duration 1→2（跨回合重拍）
import io
p = "/Users/zhuhongrui/.agent-workspaces/workspaces/sbs-local/k486/resources/moves_lib/k486_attr_counter_and_swap_trigger.cpp"
s = io.open(p, encoding="utf-8").read()
old = """            auto ce = std::make_unique<ContinuousEffect>(std::move(e), State::BATTLE_ROUND_START,
                                                         self, /*duration=*/1,
                                                         /*registeredRound=*/c->roundCount);"""
new = """            auto ce = std::make_unique<ContinuousEffect>(std::move(e), State::BATTLE_ROUND_START,
                                                         self, /*duration=*/2,
                                                         /*registeredRound=*/c->roundCount);   // M9 变异：duration 2"""
assert s.count(old) == 1, f"pattern count={s.count(old)}"
io.open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("M9 applied+verified")
