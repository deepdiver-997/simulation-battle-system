// 场景 850 —— H7 概率异常族（99 / 451，moves_lib/h_chance_anomaly.cpp）
//（2026-09-30，worktree ../agents/h7；号段 850-859 一文件覆盖两效果、四载体）
//
// 断言口径（概率类，F9 782 / 850 梦生归元先例同款）：
//   - chance=100 → 无随机精确断言：混乱魔光 [100] 必施加混乱；煞星玄黄令 [100,1]
//     必施加且恰为烧伤/冻伤/中毒之一；
//   - 种类均匀性：chance=100 的 451 独立 30 场 → 三种各出现 ≥1（全缺某一种的概率
//     ≈ 3×(2/3)^30 ≈ 5e-6，安全）；
//   - 中间档统计带（多场独立对局，每场恰一次施加机会、无异常残留互斥）：
//     99 [50] 暗影波 45 场 → 落地 ∈ [12,33]（B(45,0.5) 期望 22.5，双侧越界 P<1.5%）；
//     451 [30,1] 天之环 30 场 → 落地 ∈ [2,21]（B(30,0.3) 期望 9，双侧 P<0.3%）。
//   - 技能置 accuracy=100 消 miss 噪声（⚠️ 不置 must_hit——必中凭证绕过命中判定块，
//     H5 记档；本族效果在 SKILL_EFFECT 桶，miss 则不施加，accuracy=100 保证命中）。
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

constexpr int kHunLuanMG = 20845;  // 混乱魔光：属性技 [99] → [100]
constexpr int kAnYingBo  = 15961;  // 暗影波：特攻 [99] → [50]
constexpr int kShaXing   = 17682;  // 煞星玄黄令：物攻 [451] → [100,1]
constexpr int kTianZhiHuan = 13553; // 天之环：特攻 [451] → [30,1]
constexpr int kPaDa      = 10004;

constexpr int kConfusion = static_cast<int>(AbnormalStatusId::Confusion);  // 10
constexpr int kBurn      = static_cast<int>(AbnormalStatusId::Burn);       // 2
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5
constexpr int kPoison    = static_cast<int>(AbnormalStatusId::Poison);     // 1
const int kTriple[3] = {kBurn, kFrostbite, kPoison};

void accuracy_100(ElfPet& p, bool force_must_hit) {
    for (Skills& s : p.skills) {
        if (force_must_hit) {
            s.must_hit = 1;
        }
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*force_must_hit=*/true);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*force_must_hit=*/false);
    return p;
}

void play_once(std::array<int, 5> skills, int slot) {
    ScenarioBattle b(make_party(self_pet(skills)), make_party(foe_pet()));
    b.start();
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // T1 99 [100] 混乱魔光：必施加混乱（无随机精确断言）
    {
        ScenarioBattle b(make_party(self_pet({kHunLuanMG, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kConfusion), "T1 核心：100% 必施加混乱");
        std::cout << "  [T1] 99 [100] 必施加通过\n";
    }

    // T2 99 [50] 暗影波：45 场独立对局统计带（落地 ∈ [12,33]，期望 22.5）
    {
        int hits = 0;
        for (int i = 0; i < 45; ++i) {
            ScenarioBattle b(make_party(self_pet({kAnYingBo, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            if (ctx.has_active_abnormal_status(1, kConfusion)) {
                ++hits;
            }
        }
        CHECK(hits >= 12 && hits <= 33, "T2 统计带：45 场 50% 落地次数应在 [12,33]");
        std::cout << "  [T2] 99 [50] 统计带通过（落地 " << hits << "/45）\n";
    }

    // T3 451 [100,1] 煞星玄黄令：必施加且恰为三选一（激活数 == 1）
    {
        ScenarioBattle b(make_party(self_pet({kShaXing, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        int active = 0;
        int which = -1;
        for (const int id : kTriple) {
            if (ctx.has_active_abnormal_status(1, id)) {
                ++active;
                which = id;
            }
        }
        CHECK_EQ(active, 1, "T3 核心：必施加且恰一种（{1}=1）");
        CHECK(which == kBurn || which == kFrostbite || which == kPoison,
              "T3 核心：施加的必属烧伤/冻伤/中毒池");
        std::cout << "  [T3] 451 [100,1] 必施加一种通过\n";
    }

    // T4 451 种类均匀性：30 场 chance=100 → 三种各出现 ≥1
    {
        int burn = 0, frost = 0, poison = 0;
        for (int i = 0; i < 30; ++i) {
            ScenarioBattle b(make_party(self_pet({kShaXing, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            if (ctx.has_active_abnormal_status(1, kBurn)) {
                ++burn;
            } else if (ctx.has_active_abnormal_status(1, kFrostbite)) {
                ++frost;
            } else if (ctx.has_active_abnormal_status(1, kPoison)) {
                ++poison;
            }
        }
        CHECK(burn >= 1 && frost >= 1 && poison >= 1,
              "T4 均匀性：30 场三种各至少 1 次（全缺某一种 P≈5e-6）");
        std::cout << "  [T4] 451 种类分布通过（烧 " << burn << " 冻 " << frost << " 毒 "
                  << poison << " /30）\n";
    }

    // T5 451 [30,1] 天之环：30 场统计带（落地 ∈ [2,21]，期望 9）
    {
        int hits = 0;
        for (int i = 0; i < 30; ++i) {
            ScenarioBattle b(make_party(self_pet({kTianZhiHuan, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            if (ctx.has_active_abnormal_status(1, kBurn)
                || ctx.has_active_abnormal_status(1, kFrostbite)
                || ctx.has_active_abnormal_status(1, kPoison)) {
                ++hits;
            }
        }
        CHECK(hits >= 2 && hits <= 21, "T5 统计带：30 场 30% 落地次数应在 [2,21]");
        std::cout << "  [T5] 451 [30,1] 统计带通过（落地 " << hits << "/30）\n";
    }

    std::cout << "scenario_850_chance_anomaly all passed" << std::endl;
    return 0;
}
