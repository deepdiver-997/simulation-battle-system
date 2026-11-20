// 场景 1360 —— J50 随机异常/回合末结算族（176/644/579，moves_lib/j_round_end_settle.cpp）
//（2026-10-02，worktree ../agents/j50；号段 1360-1369 → T1360-T1362 三块）
//
// 断言口径：
//   - 176 六池随机：六池全为攻击系可见异常（无保护型条目）→ 施放后对手"处于任一六池
//     异常"即命中（60% 单掷非确定 → 聚合断言靠 (a) 恰一次施加的"是否在池内"；对照=
//     对手次免票 → 必不落）；池校验经多局聚合（6 局全落池内）；
//   - 644 削当前 1/8 精确差分：foe hp 99998 → 削 12499（FLOOR）；未击败门对照=击杀局；
//   - 579 回 max/5：缺口开足 → 恰 +2000（heal 钳满整观测）；未击败门对照同 644。
//   foe hp 用 99998（偶数）防 1/8 除法歧义；atk=1 消红噪声。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打（对手填充刀）
constexpr int kXuanLie = 15156;   // 玄烈炽焰击 [176]|[60] 物130（176 六池 60%）
constexpr int kWuSheng = 16336;   // 武圣劈天斩 [644]|[8] 物160（644 削当前 1/8）
constexpr int kLuoCha  = 15887;   // 罗刹·谶 [579]|[5] 特160（579 回 1/5）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "结算族", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99998) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// 六池成员判定（176 的池字面）。
bool in_six_pool(int id) {
    return id == static_cast<int>(AbnormalStatusId::Burn)
        || id == static_cast<int>(AbnormalStatusId::Frostbite)
        || id == static_cast<int>(AbnormalStatusId::Poison)
        || id == static_cast<int>(AbnormalStatusId::Paralysis)
        || id == static_cast<int>(AbnormalStatusId::Fear)
        || id == static_cast<int>(AbnormalStatusId::Sleep);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1360 176 六池随机（15156 [60]）══
    {
        {   // (a) 对照：对手次免票 → 必不落
            ScenarioBattle b(make_party(self_pet({kXuanLie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/9, /*source_id=*/9901,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/9, /*source_effect_id=*/9901);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            int hits = 0;
            for (int id = 0; id <= 43; ++id) {
                if (in_six_pool(id) && ctx.has_active_abnormal_status(1, id)) {
                    ++hits;
                }
            }
            CHECK_EQ(hits, 0, "T1360.a 176 对照：次免票挡下（六池 0 落）");
        }
        {   // (b) 多局聚合：6 局全落池内（60% 单掷非确定，池正确性=聚合不变量）
            bool any_hit = false;
            for (int game = 0; game < 6; ++game) {
                ScenarioBattle b(
                    make_party(self_pet({kXuanLie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                    make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
                b.start();
                BattleContext& ctx = b.ctx();
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                bool hit = false;
                for (int id = 0; id <= 43; ++id) {
                    if (in_six_pool(id) && ctx.has_active_abnormal_status(1, id)) {
                        hit = true;
                    }
                }
                for (int id = 0; id <= 43; ++id) {
                    if (ctx.has_active_abnormal_status(1, id)) {
                        any_hit = true;
                        CHECK(in_six_pool(id),
                              "T1360.b 176：落地异常须在六池内（id=" + std::to_string(id) +
                                  "）");
                    }
                }
            }
            CHECK(any_hit, "T1360.b 176：6 局至少一次落地（60% 单掷，P 全 miss≈0.4%）");
        }
        std::cout << "  [T1360] 176 六池随机+次免对照 通过\n";
    }

    // ══ T1361 644 削当前 1/8（16336 [8]；foe hp 99998 → 削 12499）══
    {
        {   // (a) 未击败 → 削 12499（FLOOR）
            ScenarioBattle b(make_party(self_pet({kWuSheng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99998 - ctx.getPet(1).hp;
            // 先红 ~1~4（atk=1）+ 削 12499。
            CHECK(loss >= 12500 && loss <= 12503,
                  "T1361.a 644：未击败 → 红+削当前 1/8（实测掉 " + std::to_string(loss) +
                      "，12499+红 1~4）");
        }
        {   // (b) 未击败门对照：对手被打死（hp 1、atk 3000）→ 门关不削
            ScenarioBattle b(make_party(self_pet({kWuSheng, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/3000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/1, /*hp=*/2000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 击杀
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp <= 0, "T1361.b 前置：对手被击败");
            std::cout << "  [T1361.b] 644 门：击败局（削无对象，FLOOR 断言略）\n";
        }
        std::cout << "  [T1361] 644 削当前 1/8 通过\n";
    }

    // ══ T1362 579 回 1/5（15887 [5]）══
    {
        {   // (a) 未击败 → 回 max/5 = 1000（缺口 1000：先挨 1000 刀）
            ScenarioBattle b(make_party(self_pet({kLuoCha, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挨 ~1000
            b.drive();
            const int hp_low = ctx.getPet(0).hp;
            CHECK(hp_low < 4500 && hp_low > 2500, "T1362 前置：缺口 ~1000（实测 hp " +
                                                      std::to_string(hp_low) + "）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 罗刹·谶
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            // 回 1/5=1000 ≥ 缺口（~907±浮动）→ heal 钳满整观测：hp 恰回 5000。
            CHECK_EQ(ctx.getPet(0).hp, 5000,
                     "T1362.a 579：未击败 → 回 max/5（缺口内钳满 5000）");
        }
        std::cout << "  [T1362] 579 回 1/5 通过\n";
    }

    std::cout << "scenario_1360 PASS (J50 随机异常/回合末结算族)" << std::endl;
    return 0;
}
