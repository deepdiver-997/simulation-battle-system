// 场景 1250 —— J40 旧版输出窗族（1589/1522/708，moves_lib/j_legacy_output_windows.cpp）
//（2026-10-02，worktree ../agents/j40；号段 1250-1259 → T1250-T1252 三块）
//
// 断言口径：
//   - 1589 PP 精确等式（中毒门两半：异常→4 槽全 0 / 净手→PP 不动）；
//   - 1522 双带：atk=250 红 ~338>300 阈 → 掉 ~338+440；atk=1 红 ~2<300 → 掉 ~222
//     （红+固 220）——两带不交（foe hp 99999 免死扰）；
//   - 708 恢复精确等式：高体 +250 / 低体 +500（缺口开足防钳满）。
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
constexpr int kPaiDa   = 10004;   // 拍打：物40（对手填充刀）
constexpr int kShiJie  = 36945;   // 噬界荒灵咒 [1589]|[4] 特240（1589 随机 4 槽）
constexpr int kFeiCui   = 36778;  // 翡翠守护 [1522[200,200]] 物140（1522 纯链载体）
//（2026-10-02 合并修正：原载体 37503 链=[541,1522,54,619]，其 541/54 等兄弟效果
//  陆续被并行任务实现后链被激活，且 1522 阈值 300 贴红伤浮动带——换纯链 36778，
//  阈值 200 与浮动带(≥282)彻底分离。）
constexpr int kQieYi   = 27459;   // 惬意时光 [708]|[250,2] 属（708 回 250 低体翻倍）

constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);   // 1

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "旧版输出", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1250 1589 异常门随机 PP 归零（36945 [4]）══
    {
        {   // (a) 对手中毒 → 4 槽全归零
            ScenarioBattle b(make_party(self_pet({kShiJie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(try_apply_anomaly(&ctx, 1, kPoison, /*duration=*/3, /*actor=*/0));
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            int zeroed = 0;
            for (int i = 0; i < 5; ++i) {
                if (ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp == 0) {
                    ++zeroed;
                }
            }
            CHECK_EQ(zeroed, 4, "T1250.a 1589：随机 4/5 槽 PP 归零（恰 4 槽为 0）");
        }
        {   // (b) 门对照：对手净手 → PP 不动
            ScenarioBattle b(make_party(self_pet({kShiJie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int pp_before = ctx.getPet(1).skills[0].pp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).skills[0].pp, pp_before,
                     "T1250.b 1589 门：无异常 PP 不动");
        }
        std::cout << "  [T1250] 1589 异常门随机 PP 归零 通过\n";
    }

    // ══ T1251 1522 固伤翻倍（36778 翡翠守护 [200,200]，纯链）══
    {
        {   // (a) atk=250：红 ~280-330 > 200 → 固伤 400（掉 ~680-730）
            ScenarioBattle b(make_party(self_pet({kFeiCui, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/250)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99999 - ctx.getPet(1).hp;
            CHECK(loss >= 640 && loss <= 780,
                  "T1251.a 1522：红>200 → 固伤翻倍 400（实测掉 " + std::to_string(loss) + "）");
        }
        {   // (b) atk=1：红 ~2 < 200 → 固伤 200（掉 ~202）
            ScenarioBattle b(make_party(self_pet({kFeiCui, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99999 - ctx.getPet(1).hp;
            CHECK(loss >= 200 && loss <= 206,
                  "T1251.b 1522：红<200 → 固伤 200（实测掉 " + std::to_string(loss) + "）");
        }
        std::cout << "  [T1251] 1522 固伤翻倍双带 通过\n";
    }

    // ══ T1252 708 低体翻倍（27459 [250,2]）══
    {
        {   // (a) 高体（4800 > 2500）→ 回 250
            ScenarioBattle b(make_party(self_pet({kQieYi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, 4, +2) == StatChangeResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 削 ~1010（留缺口，仍高体）
            b.drive();
            const int hp_high = ctx.getPet(0).hp;
            CHECK(hp_high > 2500 && hp_high < 4500,
                  "T1252.a 前置：高体（>2500）且缺口>250");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 惬意时光
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp - hp_high, 250, "T1252.a 708：高体回 250");
        }
        {   // (b) 低体（先削 1010 → 3990... 需 <2500：两次削）→ 回 500
            ScenarioBattle b(make_party(self_pet({kQieYi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int r = 0; r < 3; ++r) {   // 三刀 ~3000，压到低体（<2500）
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
            }
            const int hp_low = ctx.getPet(0).hp;
            CHECK(hp_low < 2500 && hp_low > 500, "T1252.b 前置：低体（<max/2）且缺口>500");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp - hp_low, 500, "T1252.b 708：低体回 500（翻倍）");
        }
        std::cout << "  [T1252] 708 低体翻倍 通过\n";
    }

    std::cout << "scenario_1250 PASS (J40 旧版输出窗族)" << std::endl;
    return 0;
}
