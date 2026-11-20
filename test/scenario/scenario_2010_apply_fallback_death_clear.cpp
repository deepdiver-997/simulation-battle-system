// 场景 2010 —— J107 申报制未触发/被击败清强化族（2 宏，moves_lib/j_apply_fallback_death_clear.cpp）
//（2026-10-03，worktree ../agents/j107；号段 2010-2019 → T2010-T2011 两块）
//
// 断言口径：
//   - 1003 兜底 = 对手 R1 预开 [48]|[4] 免疫 → 恒温熔解 [80,2,1] 掷必败 → 六槽各 -1
//     （确定性）；命中路径 = 统计侧证：20 局极浪奇天咒 [80,15,1]（无免疫）命中
//     （码 15 乱信落地）频次 ≥ 8（二项 p=0.8，P(X≤7)<1e-5，确定性充分）；
//   - 197 = 死切驱动（953 协议）：自身 +2 后 hp=1 被击杀 → 死切停点断对手强化已清；
//     负例 = 未被击败 → 对手强化保留；
//   - 载体：恒温熔解/极浪奇天咒 [1003] 纯链特攻技；眩光之阵 26115 [197]|[2] 属性技
//     纯链（尾参 [2] 参差未绑定记档）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打
constexpr int kHengWen = 37736;  // 恒温熔解[1003]|[80,2,1]：特140（80%烧伤·兜底-1）
constexpr int kJiLang  = 30354;  // 极浪奇天咒[1003]|[80,15,1]：特150（统计侧证用）
constexpr int kXuanGuang = 26115; // 眩光之阵[197]|[2]：属性技（被击败→对手强化消失）
constexpr int kNingShen = 17091; // 凝神聚力斩[48]|[4]：物150（对手用：4回合免疫异常）

constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int hp = kMaxHp) {
    ElfPet p = make_pet(1, "己方", skills, speed, 1, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 200, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

bool levels_all(const BattleContext& ctx, int side, int v) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] != v) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2010 1003 兜底确定性 + 命中统计侧证 ══
    {
        // a) 兜底确定性：对手先手开免疫（自身慢速）→ 自身掷必败 → 六槽各 -1
        {
            ScenarioBattle b(make_party(self_pet({kHengWen, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/100)),
                             make_party(foe_pet({kNingShen, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/200, /*speed=*/300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 掷必败
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手开免疫
            b.drive();
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                std::cerr << "[j107-dbg] T2010.a R1end foe slot" << i << "="
                          << ctx.ability_levels[1][i] << "\n";
            }
            CHECK(levels_all(ctx, 1, -1),
                  "T2010.a 1003：免疫掷必败 → 兜底六槽各 -1");
        }
        // b) 命中统计侧证：20 局（无免疫）命中（码 15）频次 ≥ 8（p=0.8）
        {
            int landed = 0;
            constexpr int kTrials = 20;
            for (int t = 0; t < kTrials; ++t) {
                ScenarioBattle b(
                    make_party(self_pet({kJiLang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                    make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
                b.start();
                BattleContext& ctx = b.ctx();
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                for (int id = 0; id <= 43; ++id) {
                    if (id == 15 && ctx.has_active_abnormal_status(1, id)) {
                        ++landed;
                    }
                }
            }
            CHECK(landed >= 8,
                  "T2010.b 1003：命中频次 " + std::to_string(landed) + "/20（p=0.8 期望 16，"
                  "下界 8 的误判率 <1e-5）");
        }
        std::cout << "  [T2010] 1003 申报制兜底/命中统计 通过\n";
    }

    // ══ T2011 197 被击败 → 对手强化消失（眩光之阵）══
    {
        // a) 正例：自身 +2、被击杀（hp=1）→ 死切停点断对手强化已清
        {
            ElfPet me = self_pet({kXuanGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/300, /*hp=*/1);
            ElfPet mate = self_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            std::array<ElfPet, 6> mine{me, mate, mate, mate, mate, mate};
            ScenarioBattle b(std::move(mine),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK(stat_change(&ctx, 1, 0, 2) == StatChangeResult::SUCCESS,
                  "T2011.a 前置：对手 +2 攻");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 用技（武装）
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手击杀
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.currentState),
                     static_cast<int>(State::CHOOSE_AFTER_DEATH), "T2011.a 前置：停在死切");
            CHECK(levels_all(ctx, 1, 0),
                  "T2011.a 197：被击败 → 对手强化清零（六槽 0）");
        }
        // b) 负例：未被击败 → 对手强化保留
        {
            ScenarioBattle b(make_party(self_pet({kXuanGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK(stat_change(&ctx, 1, 0, 2) == StatChangeResult::SUCCESS,
                  "T2011.b 前置：对手 +2 攻");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp > 0, "T2011.b 前置：对手存活");
            CHECK_EQ(ctx.ability_levels[1][0], 2,
                  "T2011.b 197 门：未被击败对手强化保留（实测 " +
                      std::to_string(ctx.ability_levels[1][0]) + "）");
        }
        std::cout << "  [T2011] 197 被击败清强化 通过\n";
    }

    std::cout << "scenario_2010 PASS (J107 申报制未触发/被击败清强化族 1003/197)" << std::endl;
    return 0;
}
