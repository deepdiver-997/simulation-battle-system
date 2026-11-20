#include <utils/sbs_rand.h>
// 场景 7733 —— K413 effect 748 龙胆·赵云（3078，row426 本体 args[1,2,1,1] /
// row460 伙伴 args[2,3,1,1]，kind 同 [7,9,12]，db[0]/db[1] 档位差）
// 「每次登场首回合免疫异常状态；回合开始时自身处于能力提升则所有技能先制+1（②划界
//  不实现——板行未处方）；每回合随机吸取对手2项属性-1，并将该属性附加给自己（boss
//  无效）」（伙伴行：先制+2 / 吸取 3 项）—— soul_lib/k413_immune_steal_seal.cpp
//
// 断言口径：
//   - ①登场免疫窗 = R0 IMMUNE / R1 落地（窗 [登场回合]，2087 每次登场重授形）+
//     重登场重授（切走切回 → 窗内再免——"每次"非"首次"）；
//   - ③吸取转嫁 = **无随机结构断言**（随机目标类、无概率申报——K267 口径无审计槽，
//     用槽集对应关系精确断言）：R0（start 内）与 R1 各偷 2 个互不相同槽（K267 645
//     去重取法全 6 槽可达），逐槽 (对手[i]<0) ⟺ (自身[i]>0) 成对、计数与总和精确；
//     档位差 = 伙伴行 db[1]=3 → 每回合 3 对（纯 args 行≠宏）；
//   - 吸取失败不转嫁 = 对手全槽 -6（AT_FLOOR）→ 自身零变化、落地计数不增（对称
//     读法 majority 记档）；
//   - boss 门 = （boss无效）紧贴③ → steal 节点不执行（等级零变化），①无尾注 →
//     boss 局照常免疫（K408 1668 仅尾支口径）；
//   - 全部 submit_operation + drive() 有限回合，绝不 while(!FINISHED)。
#include <algorithm>
#include <any>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMaxHp = 99999;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2

// 侧证键（与 k413_immune_steal_seal.cpp 一致；owner=0）。
constexpr int kStealCnt = 997480000;   // 吸取转嫁落地槽次累计

const std::vector<int> kRowBase{1, 2, 1, 1};     // row426 本体（吸 2 项）
const std::vector<int> kRowPartner{2, 3, 1, 1};  // row460 伙伴（吸 3 项）

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(748, "K413 龙胆·赵云", "test", EffectArgs(args));
    sm.monster_id = 3078;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "龙胆·赵云", {10004, 10004, 10004, 10004, 10004},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {10004, 10004, 10004, 10004, 10004},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

int sum_levels(const BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

// 成对结构：对手被降槽集 == 自身被升槽集（吸取-1 转嫁 +1 同槽——无随机精确断言）。
int pair_slots(const BattleContext& ctx) {
    int pairs = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[1][i] < 0) {
            CHECK(ctx.ability_levels[0][i] > 0,
                  "槽 " + std::to_string(i) + " 对手降自身必升（转嫁同槽）");
            ++pairs;
        }
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[0][i] > 0) {
            CHECK(ctx.ability_levels[1][i] < 0,
                  "槽 " + std::to_string(i) + " 自身升必源于对手降");
        }
    }
    return pairs;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7733a ①登场免疫窗：R0 IMMUNE / R1 落地（窗=登场首回合）═════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        auto& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, /*actor=*/1, /*chance_pct=*/100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7733a 登场首回合免疫");
        CHECK(!ctx.has_active_abnormal_status(0, kBurn), "T7733a R0 无烧伤");
        idle_round(b);
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, 1, 100), ApplyAnomalyResult::SUCCESS,
                 "T7733a R1 窗过落地（首回合窗）");
        CHECK(ctx.has_active_abnormal_status(0, kBurn), "T7733a R1 烧伤在身");
        std::cout << "  [T7733a] 登场免疫窗 通过\n";
    }

    // ══ T7733b ③吸取转嫁：R0/R1 各 2 对、槽集对应、总和/计数精确 ════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.start();   // 停泊点：RS 未跑（键未写、零偷取）
        auto& ctx = b.ctx();
        CHECK_EQ(pair_slots(ctx), 0, "T7733b 停泊点零偷取（RS 未跑）");
        idle_round(b);   // R0 RS：对手全 0 → 全槽可降 → 偷 2 槽
        CHECK_EQ(pair_slots(ctx), 2, "T7733b R0 偷 2 槽（互不相同，成对）");
        CHECK_EQ(sum_levels(ctx, 0), 2, "T7733b R0 自身 +1×2");
        CHECK_EQ(sum_levels(ctx, 1), -2, "T7733b R0 对手 -1×2");
        CHECK_EQ(read_key(b, kStealCnt), 2, "T7733b R0 落地槽次=2");
        idle_round(b);   // R1 RS：再偷 2 槽（去重=回合内相异；跨回合可重叠→成对槽 [2,4]）
        const int pairs_r1 = pair_slots(ctx);
        CHECK(pairs_r1 >= 2 && pairs_r1 <= 4, "T7733b R1 成对槽 [2,4]（跨回合可重叠）");
        CHECK_EQ(sum_levels(ctx, 0), 4, "T7733b R1 自身累计 +4（逐次落地）");
        CHECK_EQ(sum_levels(ctx, 1), -4, "T7733b R1 对手累计 -4");
        CHECK_EQ(read_key(b, kStealCnt), 4, "T7733b R1 落地槽次累计=4");
        std::cout << "  [T7733b] 吸取转嫁 2 项 通过\n";
    }

    // ══ T7733c 吸取失败不转嫁：对手全槽 -6（AT_FLOOR）→ 自身零变化 ══════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        auto& ctx = b.ctx();
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            ctx.ability_levels[1][i] = -6;   // 全槽到底：无可吸取（首回合起）
        }
        idle_round(b);   // R0：全槽 AT_FLOOR → 无可吸取
        CHECK_EQ(sum_levels(ctx, 0), 0, "T7733c AT_FLOOR 支自身零变化（不转嫁）");
        CHECK_EQ(sum_levels(ctx, 1), -36, "T7733c 对手全槽保持 -6");
        CHECK_EQ(read_key(b, kStealCnt), -1, "T7733c 落地槽次键未写（吸取全失败）");
        std::cout << "  [T7733c] 吸取失败不转嫁 通过\n";
    }

    // ══ T7733d boss 门：③不执行（等级零变化）/ ①照常免疫 ═══════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        auto& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, 1, 100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7733d boss：①登场免疫照常");
        idle_round(b);
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, 1, 100), ApplyAnomalyResult::SUCCESS,
                 "T7733d boss：①窗过落地（1 回合窗语义不变）");
        CHECK_EQ(sum_levels(ctx, 0), 0, "T7733d boss：无吸取（自身零变化）");
        CHECK_EQ(sum_levels(ctx, 1), 0, "T7733d boss：无吸取（对手零变化）");
        CHECK_EQ(read_key(b, kStealCnt), -1, "T7733d boss：无落地槽次（键未写）");
        std::cout << "  [T7733d] boss 门（③静默①照常） 通过\n";
    }

    // ══ T7733e 伙伴行档位差：db[1]=3 → 每回合 3 对（db[0]=2 先制位划界不消费）══
    {
        ScenarioBattle b(make_party(mark_pet(kRowPartner)), make_party(foe_pet()));
        b.start();
        auto& ctx = b.ctx();
        idle_round(b);   // R0 RS：偷 3 槽
        CHECK_EQ(pair_slots(ctx), 3, "T7733e 伙伴 R0 偷 3 槽（档位差位）");
        CHECK_EQ(sum_levels(ctx, 0), 3, "T7733e 伙伴 R0 自身 +3");
        CHECK_EQ(sum_levels(ctx, 1), -3, "T7733e 伙伴 R0 对手 -3");
        CHECK_EQ(read_key(b, kStealCnt), 3, "T7733e 伙伴落地槽次=3");
        idle_round(b);
        const int pairs_r1 = pair_slots(ctx);
        CHECK(pairs_r1 >= 3 && pairs_r1 <= 6, "T7733e 伙伴 R1 成对槽 [3,6]");
        CHECK_EQ(sum_levels(ctx, 0), 6, "T7733e 伙伴 R1 自身累计 +6");
        std::cout << "  [T7733e] 伙伴行 3 项档位差 通过\n";
    }

    // ══ T7733f 重登场重授：切走切回 → 窗内再免（"每次登场"非"首次"）═════════
    {
        std::array<ElfPet, 6> mine{mark_pet(kRowBase), foe_pet(), foe_pet(),
                                   foe_pet(), foe_pet(), foe_pet()};
        ScenarioBattle b(mine, make_party(foe_pet()));
        b.start();
        CHECK_EQ(apply_anomaly(&b.ctx(), 0, kBurn, 2, 1, 100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7733f 初登场免疫");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 切走
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);   // 切回（重登场）
        CHECK_EQ(apply_anomaly(&b.ctx(), 0, kBurn, 2, 1, 100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7733f 重登场再免（每次）");
        idle_round(b);
        CHECK_EQ(apply_anomaly(&b.ctx(), 0, kBurn, 2, 1, 100),
                 ApplyAnomalyResult::SUCCESS, "T7733f 重登场窗 1 回合后过");
        std::cout << "  [T7733f] 重登场重授 通过\n";
    }

    std::cout << "scenario_7733: all green" << std::endl;
    return 0;
}
