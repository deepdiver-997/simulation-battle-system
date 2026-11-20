// 场景 7060 —— K361 降转升+阈值反弹族（819，soul_lib/k_down_invert_reflect_gate.cpp）
//（2026-10-24，worktree ../agents/k361；号段 7060-7069 → T7060-T7064 五块）
//
// 断言口径（真实攻击走伤害管线；atk/def=100 时单击基数 = 0.336×atk+2，×[0.851,1.0]）：
//   - T7060 阶段末降转升：预置 -2/-1/+1 → 回合后 -2→+2、-1→+1、+1 保持；
//   - T7061 门生效（row517 [300,1]，atk=1500 → 伤 [430,506] 全 >300）：
//     印主 hp 不动（免疫）+ 对手掉血=来伤全额（反弹×1）；
//   - T7062 ≤阈值不触（foe atk=700 → 伤 [202,238] ≤300）：伤害照落、无反弹；
//   - T7063 行间阈值区分（atk=880 → 伤 [253,297] ⊆ (250,300]）：row939 [250,1] 免疫、
//     row517 [300,1] 不免——同伤害带分辨两行阈值；
//   - T7064 无正等级不设门（row517、无提升、atk=1500）：伤害照落、无反弹。
#include <array>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物理攻击 filler
constexpr int kMaxHp = 99999;

ElfPet mark_pet(const std::vector<int>& row_args, int atk) {
    ElfPet p = make_pet(1, "印主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, kMaxHp,
                        /*soul_mark=*/SoulMark(819, "天香小乔魂印", "",
                                               EffectArgs(
                                                   std::vector<int>{row_args[0],
                                                                    row_args[1]})));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
        s.critical_strike_rate = 0.0f;
    }
    return p;
}

ElfPet foe_pet(int atk) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
        s.critical_strike_rate = 0.0f;
    }
    return p;
}

// 印主 NONE 先手、对手拍打后手打印主；返回 {self_loss, opp_loss}
std::pair<int, int> foe_strike(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    BattleContext& ctx = b.ctx();
    return {kMaxHp - ctx.getPet(0).hp, kMaxHp - ctx.getPet(1).hp};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7060 战斗阶段结束：能力下降反转为上升 ══
    {
        ScenarioBattle b(make_party(mark_pet({300, 1}, /*atk=*/1)),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)stat_change(&ctx, 0, 0, -2);   // 攻 -2
        (void)stat_change(&ctx, 0, 4, -1);   // 速 -1
        (void)stat_change(&ctx, 0, 2, +1);   // 防 +1（正槽不受反转影响）
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T7060.a 攻 -2 → 阶段末反转为 +2");
        CHECK_EQ(ctx.ability_levels[0][4], 1, "T7060.b 速 -1 → 反转为 +1");
        CHECK_EQ(ctx.ability_levels[0][2], 1, "T7060.c 正槽 +1 不受反转影响");
        std::cout << "  [T7060] 819 阶段末降转升 通过\n";
    }

    // ══ T7061 回合门生效：>300 免疫 + 全额反弹（row517 [300,1]）══
    {
        ScenarioBattle b(make_party(mark_pet({300, 1}, /*atk=*/1)),
                         make_party(foe_pet(/*atk=*/1500)));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)stat_change(&ctx, 0, 0, +2);   // 带正等级进回合 → ROUND_START 置门旗
        const auto [self_loss, opp_loss] = foe_strike(b);
        CHECK_EQ(self_loss, 0, "T7061.a 伤>300 被免疫（印主 hp 不动）");
        CHECK(opp_loss > 420 && opp_loss <= 520,
              "T7061.b 反弹=免疫前来伤全额（对手掉血 " + std::to_string(opp_loss) +
                  " 应 ∈ (420,520]）");
        std::cout << "  [T7061] 819 阈值门免疫+反弹 通过（opp_loss=" << opp_loss << "）\n";
    }

    // ══ T7062 ≤阈值：不免不反（严格大于）══
    {
        ScenarioBattle b(make_party(mark_pet({300, 1}, /*atk=*/1)),
                         make_party(foe_pet(/*atk=*/700)));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)stat_change(&ctx, 0, 0, +2);
        const auto [self_loss, opp_loss] = foe_strike(b);
        CHECK(self_loss > 150 && self_loss <= 260,
              "T7062.a 伤≤300 照落（印主掉血 " + std::to_string(self_loss) +
                  " 应 ∈ (150,260]）");
        CHECK_EQ(opp_loss, 0, "T7062.b 未触发门 → 无反弹");
        std::cout << "  [T7062] 819 ≤阈值不触发 通过\n";
    }

    // ══ T7063 行间阈值区分：伤带 (250,300] —— row939 免疫 / row517 不免 ══
    {
        int r939_self = 0, r939_opp = 0, r517_self = 0;
        {
            ScenarioBattle b(make_party(mark_pet({250, 1}, /*atk=*/1)),   // 伙伴进化行
                             make_party(foe_pet(/*atk=*/880)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 0, 0, +2);
            const auto [s, o] = foe_strike(b);
            r939_self = s;
            r939_opp = o;
        }
        {
            ScenarioBattle b(make_party(mark_pet({300, 1}, /*atk=*/1)),   // 基础行
                             make_party(foe_pet(/*atk=*/880)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 0, 0, +2);
            const auto [s, o] = foe_strike(b);
            r517_self = s;
        }
        CHECK_EQ(r939_self, 0, "T7063.a row939 [250,1]：伤 (250,300] 被免疫");
        CHECK(r939_opp > 250 && r939_opp <= 300,
              "T7063.b row939 反弹全额（对手掉血 " + std::to_string(r939_opp) + "）");
        CHECK(r517_self > 250 && r517_self <= 300,
              "T7063.c row517 [300,1]：同伤害带不触发（伤 ≤300 严格大于口径）");
        std::cout << "  [T7063] 819 双行阈值区分（250 vs 300）通过\n";
    }

    // ══ T7064 无正等级：门不开（大伤照落、无反弹）══
    {
        ScenarioBattle b(make_party(mark_pet({300, 1}, /*atk=*/1)),
                         make_party(foe_pet(/*atk=*/1500)));
        b.start();
        // 不给任何能力等级 → ROUND_START 旗=0
        const auto [self_loss, opp_loss] = foe_strike(b);
        CHECK(self_loss > 420,
              "T7064.a 无正等级不设门（印主掉血 " + std::to_string(self_loss) + " > 420）");
        CHECK_EQ(opp_loss, 0, "T7064.b 门不开 → 无反弹");
        std::cout << "  [T7064] 819 无提升不设门 通过\n";
    }

    std::cout << "scenario_7060 PASS (K361 降转升+阈值反弹族 819)" << std::endl;
    return 0;
}
