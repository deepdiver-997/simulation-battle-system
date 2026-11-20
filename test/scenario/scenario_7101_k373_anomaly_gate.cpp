#include <utils/sbs_rand.h>
// 场景 7101 —— K373 异常门降属+已损恢复+动态概率威力翻倍族
//（856，soul_lib/k_anomaly_gate_drop_heal.cpp；号段 7101，2026-10-24 worktree ../agents/k373）
//
// 断言口径（真实攻击走伤害管线；85%~100% 浮动 → 比例带/精确 heal 整式）：
//   - T7101 row567 ④ 已损恢复（无异常门独立子句）：r1 挨 s1 → r2 回合初精确回
//     s1×30/100（整式复刻），gate 关（无异常）不降属；
//   - T7102 row567 ①② 异常门降属：点害怕后次回合 ROUND_START 采样 → 对手恰 2 项
//     -1（去重、幅度 1）+ 门旗=1；
//   - T7103 row567 ③ 对手输出减伤 50%（门旗票）：r2（门开）伤 s_B / r3（门关、等级
//     持平）伤 s_C = 0.5 整比 → ratio ∈ (0.40, 0.62)（同 P 等级乘数对消）；
//   ⚠️ 比例刀之间把印主 hp 拉满（回合初 heal=0）——per-strike 差值跨 ROUND_START
//   会把 heal 扣进刀伤（T7101 首版实测 437 恰=heal 量的教训）；heal 断言用纯回血
//   回合（双方 NONE）整式对账。
//   - T7104 门关对照：无异常两刀 ratio ∈ [0.83, 1.20]（无减伤）、六槽全 0、旗=0；
//   - T7105 row568 进化行互锚：恰 4 项 -1；恢复 35% 整式；
//   - T7106 ⑤ 动态概率威力翻倍：hp 压到 2/7（回合初 30% 回补后 ≈ 半血 → pct=50）
//     20 回合统计翻倍次数 ∈ [3,17]（p=0.5、n=20 → 期望 10；伤害带判别：
//     拍打 fresh [87,102]（速查 atk300/def100 raw 锚）vs ×2 [174,204]，≥150 即翻倍；
//     K220 3620 同款 debug_step 直读视图旁证 20/40 seeds 翻倍）；满血对照 0 翻倍。
//   ⚠️ 概率/随机降属断言一律跑前 srand 定种子（rand 序固定 → 逐次一致）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物理攻击 filler（P40/PP35/acc100/无附带）
constexpr int kAnomaly = static_cast<int>(AbnormalStatusId::Fear);  // 6 害怕：无跳血tick（烧伤有 max/8 回合跳血，污染刀伤差值——T7103 首版实测）
constexpr int kMaxHp = 800000;
constexpr int kSlots = BattleContext::kAbilityLevelSlotCount;      // 6
constexpr int kGateFlagKey = 990373500;   // 异常门旗（+owner，见实现文件）

SoulMark make_mark(const std::vector<int>& db_args, int monster_id) {
    SoulMark sm(/*effect_id=*/856, "K856 魂印", "test",
                EffectArgs(std::vector<int>(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
}

ElfPet mark_pet(const std::vector<int>& db_args) {
    ElfPet p = make_pet(1, "印主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp,
                        make_mark(db_args, 3242));
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(int atk) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, kMaxHp);
    accuracy_100(p);
    return p;
}

// 对手拍打打印主（印主先 NONE）；返回当刀伤害（差值）
int foe_strike(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int hp_before = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp_before - ctx.getPet(0).hp;
}

int neg_count(BattleContext& ctx, int side) {
    int n = 0;
    for (int i = 0; i < kSlots; ++i) {
        if (ctx.ability_levels[side][i] < 0) {
            ++n;
        }
    }
    return n;
}

bool all_neg_eq(BattleContext& ctx, int side, int expect) {   // 负槽全=expect、正槽=0
    for (int i = 0; i < kSlots; ++i) {
        const int lv = ctx.ability_levels[side][i];
        if (lv > 0 || (lv < 0 && lv != expect)) {
            return false;
        }
    }
    return true;
}

bool all_zero(BattleContext& ctx, int side) {
    for (int i = 0; i < kSlots; ++i) {
        if (ctx.ability_levels[side][i] != 0) {
            return false;
        }
    }
    return true;
}

std::string levels_str(BattleContext& ctx, int side) {
    std::string s = "[";
    for (int i = 0; i < kSlots; ++i) {
        s += std::to_string(ctx.ability_levels[side][i]);
        s += (i + 1 < kSlots) ? ',' : ']';
    }
    return s;
}


}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7101 row567 [2,1,50,30,1] ④：已损恢复 30%（无门独立子句）════════════
    {
        ScenarioBattle b(make_party(mark_pet({2, 1, 50, 30, 1})),
                         make_party(foe_pet(/*atk=*/5000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int s1 = foe_strike(b);
        CHECK(s1 > 300, "T7101 刀1 前提不成立：s1=" << s1);
        CHECK(all_zero(ctx, 1), "T7101 门关不降属（实测 " + levels_str(ctx, 1)
                  + "）");
        // r2 双方 NONE：ROUND_START 纯回血（无刀污染）
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int hp_expect = kMaxHp - s1 + s1 * 30 / 100;
        CHECK_EQ(ctx.getPet(0).hp, hp_expect,
                 "T7101 已损恢复 30% 整式失配：hp=" << ctx.getPet(0).hp << " 期="
                     << hp_expect << "（s1=" << s1 << "）");
        std::cout << "  [T7101] 856 row567 已损恢复30%（s1=" << s1 << "）通过\n";
    }

    // ══ T7102 row567 ①②：异常门 → 对手恰 2 项 -1 ════════════════════════════
    {
        sbs_srand(20261024);
        ScenarioBattle b(make_party(mark_pet({2, 1, 50, 30, 1})),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.abnormal_status_end_round[0][kAnomaly] = ctx.roundCount + 3;   // 点害怕
        CHECK_EQ(neg_count(ctx, 1), 0, "T7102 前提：点害怕当回合未降属");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // 次回合 ROUND_START：门开 → 降属
        CHECK_EQ(neg_count(ctx, 1), 2,
                 "T7102 门开应恰降 2 项（实测 " + levels_str(ctx, 1) + "）");
        CHECK(all_neg_eq(ctx, 1, -1),
              "T7102 幅度应全为 -1（实测 " + levels_str(ctx, 1) + "）");
        const auto it = ctx.plugin_storage.find(kGateFlagKey);
        CHECK(it != ctx.plugin_storage.end()
                  && (it->second.type() == typeid(int))
                  && std::any_cast<int>(it->second) == 1,
              "T7102 门旗应=1");
        std::cout << "  [T7102] 856 row567 异常门降属 2 项 通过\n";
    }

    // ══ T7103 row567 ③：门开对手输出 -50%（同等级对比刀）══════════════════════
    {
        sbs_srand(20261025);
        ScenarioBattle b(make_party(mark_pet({2, 1, 50, 30, 1})),
                         make_party(foe_pet(/*atk=*/5000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int s_a = foe_strike(b);            // r1：门关（无异常），fresh
        ctx.getPet(0).hp = kMaxHp;   // 拉满 → 回合初 heal=0，不污染比例刀
        ctx.abnormal_status_end_round[0][kAnomaly] = ctx.roundCount + 3;
        const int s_b = foe_strike(b);            // r2：门开 → 降属 + 输出-50%
        CHECK(neg_count(ctx, 1) == 2,
              "T7103 r2 门开应已降 2 项（实测 " + levels_str(ctx, 1) + "）");
        ctx.getPet(0).hp = kMaxHp;   // 拉满 → heal=0
        ctx.abnormal_status_end_round[0][kAnomaly] = 0;   // 熄门
        const int s_c = foe_strike(b);            // r3：门关、等级持平 → fresh×P
        const double ratio =
            static_cast<double>(s_b) / static_cast<double>(s_c);
        CHECK(ratio > 0.40 && ratio < 0.62,
              "T7103 门开刀应被减半（同 P 对消）：s_a=" << s_a << " s_b=" << s_b
                  << " s_c=" << s_c << " ratio=" << ratio);
        std::cout << "  [T7103] 856 row567 对手输出-50%（s_b/s_c=" << ratio
                  << "）通过\n";
    }

    // ══ T7104 门关对照：无异常 → 无减伤无降属 ════════════════════════════════
    {
        sbs_srand(20261026);
        ScenarioBattle b(make_party(mark_pet({2, 1, 50, 30, 1})),
                         make_party(foe_pet(/*atk=*/5000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int s1 = foe_strike(b);
        ctx.getPet(0).hp = kMaxHp;   // 拉满 → heal=0
        const int s2 = foe_strike(b);
        const double ratio = static_cast<double>(s2) / static_cast<double>(s1);
        CHECK(ratio >= 0.83 && ratio <= 1.20,
              "T7104 门关不应有减伤：s1=" << s1 << " s2=" << s2 << " ratio="
                  << ratio);
        CHECK(all_zero(ctx, 1), "T7104 门关不降属（实测 " + levels_str(ctx, 1)
                  + "）");
        CHECK(ctx.plugin_storage.find(kGateFlagKey) == ctx.plugin_storage.end()
                  || std::any_cast<int>(ctx.plugin_storage[kGateFlagKey]) == 0,
              "T7104 门旗应=0");
        std::cout << "  [T7104] 856 门关对照（ratio=" << ratio << "）通过\n";
    }

    // ══ T7105 row568 [4,1,60,35,1] 进化行互锚：4 项 -1 + 35% 整式 ════════════
    {
        sbs_srand(20261027);
        ScenarioBattle b(make_party(mark_pet({4, 1, 60, 35, 1})),
                         make_party(foe_pet(/*atk=*/5000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int s1 = foe_strike(b);
        ctx.abnormal_status_end_round[0][kAnomaly] = ctx.roundCount + 3;
        // r2 双方 NONE：门开（4 项降属）+ 纯回血 35%（无刀污染）
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(neg_count(ctx, 1), 4,
                 "T7105 进化行应恰降 4 项（实测 " + levels_str(ctx, 1) + "）");
        CHECK(all_neg_eq(ctx, 1, -1),
              "T7105 幅度应全为 -1（实测 " + levels_str(ctx, 1) + "）");
        const int hp_expect = kMaxHp - s1 + s1 * 35 / 100;
        CHECK_EQ(ctx.getPet(0).hp, hp_expect,
                 "T7105 已损恢复 35% 整式失配：hp=" << ctx.getPet(0).hp << " 期="
                     << hp_expect);
        std::cout << "  [T7105] 856 row568 进化行（4 项/35%）通过\n";
    }

    // ══ T7106 row567 ⑤：动态概率威力翻倍（半血 → pct≈50）═════════════════════
    {
        sbs_srand(20261028);
        ScenarioBattle b(make_party(mark_pet({2, 1, 50, 30, 1})),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        int doubles = 0;
        for (int r = 0; r < 20; ++r) {
            ctx.getPet(0).hp = 2 * kMaxHp / 7;   // 压血：回合初回 30% 后 ≈ 半血
            const int hp_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            const int dmg = hp_before - ctx.getPet(1).hp;
            if (dmg >= 150) {
                ++doubles;   // 拍打 fresh [87,102]（速查 raw 锚）vs ×2 [174,204]：
                             // ≥150 即翻倍（首版 400 阈值系 K164 特例带，实测勘误）
            }
        }
        CHECK(doubles >= 3 && doubles <= 17,
              "T7106 半血 20 回合翻倍次数 ∈ [3,17]（期望 10，实测 "
                  + std::to_string(doubles) + "）");
        std::cout << "  [T7106] 856 动态概率威力翻倍（翻倍 " << doubles
                  << "/20 @pct≈50）通过\n";
    }

    // ══ T7107 ⑤ 满血对照：pct=0 → 不翻倍 ════════════════════════════════════
    {
        sbs_srand(20261029);
        ScenarioBattle b(make_party(mark_pet({2, 1, 50, 30, 1})),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 0; r < 5; ++r) {
            const int hp_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            const int dmg = hp_before - ctx.getPet(1).hp;
            CHECK(dmg < 150, "T7107 满血不应翻倍（r" << r << " dmg=" << dmg
                      << "）");
        }
        std::cout << "  [T7107] 856 满血不翻倍对照 通过\n";
    }

    std::cout << "scenario_7101 PASS (K373 异常门降属+已损恢复+动态概率威力翻倍族)"
              << std::endl;
    return 0;
}
